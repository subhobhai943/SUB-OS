/*
 * TCP for SUB-OS.
 *
 * The engine drives both ends of a connection:
 *
 *   Passive open  -- an inbound SYN to a listening service (sshd, httpd)
 *                    creates a server connection, which is answered straight
 *                    out of the receive path.
 *   Active open   -- tcp_connect() performs a real three-way handshake, after
 *                    which tcp_send/tcp_recv carry a byte stream and
 *                    tcp_close() shuts it down in order.
 *
 * Inbound segments are matched against the connection table first and only
 * then offered to the listeners, which is what lets a reply to one of our own
 * outbound connections through -- previously anything that was not addressed
 * to a listening port was answered with RST, so the SYN-ACK completing our own
 * handshake would have been rejected.
 *
 * Transmission is stop-and-wait rather than a sliding window: one segment is
 * outstanding at a time and is retransmitted until acknowledged. That is
 * enough to be correct over a lossy path and keeps the retransmission state to
 * a single sequence number, at the cost of throughput on a long fat link.
 *
 * Every blocking call is bounded against the timer tick and halts the CPU
 * between checks, so a silent peer costs a timeout rather than a hang.
 */
#include <net/tcp.h>
#include <net/net.h>
#include <net/ssh.h>
#include <net/http.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>
#include <crypto/crypto.h>
#include <arch/arch.h>

#define MAX_TCP_CONNS 64

// The timer tick is 100 Hz, so one tick is 10 ms.
#define TCP_RTO_TICKS        50   // 500 ms retransmission timeout
#define TCP_MAX_RETRIES       5
#define TCP_CLOSE_TICKS      50   // how long tcp_close waits for the shutdown
#define TCP_TIME_WAIT_TICKS 200   // 2 s; a deliberately shortened 2*MSL

// A connection this end closed after serving one request (httpd/sshd) does not
// need the full linger: we sent the FIN, the peer is expected to be a
// short-lived client, and holding the slot for two seconds is what let a burst
// of requests exhaust the table. A brief linger still absorbs a retransmitted
// FIN while freeing the slot roughly ten times sooner.
#define TCP_SVC_TIME_WAIT_TICKS 25   // 250 ms

#define TCP_EPHEMERAL_LO 49152
#define TCP_EPHEMERAL_HI 65535

static tcp_conn_t tcp_connections[MAX_TCP_CONNS];

// A claimed port, plus the connections whose handshakes have completed on it
// and are waiting to be collected by tcp_accept.
typedef struct {
    uint16_t port;
    bool     in_use;
    int      backlog;
    tcp_conn_t* queue[TCP_BACKLOG_MAX];
    int      count;
} tcp_listener_t;

static tcp_listener_t tcp_listeners[TCP_MAX_LISTENERS];

// Cumulative connections that have reached ESTABLISHED, and the high-water mark
// of simultaneously-live table entries. Diagnostic only.
static uint64_t tcp_total_opened = 0;
static int      tcp_peak_conns   = 0;

static void tcp_note_peak(void) {
    int live = 0;
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) if (tcp_connections[i].in_use) live++;
    if (live > tcp_peak_conns) tcp_peak_conns = live;
}

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const tcp_header_t* tcp,
                             const void* payload, uint16_t payload_len) {
    uint16_t tcp_total_len = (uint16_t)(sizeof(tcp_header_t) + payload_len);

    // The pseudo-header, laid out explicitly rather than as a packed struct:
    // src and dst already hold network order, so their bytes are summed as-is.
    uint8_t tail[4] = {
        0, IP_PROTO_TCP,
        (uint8_t)(tcp_total_len >> 8), (uint8_t)(tcp_total_len & 0xFF)
    };

    uint32_t sum = 0;
    sum = net_csum_add(sum, &src_ip, sizeof(src_ip));
    sum = net_csum_add(sum, &dst_ip, sizeof(dst_ip));
    sum = net_csum_add(sum, tail, sizeof(tail));
    sum = net_csum_add(sum, tcp, sizeof(tcp_header_t));
    if (payload && payload_len > 0) sum = net_csum_add(sum, payload, payload_len);

    return net_csum_finish(sum);
}

