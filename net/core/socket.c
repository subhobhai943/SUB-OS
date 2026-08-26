// BSD-style socket layer for SUB-OS.
//
// Implements connectionless (SOCK_DGRAM) sockets on top of the UDP transport
// and connection-oriented (SOCK_STREAM) sockets on top of the TCP engine.
//
// Binding a datagram socket registers a UDP handler that appends inbound
// datagrams to the socket's fixed-depth receive ring; sys_recv/sys_recvfrom pop
// the oldest datagram. A stream socket instead holds a TCP connection opened by
// sys_connect, and its send/recv calls carry a byte stream through that.
#include <net/socket.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/net.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_SOCKETS   32
#define RX_RING_DEPTH  8
#define DGRAM_MAX   1472   // 1500 MTU - 20 IP - 8 UDP

#define SOCK_CONNECT_TIMEOUT_MS 4000
#define SOCK_RECV_TIMEOUT_MS    2000
#define SOCK_ACCEPT_TIMEOUT_MS  5000

typedef struct {
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t len;
    uint8_t  data[DGRAM_MAX];
} dgram_t;

// Per-socket receive ring, kept parallel to the public socket_t so the exported
// struct stays small.
typedef struct {
    dgram_t  ring[RX_RING_DEPTH];
    int      head;   // next slot to read
    int      count;  // queued datagrams
    bool     bound;  // a UDP binding is installed for local_port
    tcp_conn_t* tcp; // stream sockets only: the connection sys_connect opened
    bool     listening; // stream sockets only: a TCP listener holds local_port
} sock_priv_t;

static socket_t    socket_table[MAX_SOCKETS];
static sock_priv_t socket_priv[MAX_SOCKETS];

static volatile uint64_t stat_tx = 0;
static volatile uint64_t stat_rx = 0;
static volatile uint64_t stat_drop = 0;

void socket_subsystem_init(void) {
    memset(socket_table, 0, sizeof(socket_table));
    memset(socket_priv, 0, sizeof(socket_priv));
    stat_tx = stat_rx = stat_drop = 0;
    printk(KERN_INFO "NET: BSD Sockets API Layer initialized (AF_INET support)\n");
}

// Runs in UDP receive context: enqueue the datagram, dropping the oldest if the
// ring is full so a stalled reader never wedges delivery.
static void socket_udp_cb(uint32_t src_ip, uint16_t src_port, uint16_t dst_port,
                          const uint8_t* data, uint16_t len, void* ctx) {
    (void)dst_port;
    sock_priv_t* pv = (sock_priv_t*)ctx;
    if (!pv) return;
    if (len > DGRAM_MAX) len = DGRAM_MAX;

    int slot;
    if (pv->count == RX_RING_DEPTH) {
        // Overwrite the oldest.
        pv->head = (pv->head + 1) % RX_RING_DEPTH;
        slot = (pv->head + pv->count - 1) % RX_RING_DEPTH;
        stat_drop++;
    } else {
        slot = (pv->head + pv->count) % RX_RING_DEPTH;
        pv->count++;
    }

    dgram_t* d = &pv->ring[slot];
    d->src_ip = src_ip;
    d->src_port = src_port;
    d->len = len;
    if (data && len) memcpy(d->data, data, len);
    stat_rx++;
}

int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) return -1;
    if (type != SOCK_DGRAM && type != SOCK_STREAM && type != SOCK_RAW) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!socket_table[i].in_use) {
            memset(&socket_table[i], 0, sizeof(socket_t));
            memset(&socket_priv[i], 0, sizeof(sock_priv_t));
            socket_table[i].in_use = true;
            socket_table[i].domain = domain;
            socket_table[i].type = type;
            socket_table[i].protocol = protocol ? protocol :
                (type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP);
            socket_table[i].state = 0;
            return i;
        }
    }
    return -1;
}

static bool valid_fd(int fd) {
    return fd >= 0 && fd < MAX_SOCKETS && socket_table[fd].in_use;
}

