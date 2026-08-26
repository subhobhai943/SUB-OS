/*
 * Asynchronous HTTP/1.0 client for SUB-OS.
 *
 * One worker thread, created at boot, services a single-slot mailbox. A
 * requester parses a URL into a job, drops it in the mailbox, and polls the
 * job's state; the worker performs the blocking DNS + TCP fetch and marks the
 * job DONE or ERROR. Because the blocking happens on the worker rather than in
 * the caller, the GUI compositor keeps running at full frame rate during a
 * fetch that may take seconds.
 *
 * See http_client.h for the ownership rules; the two critical sections that
 * enforce them (worker completion and requester release) run with interrupts
 * disabled, which on this single-CPU kernel makes the hand-off indivisible.
 */
#include <net/http_client.h>
#include <net/tcp.h>
#include <net/dns.h>
#include <net/net.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>
#include <kernel/task.h>
#include <kernel/sched.h>
#include <arch/arch.h>

#define HTTP_CONNECT_TIMEOUT_MS 4000
#define HTTP_RECV_TIMEOUT_MS    3000

static task_t*                 worker_task = NULL;
static http_fetch_t* volatile  mailbox     = NULL;   // job awaiting the worker

// ===========================================================================
// URL parsing
//
// Accepts host, host/path, host:port/path, with an optional http:// prefix.
// Anything it cannot make sense of leaves the job unusable and is reported as
// an error rather than guessed at.
// ===========================================================================
static void parse_url(const char* url, http_fetch_t* f) {
    f->host[0] = '\0';
    strcpy(f->path, "/");
    f->port = 80;

    if (!url) return;
    if (strncmp(url, "http://", 7) == 0) url += 7;
    else if (strncmp(url, "https://", 8) == 0) url += 8;   // no TLS; try plain

    // Host (and optional :port) up to the first '/'.
    int i = 0;
    char hostport[192];
    while (url[i] && url[i] != '/' && i < (int)sizeof(hostport) - 1) {
        hostport[i] = url[i];
        i++;
    }
    hostport[i] = '\0';

    const char* rest = url + i;      // "" or "/path..."

    // Split host:port.
    char* colon = strchr(hostport, ':');
    if (colon) {
        *colon = '\0';
        int p = 0;
        for (const char* c = colon + 1; *c >= '0' && *c <= '9'; c++) p = p * 10 + (*c - '0');
        if (p > 0 && p < 65536) f->port = (uint16_t)p;
    }
    strncpy(f->host, hostport, sizeof(f->host) - 1);
    f->host[sizeof(f->host) - 1] = '\0';

    if (rest[0] == '/') {
        strncpy(f->path, rest, sizeof(f->path) - 1);
        f->path[sizeof(f->path) - 1] = '\0';
    }
}

// Parse the numeric status out of "HTTP/1.x NNN ...", or 0 if absent.
static int parse_status(const char* buf, int len) {
    if (len < 12 || strncmp(buf, "HTTP/", 5) != 0) return 0;
    const char* p = buf;
    const char* end = buf + len;
    while (p < end && *p != ' ') p++;        // skip "HTTP/1.x"
    while (p < end && *p == ' ') p++;
    int code = 0;
    while (p < end && *p >= '0' && *p <= '9') code = code * 10 + (*p++ - '0');
    return code;
}