void tcp_init(void) {
    memset(tcp_connections, 0, sizeof(tcp_connections));
    memset(tcp_listeners, 0, sizeof(tcp_listeners));
    printk(KERN_INFO "NET: TCP state machine online (active + passive open)\n");
}

// ===========================================================================
// Sequence arithmetic
//
// Sequence numbers wrap at 2^32, so they are compared by the sign of their
// difference rather than by magnitude.
// ===========================================================================
static inline bool seq_ge(uint32_t a, uint32_t b) { return (int32_t)(a - b) >= 0; }
static inline bool seq_gt(uint32_t a, uint32_t b) { return (int32_t)(a - b) >  0; }

static inline uint64_t ms_to_ticks(uint32_t ms) {
    uint64_t t = ms / 10;
    return t ? t : 1;
}

// ===========================================================================
// Transmission
// ===========================================================================
static void tcp_tx_full(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                        uint32_t seq, uint32_t ack, uint8_t flags,
                        const void* payload, uint16_t payload_len, uint16_t window) {
    net_if_t* netif = net_get_primary_if();
    uint32_t src_ip = netif ? netif->ip : 0x0A00020F; // 10.0.2.15

    uint16_t total_len = sizeof(tcp_header_t) + payload_len;
    uint8_t* buf = (uint8_t*)kzalloc(total_len);
    if (!buf) return;

    tcp_header_t* tcp = (tcp_header_t*)buf;
    tcp->src_port = htons(src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq_num  = htonl(seq);
    tcp->ack_num  = htonl(ack);
    tcp->data_offset_reserved = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags    = flags;
    tcp->window_size = htons(window);
    tcp->urgent_pointer = 0;

    if (payload && payload_len > 0) {
        memcpy(buf + sizeof(tcp_header_t), payload, payload_len);
    }

    tcp->checksum = 0;
    tcp->checksum = tcp_checksum(src_ip, dst_ip, tcp, payload, payload_len);

    net_send_ip(dst_ip, IP_PROTO_TCP, buf, total_len);
    kfree(buf);
}

void tcp_send_packet(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                     uint32_t seq, uint32_t ack, uint8_t flags,
                     const void* payload, uint16_t payload_len) {
    tcp_tx_full(dst_ip, src_port, dst_port, seq, ack, flags, payload, payload_len, 65535);
}

// Free space in the receive ring, which is what we advertise as our window so
// a peer stops before overrunning a reader that has fallen behind.
static uint16_t tcp_window(const tcp_conn_t* c) {
    if (!c->rx_buf) return 65535;
    return (uint16_t)(TCP_RX_BUF_SIZE - c->rx_count);
}

// Send on a connection at an explicit sequence number. Retransmissions reuse
// the original segment's sequence number, which is why this is separate from
// the connection's current send position.
static void tcp_tx_seq(tcp_conn_t* c, uint32_t seq, uint8_t flags,
                       const void* payload, uint16_t payload_len) {
    tcp_tx_full(c->remote_ip, c->local_port, c->remote_port,
                seq, c->ack_num, flags, payload, payload_len, tcp_window(c));
}

static void tcp_tx(tcp_conn_t* c, uint8_t flags, const void* payload, uint16_t payload_len) {
    tcp_tx_seq(c, c->seq_num, flags, payload, payload_len);
}

// ===========================================================================
// Connection table
// ===========================================================================
static void conn_free(tcp_conn_t* c) {
    if (c->rx_buf) { kfree(c->rx_buf); c->rx_buf = NULL; }
    memset(c, 0, sizeof(*c));
}

// A connection in TIME_WAIT still occupies its slot so that a stray duplicate
// from the old incarnation cannot be mistaken for traffic on a new one.
static void reap_time_wait(void) {
    uint64_t now = pit_get_ticks();
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        tcp_conn_t* c = &tcp_connections[i];
        if (!c->in_use || c->state != TCP_STATE_TIME_WAIT) continue;

        uint64_t linger = (c->role == TCP_ROLE_SERVICE)
                        ? TCP_SVC_TIME_WAIT_TICKS : TCP_TIME_WAIT_TICKS;
        if ((now - c->closed_at) >= linger) conn_free(c);
    }
}