// Install the UDP binding for a datagram socket's local port (idempotent).
static int dgram_ensure_bound(int fd) {
    socket_t* s = &socket_table[fd];
    sock_priv_t* pv = &socket_priv[fd];
    if (pv->bound) return 0;
    if (s->local_port == 0) {
        s->local_port = udp_alloc_ephemeral_port();
        if (s->local_port == 0) return -1;
    }
    if (udp_bind(s->local_port, socket_udp_cb, pv) != 0) return -1;
    pv->bound = true;
    return 0;
}

int sys_bind(int sockfd, const struct sockaddr_in* addr, size_t addrlen) {
    (void)addrlen;
    if (!valid_fd(sockfd) || !addr) return -1;
    socket_t* s = &socket_table[sockfd];
    s->local_ip = addr->sin_addr;
    s->local_port = ntohs(addr->sin_port);
    if (s->type == SOCK_DGRAM) return dgram_ensure_bound(sockfd);
    return 0;
}

int sys_connect(int sockfd, const struct sockaddr_in* addr, size_t addrlen) {
    (void)addrlen;
    if (!valid_fd(sockfd) || !addr) return -1;
    socket_t* s = &socket_table[sockfd];
    sock_priv_t* pv = &socket_priv[sockfd];
    s->remote_ip = addr->sin_addr;
    s->remote_port = ntohs(addr->sin_port);

    // A stream socket really connects: the call returns only once the
    // three-way handshake has completed, or fails on timeout.
    if (s->type == SOCK_STREAM) {
        if (pv->tcp) return 0;   // already connected
        pv->tcp = tcp_connect(s->remote_ip, s->remote_port, SOCK_CONNECT_TIMEOUT_MS);
        if (!pv->tcp) return -1;
        s->local_ip   = pv->tcp->local_ip;
        s->local_port = pv->tcp->local_port;
        s->state = SOCK_STATE_CONNECTED;
        return 0;
    }

    s->state = SOCK_STATE_CONNECTED; // address memorised for send/recv
    if (s->type == SOCK_DGRAM) return dgram_ensure_bound(sockfd);
    return 0;
}

int sys_listen(int sockfd, int backlog) {
    if (!valid_fd(sockfd)) return -1;
    socket_t* s = &socket_table[sockfd];
    if (s->type != SOCK_STREAM || s->local_port == 0) return -1;

    if (tcp_listen(s->local_port, backlog) != 0) return -1;
    socket_priv[sockfd].listening = true;
    s->state = SOCK_STATE_LISTEN;
    return 0;
}

int sys_accept(int sockfd, struct sockaddr_in* addr, size_t* addrlen) {
    if (!valid_fd(sockfd)) return -1;
    socket_t* s = &socket_table[sockfd];
    if (s->type != SOCK_STREAM || !socket_priv[sockfd].listening) return -1;

    tcp_conn_t* c = tcp_accept(s->local_port, SOCK_ACCEPT_TIMEOUT_MS);
    if (!c) return -1;

    // The accepted connection needs a descriptor of its own; the listening
    // socket keeps its port and stays available for the next caller.
    int fd = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) { tcp_close(c); return -1; }

    socket_t* ns = &socket_table[fd];
    ns->local_ip    = c->local_ip;
    ns->local_port  = c->local_port;
    ns->remote_ip   = c->remote_ip;
    ns->remote_port = c->remote_port;
    ns->state       = SOCK_STATE_CONNECTED;
    socket_priv[fd].tcp = c;

    if (addr) {
        addr->sin_family = AF_INET;
        addr->sin_addr   = c->remote_ip;
        addr->sin_port   = htons(c->remote_port);
        if (addrlen) *addrlen = sizeof(struct sockaddr_in);
    }
    return fd;
}