// ===========================================================================
// The fetch itself, run on the worker thread.
//
// It fills in the result fields but deliberately does NOT touch f->state:
// publishing the terminal state is the finalize step's job, done under
// interrupts-off. If do_fetch flipped the state itself, the requester could
// observe DONE and free the job in the instant before finalize ran, and
// finalize would then read freed memory. Returning the state instead keeps the
// job private to the worker until the atomic hand-off.
// ===========================================================================
static int do_fetch(http_fetch_t* f) {
    uint64_t t0 = pit_get_ticks();

    // Resolve: a dotted quad is taken as-is, otherwise the DNS resolver runs
    // (itself bounded, so an unreachable server times out rather than hangs).
    f->ip = ip_parse(f->host);
    if (f->ip == 0) f->ip = dns_resolve(f->host);
    if (f->ip == 0) {
        snprintf(f->err, sizeof(f->err), "cannot resolve %s", f->host);
        return HTTP_STATE_ERROR;
    }

    tcp_conn_t* c = tcp_connect(f->ip, f->port, HTTP_CONNECT_TIMEOUT_MS);
    if (!c) {
        snprintf(f->err, sizeof(f->err), "connect to :%u refused/timed out", f->port);
        return HTTP_STATE_ERROR;
    }

    char req[512];
    int rl = snprintf(req, sizeof(req),
                      "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SUB-OS/webfetch\r\n"
                      "Connection: close\r\n\r\n", f->path, f->host);
    if (tcp_send(c, req, (uint16_t)rl) < 0) {
        snprintf(f->err, sizeof(f->err), "send failed");
        tcp_close(c);
        return HTTP_STATE_ERROR;
    }

    // Drain the response until the peer closes or the buffer fills.
    f->len = 0;
    while (f->len < f->cap - 1) {
        int n = tcp_recv(c, f->buf + f->len, (uint16_t)(f->cap - 1 - f->len),
                         HTTP_RECV_TIMEOUT_MS);
        if (n <= 0) break;
        f->len += n;
    }
    f->buf[f->len] = '\0';
    tcp_close(c);

    f->status_code = parse_status(f->buf, f->len);
    f->elapsed_ms  = (uint32_t)((pit_get_ticks() - t0) * 10);

    if (f->len == 0) {
        snprintf(f->err, sizeof(f->err), "no data received");
        return HTTP_STATE_ERROR;
    }
    return HTTP_STATE_DONE;
}

static void free_job(http_fetch_t* f) {
    if (!f) return;
    if (f->buf) kfree(f->buf);
    kfree(f);
}

// Publish the terminal state, or free the job if the requester walked away
// while the fetch was in flight. Interrupts are off across the decision so it
// serialises against http_fetch_release: exactly one of the two frees the job,
// and the requester never sees a non-RUNNING state for a job it has released.
static void finalize(http_fetch_t* f, int result) {
    unsigned long fl = arch_irq_save();
    bool abandoned = f->abandoned;
    if (!abandoned) f->state = result;
    arch_irq_restore(fl);

    if (abandoned) free_job(f);
}

static void http_worker_main(void* arg) {
    (void)arg;
    for (;;) {
        unsigned long fl = arch_irq_save();
        http_fetch_t* job = mailbox;
        mailbox = NULL;
        arch_irq_restore(fl);

        if (!job) {
            // Idle: yield rather than halt. Halting would hold the CPU until a
            // timer tick and starve the compositor between ticks; a cooperative
            // yield hands the CPU straight back and the two ping-pong cheaply.
            net_wait();
            continue;
        }

        int result = do_fetch(job);
        finalize(job, result);
    }
}

// ===========================================================================
// Public API
// ===========================================================================
void http_client_init(void) {
    mailbox = NULL;
    worker_task = task_create("http-worker", http_worker_main, NULL, 0);
    if (worker_task) {
        printk(KERN_INFO "NET: async HTTP client worker online\n");
    } else {
        printk(KERN_WARNING "NET: async HTTP client worker failed to start\n");
    }
}

bool http_client_busy(void) {
    return mailbox != NULL;
}

http_fetch_t* http_fetch_start(const char* url, int cap) {
    if (!worker_task || !url || cap < 64) return NULL;
    if (mailbox) return NULL;                  // a fetch is already queued

    http_fetch_t* f = (http_fetch_t*)kzalloc(sizeof(http_fetch_t));
    if (!f) return NULL;

    f->buf = (char*)kzalloc(cap);
    if (!f->buf) { kfree(f); return NULL; }
    f->cap   = cap;
    f->state = HTTP_STATE_RUNNING;

    parse_url(url, f);
    if (f->host[0] == '\0') {
        kfree(f->buf); kfree(f);
        return NULL;
    }

    // Hand the job to the worker. Re-check the mailbox under interrupts-off so
    // two near-simultaneous starts cannot both install a job.
    unsigned long fl = arch_irq_save();
    if (mailbox) { arch_irq_restore(fl); kfree(f->buf); kfree(f); return NULL; }
    mailbox = f;
    arch_irq_restore(fl);

    return f;
}

void http_fetch_release(http_fetch_t* f) {
    if (!f) return;

    unsigned long fl = arch_irq_save();
    if (f->state == HTTP_STATE_RUNNING) {
        // The worker still owns it (queued or in flight): let it free the job
        // when it finishes rather than pulling the memory out from under it.
        f->abandoned = true;
        arch_irq_restore(fl);
        return;
    }
    arch_irq_restore(fl);

    free_job(f);
}