static tcp_conn_t* find_conn(uint32_t r_ip, uint16_t r_port, uint16_t l_port) {
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        tcp_conn_t* c = &tcp_connections[i];
        if (c->in_use && c->remote_ip == r_ip &&
            c->remote_port == r_port && c->local_port == l_port) {
            return c;
        }
    }
    return NULL;
}

static tcp_conn_t* alloc_conn(void) {
    reap_time_wait();
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        if (!tcp_connections[i].in_use) {
            memset(&tcp_connections[i], 0, sizeof(tcp_conn_t));
            tcp_connections[i].in_use = true;
            tcp_note_peak();
            return &tcp_connections[i];
        }
    }
    return NULL;
}

static tcp_conn_t* alloc_conn_server(uint32_t r_ip, uint16_t r_port, uint16_t l_port) {
    tcp_conn_t* c = alloc_conn();
    if (!c) return NULL;

    net_if_t* netif = net_get_primary_if();
    c->local_ip    = netif ? netif->ip : 0x0A00020F;
    c->remote_ip   = r_ip;
    c->remote_port = r_port;
    c->local_port  = l_port;
    c->role        = TCP_ROLE_SERVICE;
    c->state       = TCP_STATE_CLOSED;
    c->seq_num     = 1000;
    c->ack_num     = 0;
    return c;
}

// A connection born from an inbound SYN on a listening port. It carries a
// stream, so unlike the inline-service role it needs a receive buffer.
static tcp_conn_t* alloc_conn_accepted(uint32_t r_ip, uint16_t r_port, uint16_t l_port) {
    tcp_conn_t* c = alloc_conn();
    if (!c) return NULL;

    c->rx_buf = (uint8_t*)kzalloc(TCP_RX_BUF_SIZE);
    if (!c->rx_buf) { conn_free(c); return NULL; }

    net_if_t* netif = net_get_primary_if();
    c->local_ip    = netif ? netif->ip : 0x0A00020F;
    c->remote_ip   = r_ip;
    c->remote_port = r_port;
    c->local_port  = l_port;
    c->role        = TCP_ROLE_ACCEPTED;
    c->iss         = prng_rand32();
    c->seq_num     = c->iss;
    c->snd_una     = c->iss;
    return c;
}

static uint16_t alloc_ephemeral_port(void) {
    for (int attempt = 0; attempt < 512; attempt++) {
        uint16_t p = (uint16_t)(TCP_EPHEMERAL_LO +
                     (prng_rand32() % (TCP_EPHEMERAL_HI - TCP_EPHEMERAL_LO + 1)));
        bool taken = false;
        for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
            if (tcp_connections[i].in_use && tcp_connections[i].local_port == p) {
                taken = true;
                break;
            }
        }
        if (!taken) return p;
    }
    return 0;
}

// ===========================================================================
// Receive ring
// ===========================================================================

// Store what fits and report how much was taken, so the acknowledgement only
// ever covers bytes actually kept.
static uint16_t rx_append(tcp_conn_t* c, const uint8_t* data, uint16_t len) {
    if (!c->rx_buf || !data) return 0;

    uint16_t stored = 0;
    while (stored < len && c->rx_count < TCP_RX_BUF_SIZE) {
        uint16_t tail = (uint16_t)((c->rx_head + c->rx_count) % TCP_RX_BUF_SIZE);
        c->rx_buf[tail] = data[stored++];
        c->rx_count++;
    }
    return stored;
}

// ===========================================================================
// Listener table
// ===========================================================================
static tcp_listener_t* find_listener(uint16_t port) {
    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        if (tcp_listeners[i].in_use && tcp_listeners[i].port == port) {
            return &tcp_listeners[i];
        }
    }
    return NULL;
}

// A handshake finished on a listening port: hand the connection to whoever is
// in tcp_accept. If nobody has drained the backlog, the peer is refused rather
// than left believing it has an open connection nobody will ever read.
static void listener_enqueue(tcp_conn_t* c) {
    tcp_listener_t* l = find_listener(c->local_port);
    if (!l) return;

    if (l->count >= l->backlog) {
        tcp_tx(c, TCP_FLAG_RST, NULL, 0);
        conn_free(c);
        return;
    }
    l->queue[l->count++] = c;
}