ssize_t sys_sendto(int sockfd, const void* buf, size_t len, int flags,
                   const struct sockaddr_in* dest, size_t addrlen) {
    (void)flags; (void)addrlen;
    if (!valid_fd(sockfd) || !buf) return -1;
    socket_t* s = &socket_table[sockfd];
    if (s->type != SOCK_DGRAM) return -1;
    if (len > DGRAM_MAX) len = DGRAM_MAX;

    uint32_t dip;
    uint16_t dport;
    if (dest) { dip = dest->sin_addr; dport = ntohs(dest->sin_port); }
    else if (s->state == SOCK_STATE_CONNECTED) { dip = s->remote_ip; dport = s->remote_port; }
    else return -1;

    if (dgram_ensure_bound(sockfd) != 0) return -1;
    udp_send(dip, s->local_port, dport, buf, (uint16_t)len);
    stat_tx++;
    return (ssize_t)len;
}

ssize_t sys_send(int sockfd, const void* buf, size_t len, int flags) {
    if (!valid_fd(sockfd) || !buf) return -1;
    socket_t* s = &socket_table[sockfd];

    if (s->type == SOCK_STREAM) {
        sock_priv_t* pv = &socket_priv[sockfd];
        if (!pv->tcp) return -1;   // not connected
        if (len > 0xFFFF) len = 0xFFFF;
        int n = tcp_send(pv->tcp, buf, (uint16_t)len);
        if (n > 0) stat_tx++;
        return (n < 0) ? -1 : (ssize_t)n;
    }

    return sys_sendto(sockfd, buf, len, flags, NULL, 0);
}

ssize_t sys_recvfrom(int sockfd, void* buf, size_t len, int flags,
                     struct sockaddr_in* src, size_t* addrlen) {
    (void)flags;
    if (!valid_fd(sockfd) || !buf) return -1;
    socket_t* s = &socket_table[sockfd];
    if (s->type != SOCK_DGRAM) return 0;

    sock_priv_t* pv = &socket_priv[sockfd];
    if (pv->count == 0) return 0; // non-blocking: nothing queued

    dgram_t* d = &pv->ring[pv->head];
    size_t n = d->len < len ? d->len : len;
    memcpy(buf, d->data, n);

    if (src) {
        src->sin_family = AF_INET;
        src->sin_addr = d->src_ip;
        src->sin_port = htons(d->src_port);
        if (addrlen) *addrlen = sizeof(struct sockaddr_in);
    }

    pv->head = (pv->head + 1) % RX_RING_DEPTH;
    pv->count--;
    return (ssize_t)n;
}

ssize_t sys_recv(int sockfd, void* buf, size_t len, int flags) {
    if (!valid_fd(sockfd) || !buf) return -1;
    socket_t* s = &socket_table[sockfd];

    if (s->type == SOCK_STREAM) {
        sock_priv_t* pv = &socket_priv[sockfd];
        if (!pv->tcp) return -1;   // not connected
        if (len > 0xFFFF) len = 0xFFFF;
        int n = tcp_recv(pv->tcp, buf, (uint16_t)len, SOCK_RECV_TIMEOUT_MS);
        if (n > 0) stat_rx++;
        return (n < 0) ? -1 : (ssize_t)n;
    }

    return sys_recvfrom(sockfd, buf, len, flags, NULL, NULL);
}

int sys_close_socket(int sockfd) {
    if (!valid_fd(sockfd)) return -1;
    socket_t* s = &socket_table[sockfd];
    sock_priv_t* pv = &socket_priv[sockfd];
    if (pv->listening && s->local_port) tcp_unlisten(s->local_port);
    if (pv->tcp) tcp_close(pv->tcp);
    if (pv->bound && s->local_port) udp_unbind(s->local_port);
    memset(s, 0, sizeof(socket_t));
    memset(pv, 0, sizeof(sock_priv_t));
    return 0;
}

int socket_get_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_SOCKETS; i++) if (socket_table[i].in_use) n++;
    return n;
}

const socket_t* socket_get(int idx) {
    if (idx < 0 || idx >= MAX_SOCKETS || !socket_table[idx].in_use) return NULL;
    return &socket_table[idx];
}

void socket_get_stats(uint64_t* tx_dgrams, uint64_t* rx_dgrams, uint64_t* drops) {
    if (tx_dgrams) *tx_dgrams = stat_tx;
    if (rx_dgrams) *rx_dgrams = stat_rx;
    if (drops) *drops = stat_drop;
}
