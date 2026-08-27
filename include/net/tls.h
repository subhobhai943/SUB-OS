#ifndef _NET_TLS_H
#define _NET_TLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Minimal TLS 1.3 client (RFC 8446), enough to fetch an HTTPS page.
//
// Scope, stated plainly:
//   - TLS 1.3 only, one cipher suite (TLS_CHACHA20_POLY1305_SHA256) and one
//     key exchange (X25519). These are universally supported by modern servers.
//   - The server certificate is NOT verified. The handshake completes and the
//     channel is encrypted, but a man-in-the-middle could impersonate the peer.
//     Real certificate-chain validation (X.509 parsing, signature checks, a CA
//     bundle, hostname matching) is a large separate piece not built here.
//
// The calls mirror the TCP client and block the same cooperative way, so they
// run on the async HTTP worker thread, not the compositor.

typedef struct tls_conn tls_conn_t;

// Open a TLS connection to ip:port, using `host` for SNI. Returns NULL on any
// failure (connect, handshake, unsupported server parameters).
tls_conn_t* tls_connect(const char* host, uint32_t ip, uint16_t port,
                        uint32_t timeout_ms);

// Send application data (the HTTP request). Returns bytes sent or -1.
int tls_send(tls_conn_t* c, const void* data, int len);

// Receive up to len bytes of application data, waiting up to timeout_ms.
// Returns the byte count, 0 at clean end of stream, or -1 on error.
int tls_recv(tls_conn_t* c, void* buf, int len, uint32_t timeout_ms);

// Shut down and release the connection.
void tls_close(tls_conn_t* c);

// A short reason for the last failure, for diagnostics.
const char* tls_last_error(void);

#endif // _NET_TLS_H