// ===========================================================================
// Stream state machine, shared by active opens and accepted connections
// ===========================================================================
static void tcp_stream_input(tcp_conn_t* c, uint32_t seq, uint32_t ack, uint8_t flags,
                             const uint8_t* payload, uint16_t payload_len) {
    if (flags & TCP_FLAG_RST) {
        c->reset = true;
        c->state = TCP_STATE_CLOSED;
        return;
    }

    if (c->state == TCP_STATE_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            // The SYN we sent occupies one sequence number, so a SYN-ACK that
            // completes our handshake acknowledges exactly iss + 1.
            if (ack != c->iss + 1) return;
            c->snd_una = ack;
            c->ack_num = seq + 1;
            c->state   = TCP_STATE_ESTABLISHED;
            tcp_total_opened++;
            tcp_tx(c, TCP_FLAG_ACK, NULL, 0);
        } else if (flags & TCP_FLAG_SYN) {
            // Simultaneous open: both ends sent a SYN at once.
            c->ack_num = seq + 1;
            c->state   = TCP_STATE_SYN_RCVD;
            tcp_tx_seq(c, c->iss, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
        }
        return;
    }

    if (c->state == TCP_STATE_SYN_RCVD) {
        if ((flags & TCP_FLAG_ACK) && seq_ge(ack, c->iss + 1)) {
            c->snd_una = ack;
            c->state   = TCP_STATE_ESTABLISHED;
            tcp_total_opened++;
            // A passive open is only complete now: this is the third leg of
            // the handshake, so the connection can finally be handed out.
            if (c->role == TCP_ROLE_ACCEPTED) listener_enqueue(c);
        }
        return;
    }

    if (c->state == TCP_STATE_CLOSED) return;

    // A lingering connection only has to keep acknowledging the peer's FIN,
    // whether it is a retransmission or the peer's first one arriving after we
    // stopped waiting for it.
    if (c->state == TCP_STATE_TIME_WAIT) {
        if (flags & TCP_FLAG_FIN) {
            if (seq == c->ack_num) c->ack_num++;   // the FIN itself
            c->peer_fin = true;
            tcp_tx(c, TCP_FLAG_ACK, NULL, 0);
        }
        return;
    }

    // Advance the send window, ignoring anything that acknowledges more than
    // has actually been sent.
    if ((flags & TCP_FLAG_ACK) && seq_gt(ack, c->snd_una) && seq_ge(c->seq_num, ack)) {
        c->snd_una = ack;
    }

    // Accept in-order data. Anything else earns a duplicate acknowledgement
    // telling the peer where we actually are.
    if (payload_len > 0 && !c->peer_fin) {
        if (seq == c->ack_num) {
            c->ack_num += rx_append(c, payload, payload_len);
        }
        tcp_tx(c, TCP_FLAG_ACK, NULL, 0);
    }

    // A FIN sits after the segment's data, so it is only in order once every
    // preceding byte has been taken.
    if ((flags & TCP_FLAG_FIN) && !c->peer_fin && c->ack_num == seq + payload_len) {
        c->ack_num++;
        c->peer_fin = true;
        tcp_tx(c, TCP_FLAG_ACK, NULL, 0);

        if (c->state == TCP_STATE_ESTABLISHED)    c->state = TCP_STATE_CLOSE_WAIT;
        else if (c->state == TCP_STATE_FIN_WAIT1) c->state = TCP_STATE_CLOSING;
        else if (c->state == TCP_STATE_FIN_WAIT2) {
            c->state = TCP_STATE_TIME_WAIT;
            c->closed_at = pit_get_ticks();
        }
    }

    // Our own FIN also consumes a sequence number: once snd_una reaches
    // seq_num there is nothing of ours left outstanding.
    if (c->snd_una == c->seq_num) {
        if (c->state == TCP_STATE_FIN_WAIT1) {
            c->state = TCP_STATE_FIN_WAIT2;
        } else if (c->state == TCP_STATE_CLOSING) {
            c->state = TCP_STATE_TIME_WAIT;
            c->closed_at = pit_get_ticks();
        } else if (c->state == TCP_STATE_LAST_ACK) {
            c->state = TCP_STATE_CLOSED;
        }
    }
}

