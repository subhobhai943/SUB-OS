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
// Parse an absolute URL into its parts. Returns false only when there is no
// host to be found. `is_https` reports the scheme so the caller can refuse a
// TLS target cleanly rather than speaking plaintext to port 443.
static bool split_url(const char* url, char* host, size_t host_cap,
                      char* path, size_t path_cap, uint16_t* port, bool* is_https) {
    host[0] = '\0';
    strcpy(path, "/");
    *port = 80;
    *is_https = false;

    if (!url) return false;
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        url += 8;
        *is_https = true;
        *port = 443;
    }

    // Host (and optional :port) up to the first '/'.
    char hostport[192];
    int i = 0;
    while (url[i] && url[i] != '/' && i < (int)sizeof(hostport) - 1) {
        hostport[i] = url[i];
        i++;
    }
    hostport[i] = '\0';
    const char* rest = url + i;      // "" or "/path..."

    char* colon = strchr(hostport, ':');
    if (colon) {
        *colon = '\0';
        int p = 0;
        for (const char* c = colon + 1; *c >= '0' && *c <= '9'; c++) p = p * 10 + (*c - '0');
        if (p > 0 && p < 65536) *port = (uint16_t)p;
    }
    strncpy(host, hostport, host_cap - 1);
    host[host_cap - 1] = '\0';

    if (rest[0] == '/') {
        strncpy(path, rest, path_cap - 1);
        path[path_cap - 1] = '\0';
    }
    return host[0] != '\0';
}

static void parse_url(const char* url, http_fetch_t* f) {
    split_url(url, f->host, sizeof(f->host), f->path, sizeof(f->path), &f->port, &f->tls);
}

// Find a header's value in the response, case-insensitively. Copies the value
// (trimmed, up to the CRLF) into out. Returns false if the header is absent.
static bool find_header(const char* buf, int len, const char* name,
                        char* out, size_t out_cap) {
    size_t nlen = strlen(name);
    // Scan line by line; the status line is skipped naturally (no match).
    for (int i = 0; i + (int)nlen < len; i++) {
        if (i != 0 && buf[i - 1] != '\n') continue;   // only at a line start

        // Case-insensitive compare of the header name.
        size_t k = 0;
        while (k < nlen) {
            char a = buf[i + k], b = name[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            k++;
        }
        if (k != nlen || buf[i + k] != ':') continue;

        const char* v = buf + i + nlen + 1;
        const char* end = buf + len;
        while (v < end && (*v == ' ' || *v == '\t')) v++;
        size_t o = 0;
        while (v < end && *v != '\r' && *v != '\n' && o < out_cap - 1) out[o++] = *v++;
        out[o] = '\0';
        return true;
    }
    return false;
}

// Resolve a Location value against the current request. Absolute URLs replace
// host/port/path; a path-only Location keeps the host. Returns false and sets
// f->err when the target is HTTPS, which this TLS-less client cannot follow.
static bool apply_redirect(http_fetch_t* f, const char* location) {
    if (location[0] == '/' || location[0] == '\0') {
        // Same host, new path.
        strncpy(f->path, location[0] ? location : "/", sizeof(f->path) - 1);
        f->path[sizeof(f->path) - 1] = '\0';
        return true;
    }

    if (strncmp(location, "http://", 7) != 0 && strncmp(location, "https://", 8) != 0) {
        // A bare relative reference; treat it as a rooted path.
        snprintf(f->path, sizeof(f->path), "/%s", location);
        return true;
    }

    bool https;
    char host[128], path[256];
    uint16_t port;
    if (!split_url(location, host, sizeof(host), path, sizeof(path), &port, &https)) {
        snprintf(f->err, sizeof(f->err), "redirect to a URL we cannot parse");
        return false;
    }
    if (https) {
        snprintf(f->err, sizeof(f->err), "redirect to HTTPS (TLS unsupported): %s", host);
        return false;
    }
    strcpy(f->host, host);
    strcpy(f->path, path);
    f->port = port;
    return true;
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
#define HTTP_MAX_REDIRECTS 5

// Perform one request/response against the current host/path/port. Returns the
// HTTP status code, or a negative error code; the raw response lands in f->buf.
static int do_one_request(http_fetch_t* f) {
    f->ip = ip_parse(f->host);
    if (f->ip == 0) f->ip = dns_resolve(f->host);
    if (f->ip == 0) {
        snprintf(f->err, sizeof(f->err), "cannot resolve %s", f->host);
        return -1;
    }

    tcp_conn_t* c = tcp_connect(f->ip, f->port, HTTP_CONNECT_TIMEOUT_MS);
    if (!c) {
        snprintf(f->err, sizeof(f->err), "connect to %s:%u refused/timed out",
                 f->host, f->port);
        return -1;
    }

    char req[512];
    int rl = snprintf(req, sizeof(req),
                      "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SUB-OS/webfetch\r\n"
                      "Connection: close\r\n\r\n", f->path, f->host);
    if (tcp_send(c, req, (uint16_t)rl) < 0) {
        snprintf(f->err, sizeof(f->err), "send failed");
        tcp_close(c);
        return -1;
    }

    f->len = 0;
    while (f->len < f->cap - 1) {
        int n = tcp_recv(c, f->buf + f->len, (uint16_t)(f->cap - 1 - f->len),
                         HTTP_RECV_TIMEOUT_MS);
        if (n <= 0) break;
        f->len += n;
    }
    f->buf[f->len] = '\0';
    tcp_close(c);

    if (f->len == 0) {
        snprintf(f->err, sizeof(f->err), "no data received");
        return -1;
    }
    return parse_status(f->buf, f->len);
}

// Fills in the result fields but deliberately does NOT touch f->state:
// publishing the terminal state is finalize's job, done under interrupts-off.
// Returning the state keeps the job private to the worker until that hand-off.
//
// A 3xx response with a Location header is followed, up to a bounded number of
// hops, so a real-world URL that redirects (a bare host to a path, http to a
// canonical host) resolves to actual content. An https target is reported
// rather than followed, since this client has no TLS.
static int do_fetch(http_fetch_t* f) {
    uint64_t t0 = pit_get_ticks();
    f->redirects = 0;

    // The client has no TLS, so an https URL cannot be served. Say so plainly
    // rather than sending a plaintext request to port 443 and reporting a
    // confusing timeout.
    if (f->tls) {
        snprintf(f->err, sizeof(f->err), "%s is HTTPS; this client has no TLS", f->host);
        return HTTP_STATE_ERROR;
    }

    for (;;) {
        int status = do_one_request(f);
        if (status < 0) return HTTP_STATE_ERROR;   // f->err already set

        bool is_redirect = (status == 301 || status == 302 || status == 303 ||
                            status == 307 || status == 308);
        char location[512];
        if (is_redirect && f->redirects < HTTP_MAX_REDIRECTS &&
            find_header(f->buf, f->len, "Location", location, sizeof(location))) {
            if (!apply_redirect(f, location)) return HTTP_STATE_ERROR;  // e.g. HTTPS
            f->redirects++;
            continue;
        }

        f->status_code = status;
        f->elapsed_ms  = (uint32_t)((pit_get_ticks() - t0) * 10);
        return HTTP_STATE_DONE;
    }
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
