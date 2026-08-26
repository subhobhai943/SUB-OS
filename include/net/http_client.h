#ifndef _NET_HTTP_CLIENT_H
#define _NET_HTTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Asynchronous HTTP/1.0 client.
//
// The TCP client calls (tcp_connect/tcp_send/tcp_recv) all busy-wait against
// the timer tick, so calling them from an interactive context -- the GUI
// compositor above all -- would freeze it for the length of the fetch. This
// module runs every fetch on one long-lived kernel worker thread instead: the
// requester posts a job and polls its state, and the compositor keeps painting
// at full rate while the worker blocks.
//
// Ownership follows the state. While a job is HTTP_STATE_RUNNING the worker
// owns it; once it is DONE or ERROR the requester owns it again. The requester
// hands a job back with http_fetch_release, which frees it now if the fetch has
// finished, or marks it abandoned so the worker frees it when it does -- so a
// window can close mid-fetch without leaking or use-after-free.

typedef enum {
    HTTP_STATE_RUNNING = 0,   // worker owns the job
    HTTP_STATE_DONE,          // completed; buf holds the response
    HTTP_STATE_ERROR          // failed; err holds a short reason
} http_fetch_state_t;

typedef struct http_fetch {
    // Request, filled in by http_fetch_start from the parsed URL.
    char     host[128];
    char     path[256];
    uint16_t port;
    bool     tls;             // the URL asked for https, which we cannot speak

    // Result. `state` is the only field the requester may read while the fetch
    // is in flight; everything else is valid once state != RUNNING.
    volatile int state;       // http_fetch_state_t
    volatile bool abandoned;  // requester released it mid-flight; worker frees

    char*    buf;             // response bytes (status line, headers, body)
    int      len;             // bytes stored
    int      cap;             // buffer capacity

    uint32_t ip;              // resolved peer address (of the final hop)
    int      status_code;     // parsed HTTP status, 0 if not found
    uint32_t elapsed_ms;      // wall time of the whole fetch
    int      redirects;       // number of 3xx hops followed
    char     err[96];         // human-readable failure reason
} http_fetch_t;

// Start the worker thread. Call once at boot, after the TCP stack is up.
void http_client_init(void);

// Begin fetching `url` (forms: "host", "host/path", "host:port/path", with an
// optional "http://" prefix). `cap` bounds the stored response. Returns a job
// to poll, or NULL if the URL is unusable or a fetch is already in flight.
http_fetch_t* http_fetch_start(const char* url, int cap);

// Give a job back. Frees it if the fetch has finished; otherwise marks it so
// the worker frees it on completion. The pointer must not be used afterwards.
void http_fetch_release(http_fetch_t* f);

// True while the worker thread is busy with a fetch.
bool http_client_busy(void);

#endif // _NET_HTTP_CLIENT_H