// ===========================================================================
// Server state machine
//
// Behaviour is unchanged from the original listener: SSH and HTTP are served
// straight out of the receive path, one request per segment.
// ===========================================================================
static void tcp_server_input(tcp_conn_t* c, uint32_t src_ip, uint16_t src_port,
                             uint16_t dst_port, uint32_t seq_num, uint8_t flags,
                             const uint8_t* payload, uint16_t payload_len) {
    bool is_ssh_port  = (dst_port == sshd_get_port() && sshd_is_running());
    bool is_http_port = (dst_port == 80 && httpd_is_running());

    if (flags & TCP_FLAG_SYN) {
        c->state   = TCP_STATE_SYN_RCVD;
        c->ack_num = seq_num + 1;
        c->seq_num = 10000;

        tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                        TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
        c->seq_num++;
        return;
    }

    if (flags & TCP_FLAG_FIN) {
        c->ack_num = seq_num + 1;
        tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                        TCP_FLAG_ACK | TCP_FLAG_FIN, NULL, 0);
        conn_free(c);
        return;
    }

    if (flags & TCP_FLAG_RST) {
        conn_free(c);
        return;
    }

    if (c->state == TCP_STATE_SYN_RCVD) {
        c->state = TCP_STATE_ESTABLISHED;
        tcp_total_opened++;
        if (is_ssh_port && payload_len == 0) {
            const char* banner = SSH_BANNER;
            uint16_t blen = (uint16_t)strlen(banner);
            tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                            TCP_FLAG_PSH | TCP_FLAG_ACK, banner, blen);
            c->seq_num += blen;
            return;
        }
    }
    c->state = TCP_STATE_ESTABLISHED;

    if (payload_len == 0) return;
    c->ack_num = seq_num + payload_len;

    if (is_ssh_port) {
        char resp[1024];
        int rlen = sshd_process_packet((const char*)payload, payload_len, resp, sizeof(resp));
        if (rlen > 0) {
            tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                            TCP_FLAG_PSH | TCP_FLAG_ACK, resp, (uint16_t)rlen);
            c->seq_num += rlen;
        } else {
            tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                            TCP_FLAG_ACK, NULL, 0);
        }
    } else if (is_http_port) {
        char resp[2048];
        int rlen = httpd_handle_request((const char*)payload, resp, sizeof(resp));
        if (rlen > 0) {
            tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                            TCP_FLAG_PSH | TCP_FLAG_ACK, resp, (uint16_t)rlen);
            c->seq_num += rlen;

            // The response advertises "Connection: close", so honour it: send
            // the FIN now and let the connection linger in TIME_WAIT until the
            // reaper takes it. Without this every served request held its slot
            // for good and the 32-entry table filled up after 32 hits.
            tcp_send_packet(src_ip, dst_port, src_port, c->seq_num, c->ack_num,
                            TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            c->seq_num++;
            c->state     = TCP_STATE_TIME_WAIT;
            c->closed_at = pit_get_ticks();
        }
    }
}

// ===========================================================================
// Receive dispatch
// ===========================================================================
void tcp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip) {
    if (!packet || length < sizeof(tcp_header_t)) return;

    const tcp_header_t* tcp = (const tcp_header_t*)packet;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint32_t seq_num  = ntohl(tcp->seq_num);
    uint32_t ack_num  = ntohl(tcp->ack_num);
    uint8_t  flags    = tcp->flags;

    uint8_t header_len = (tcp->data_offset_reserved >> 4) * 4;
    if (header_len < sizeof(tcp_header_t) || header_len > length) return;

    const uint8_t* payload = packet + header_len;
    uint16_t payload_len = length - header_len;

    // 1. An established connection -- ours or a peer's -- owns this segment.
    tcp_conn_t* conn = find_conn(src_ip, src_port, dst_port);
    if (conn) {
        // Lingering is role-independent: whichever half opened the connection,
        // all a TIME_WAIT entry owes the peer is an acknowledgement of its FIN.
        if (conn->state == TCP_STATE_TIME_WAIT) {
            tcp_stream_input(conn, seq_num, ack_num, flags, payload, payload_len);
        } else if (conn->role == TCP_ROLE_SERVICE) {
            tcp_server_input(conn, src_ip, src_port, dst_port, seq_num, flags,
                             payload, payload_len);
        } else {
            tcp_stream_input(conn, seq_num, ack_num, flags, payload, payload_len);
        }
        return;
    }

    // 2. A SYN to a port someone called tcp_listen on starts a passive open.
    //    Only a SYN may do so: anything else for an unknown 4-tuple is stale.
    if ((flags & TCP_FLAG_SYN) && !(flags & TCP_FLAG_ACK) && find_listener(dst_port)) {
        conn = alloc_conn_accepted(src_ip, src_port, dst_port);
        if (conn) {
            conn->ack_num = seq_num + 1;
            conn->state   = TCP_STATE_SYN_RCVD;
            tcp_tx_seq(conn, conn->iss, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
            conn->seq_num = conn->iss + 1;   // our SYN consumes a sequence number
        } else {
            tcp_send_packet(src_ip, dst_port, src_port, 0, seq_num + 1,
                            TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
        }
        return;
    }

    // 3. Otherwise one of the built-in services may want it.
    bool is_ssh_port  = (dst_port == sshd_get_port() && sshd_is_running());
    bool is_http_port = (dst_port == 80 && httpd_is_running());
    if (is_ssh_port || is_http_port) {
        conn = alloc_conn_server(src_ip, src_port, dst_port);
        if (conn) {
            tcp_server_input(conn, src_ip, src_port, dst_port, seq_num, flags,
                             payload, payload_len);
        }
        return;
    }

    // 3. Nothing is listening and nothing matches: refuse the segment.
    if (!(flags & TCP_FLAG_RST)) {
        tcp_send_packet(src_ip, dst_port, src_port, 0, seq_num + 1,
                        TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
    }
}

// ===========================================================================
// Active open API
// ===========================================================================
tcp_conn_t* tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms) {
    if (dst_ip == 0 || dst_port == 0) return NULL;

    tcp_conn_t* c = alloc_conn();
    if (!c) return NULL;

    c->rx_buf = (uint8_t*)kzalloc(TCP_RX_BUF_SIZE);
    if (!c->rx_buf) { conn_free(c); return NULL; }

    c->local_port = alloc_ephemeral_port();
    if (c->local_port == 0) { conn_free(c); return NULL; }

    net_if_t* netif = net_get_primary_if();
    c->local_ip    = netif ? netif->ip : 0x0A00020F;
    c->remote_ip   = dst_ip;
    c->remote_port = dst_port;
    c->role        = TCP_ROLE_CLIENT;
    c->iss         = prng_rand32();
    c->seq_num     = c->iss;
    c->snd_una     = c->iss;
    c->ack_num     = 0;
    c->state       = TCP_STATE_SYN_SENT;

    tcp_tx_seq(c, c->iss, TCP_FLAG_SYN, NULL, 0);
    c->seq_num = c->iss + 1;   // the SYN consumes one sequence number

    uint64_t t0   = pit_get_ticks();
    uint64_t wait = ms_to_ticks(timeout_ms);
    uint64_t last = t0;

    while ((pit_get_ticks() - t0) < wait) {
        if (c->state == TCP_STATE_ESTABLISHED) return c;
        if (c->reset || c->state == TCP_STATE_CLOSED) break;

        if ((pit_get_ticks() - last) >= TCP_RTO_TICKS) {
            tcp_tx_seq(c, c->iss, TCP_FLAG_SYN, NULL, 0);
            last = pit_get_ticks();
        }
        net_wait();
    }

    conn_free(c);
    return NULL;
}

int tcp_send(tcp_conn_t* c, const void* data, uint16_t len) {
    if (!c || !c->in_use || !data) return -1;
    if (c->state != TCP_STATE_ESTABLISHED && c->state != TCP_STATE_CLOSE_WAIT) return -1;
    if (len == 0) return 0;

    const uint8_t* p = (const uint8_t*)data;
    uint16_t sent = 0;

    while (sent < len) {
        uint16_t chunk = (uint16_t)(len - sent);
        if (chunk > TCP_MSS) chunk = TCP_MSS;

        uint32_t seg_seq = c->seq_num;
        uint32_t seg_end = seg_seq + chunk;

        // Publish the segment as sent before waiting: the acknowledgement is
        // only accepted if it falls within what seq_num says we have sent.
        c->seq_num = seg_end;

        int tries = 0;
        for (;;) {
            tcp_tx_seq(c, seg_seq, TCP_FLAG_PSH | TCP_FLAG_ACK, p + sent, chunk);

            uint64_t t0 = pit_get_ticks();
            while ((pit_get_ticks() - t0) < TCP_RTO_TICKS) {
                if (seq_ge(c->snd_una, seg_end) || c->reset) break;
                net_wait();
            }

            if (seq_ge(c->snd_una, seg_end)) break;
            if (c->reset) return sent ? (int)sent : -1;
            if (++tries >= TCP_MAX_RETRIES) return sent ? (int)sent : -1;
        }

        sent += chunk;
    }

    return (int)sent;
}

int tcp_recv(tcp_conn_t* c, void* buf, uint16_t len, uint32_t timeout_ms) {
    if (!c || !c->in_use || !buf || len == 0) return -1;

    uint64_t t0   = pit_get_ticks();
    uint64_t wait = ms_to_ticks(timeout_ms);

    while (c->rx_count == 0) {
        if (c->reset) return -1;
        if (c->peer_fin) return 0;                       // stream ended
        if ((pit_get_ticks() - t0) >= wait) return 0;     // nothing arrived
        net_wait();
    }

    uint16_t before = c->rx_count;
    uint16_t n = (c->rx_count < len) ? c->rx_count : len;

    uint8_t* out = (uint8_t*)buf;
    for (uint16_t i = 0; i < n; i++) {
        out[i] = c->rx_buf[(c->rx_head + i) % TCP_RX_BUF_SIZE];
    }

    c->rx_head = (uint16_t)((c->rx_head + n) % TCP_RX_BUF_SIZE);
    c->rx_count = (uint16_t)(c->rx_count - n);

    // If the window had closed down far enough that the peer may have stalled,
    // tell it immediately that there is room again.
    if (before >= TCP_RX_BUF_SIZE / 2 && c->state == TCP_STATE_ESTABLISHED) {
        tcp_tx(c, TCP_FLAG_ACK, NULL, 0);
    }

    return (int)n;
}

void tcp_close(tcp_conn_t* c) {
    if (!c || !c->in_use) return;

    if (c->state == TCP_STATE_ESTABLISHED || c->state == TCP_STATE_CLOSE_WAIT) {
        // Closing after the peer already sent its FIN is the passive case and
        // finishes in LAST_ACK; closing first is the active case.
        bool passive = (c->state == TCP_STATE_CLOSE_WAIT);

        tcp_tx(c, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        c->seq_num++;   // our FIN consumes a sequence number
        c->state = passive ? TCP_STATE_LAST_ACK : TCP_STATE_FIN_WAIT1;

        uint64_t t0 = pit_get_ticks();
        while ((pit_get_ticks() - t0) < TCP_CLOSE_TICKS) {
            if (c->reset) break;
            if (c->state == TCP_STATE_CLOSED || c->state == TCP_STATE_TIME_WAIT) break;
            net_wait();
        }
    }

    // A close we started may not have finished inside that wait: the peer is
    // entitled to take its time sending its own FIN. Falling into TIME_WAIT
    // rather than freeing the slot means that late FIN gets acknowledged
    // properly instead of being answered with a reset.
    if (c->state == TCP_STATE_FIN_WAIT1 || c->state == TCP_STATE_FIN_WAIT2 ||
        c->state == TCP_STATE_CLOSING) {
        c->state = TCP_STATE_TIME_WAIT;
    }

    if (c->state == TCP_STATE_TIME_WAIT) {
        // Hold the slot so a late duplicate cannot land on a new connection.
        // The receive buffer is not needed for that, so give it back now.
        if (c->rx_buf) { kfree(c->rx_buf); c->rx_buf = NULL; }
        c->rx_head = c->rx_count = 0;
        c->closed_at = pit_get_ticks();
        return;
    }

    conn_free(c);
}

// ===========================================================================
// Passive open API
// ===========================================================================
int tcp_listen(uint16_t port, int backlog) {
    if (port == 0) return -1;
    if (find_listener(port)) return -1;         // already claimed
    if (backlog < 1) backlog = 1;
    if (backlog > TCP_BACKLOG_MAX) backlog = TCP_BACKLOG_MAX;

    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        if (tcp_listeners[i].in_use) continue;
        memset(&tcp_listeners[i], 0, sizeof(tcp_listener_t));
        tcp_listeners[i].in_use  = true;
        tcp_listeners[i].port    = port;
        tcp_listeners[i].backlog = backlog;
        return 0;
    }
    return -1;
}

tcp_conn_t* tcp_accept(uint16_t port, uint32_t timeout_ms) {
    tcp_listener_t* l = find_listener(port);
    if (!l) return NULL;

    uint64_t t0   = pit_get_ticks();
    uint64_t wait = ms_to_ticks(timeout_ms);

    while (l->count == 0) {
        if ((pit_get_ticks() - t0) >= wait) return NULL;
        net_wait();
    }

    tcp_conn_t* c = l->queue[0];
    for (int i = 1; i < l->count; i++) l->queue[i - 1] = l->queue[i];
    l->count--;
    return c;
}

void tcp_unlisten(uint16_t port) {
    tcp_listener_t* l = find_listener(port);
    if (!l) return;

    // Anything still queued was never handed to a reader, so refuse it rather
    // than leaving the peer with a connection nobody owns.
    for (int i = 0; i < l->count; i++) {
        tcp_conn_t* c = l->queue[i];
        if (!c || !c->in_use) continue;
        tcp_tx(c, TCP_FLAG_RST, NULL, 0);
        conn_free(c);
    }
    memset(l, 0, sizeof(*l));
}

int tcp_get_listener_count(void) {
    int n = 0;
    for (int i = 0; i < TCP_MAX_LISTENERS; i++) if (tcp_listeners[i].in_use) n++;
    return n;
}

uint16_t tcp_get_listener_port(int idx) {
    int n = 0;
    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        if (!tcp_listeners[i].in_use) continue;
        if (n++ == idx) return tcp_listeners[i].port;
    }
    return 0;
}

int tcp_get_listener_backlog(int idx) {
    int n = 0;
    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        if (!tcp_listeners[i].in_use) continue;
        if (n++ == idx) return tcp_listeners[i].count;
    }
    return 0;
}

bool tcp_is_established(const tcp_conn_t* c) {
    return c && c->in_use && c->state == TCP_STATE_ESTABLISHED;
}

bool tcp_peer_closed(const tcp_conn_t* c) {
    return c && c->in_use && c->peer_fin;
}

size_t tcp_get_connections_count(void) {
    // Expired TIME_WAIT slots are normally reclaimed when the next connection
    // is allocated, which never happens if none follows. netstat and the
    // Network Monitor poll this, so it doubles as the periodic tick that stops
    // finished connections lingering in the table for good.
    reap_time_wait();

    size_t count = 0;
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        if (tcp_connections[i].in_use) count++;
    }
    return count;
}

const tcp_conn_t* tcp_get_connection(size_t index) {
    if (index >= MAX_TCP_CONNS || !tcp_connections[index].in_use) return NULL;
    return &tcp_connections[index];
}

int tcp_conn_table_size(void) { return MAX_TCP_CONNS; }
uint64_t tcp_get_total_opened(void) { return tcp_total_opened; }
int tcp_get_peak_connections(void) { return tcp_peak_conns; }

const char* tcp_state_name(uint8_t state) {
    static const char* const names[] = {
        "CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED",
        "FIN_WAIT1", "FIN_WAIT2", "CLOSE_WAIT", "CLOSING", "LAST_ACK", "TIME_WAIT"
    };
    if (state >= sizeof(names) / sizeof(names[0])) return "UNKNOWN";
    return names[state];
}
