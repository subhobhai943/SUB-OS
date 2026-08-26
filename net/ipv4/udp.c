// UDP (User Datagram Protocol) transport for SUB-OS.
//
// Provides connectionless datagram delivery on top of the IPv4 layer. Inbound
// datagrams are demultiplexed by destination port to a small table of bound
// handlers (used by the socket layer, the DNS resolver and the DHCP client);
// unclaimed ports are counted as drops. Outbound datagrams are checksummed with
// the standard IPv4 pseudo-header.
#include <net/udp.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define UDP_MAX_BINDINGS 32
#define UDP_EPHEMERAL_LO 49152u
#define UDP_EPHEMERAL_HI 65535u

typedef struct {
    uint16_t      port;
    udp_handler_t handler;
    void*         ctx;
    bool          active;
} udp_binding_t;

// Pseudo-header used only for checksum computation (never transmitted).
typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t udp_len;
} __attribute__((packed)) udp_pseudo_header_t;

static udp_binding_t bindings[UDP_MAX_BINDINGS];
static uint16_t      next_ephemeral = UDP_EPHEMERAL_LO;

static volatile uint64_t stat_rx = 0;
static volatile uint64_t stat_tx = 0;
static volatile uint64_t stat_drop = 0;

static uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip,
                             const udp_header_t* udp, const void* payload,
                             uint16_t payload_len) {
    udp_pseudo_header_t ph;
    ph.src_ip = src_ip;
    ph.dst_ip = dst_ip;
    ph.zero = 0;
    ph.protocol = IP_PROTO_UDP;
    ph.udp_len = htons((uint16_t)(sizeof(udp_header_t) + payload_len));

    uint32_t sum = 0;
    const uint16_t* p = (const uint16_t*)&ph;
    for (size_t i = 0; i < sizeof(ph) / 2; i++) sum += *p++;

    p = (const uint16_t*)udp;
    for (size_t i = 0; i < sizeof(udp_header_t) / 2; i++) sum += *p++;

    if (payload && payload_len > 0) {
        p = (const uint16_t*)payload;
        size_t len = payload_len;
        while (len > 1) { sum += *p++; len -= 2; }
        if (len > 0) sum += *(const uint8_t*)p;
    }

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    uint16_t cksum = (uint16_t)(~sum);
    // A zero checksum means "not computed" on the wire; send 0xFFFF instead.
    return cksum ? cksum : 0xFFFF;
}

void udp_init(void) {
    memset(bindings, 0, sizeof(bindings));
    next_ephemeral = UDP_EPHEMERAL_LO;
    stat_rx = stat_tx = stat_drop = 0;
    printk(KERN_INFO "NET: UDP (User Datagram Protocol) transport layer online\n");
}

int udp_bind(uint16_t port, udp_handler_t handler, void* ctx) {
    if (!handler || port == 0) return -1;
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (bindings[i].active && bindings[i].port == port) return -1;
    }
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (!bindings[i].active) {
            bindings[i].port = port;
            bindings[i].handler = handler;
            bindings[i].ctx = ctx;
            bindings[i].active = true;
            return 0;
        }
    }
    return -1;
}

void udp_unbind(uint16_t port) {
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (bindings[i].active && bindings[i].port == port) {
            bindings[i].active = false;
            bindings[i].handler = NULL;
            bindings[i].ctx = NULL;
            return;
        }
    }
}

uint16_t udp_alloc_ephemeral_port(void) {
    for (uint32_t tries = 0; tries <= (UDP_EPHEMERAL_HI - UDP_EPHEMERAL_LO); tries++) {
        uint16_t candidate = next_ephemeral;
        if (next_ephemeral >= UDP_EPHEMERAL_HI) next_ephemeral = UDP_EPHEMERAL_LO;
        else next_ephemeral++;

        bool taken = false;
        for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
            if (bindings[i].active && bindings[i].port == candidate) { taken = true; break; }
        }
        if (!taken) return candidate;
    }
    return 0;
}

void udp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip) {
    if (!packet || length < sizeof(udp_header_t)) { stat_drop++; return; }

    const udp_header_t* udp = (const udp_header_t*)packet;
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t udp_len  = ntohs(udp->length);

    if (udp_len < sizeof(udp_header_t) || udp_len > length) { stat_drop++; return; }

    const uint8_t* data = packet + sizeof(udp_header_t);
    uint16_t data_len = udp_len - sizeof(udp_header_t);

    stat_rx++;

    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (bindings[i].active && bindings[i].port == dst_port && bindings[i].handler) {
            bindings[i].handler(src_ip, src_port, dst_port, data, data_len, bindings[i].ctx);
            return;
        }
    }
    // No listener on this port.
    stat_drop++;
}

void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void* data, uint16_t len) {
    uint16_t total = (uint16_t)(sizeof(udp_header_t) + len);
    uint8_t* buf = (uint8_t*)kzalloc(total);
    if (!buf) return;

    udp_header_t* udp = (udp_header_t*)buf;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons(total);
    udp->checksum = 0;

    if (data && len > 0) memcpy(buf + sizeof(udp_header_t), data, len);

    net_if_t* nif = net_get_primary_if();
    uint32_t src_ip = nif ? nif->ip : 0;
    udp->checksum = udp_checksum(src_ip, dst_ip, udp, data, len);

    net_send_ip(dst_ip, IP_PROTO_UDP, buf, total);
    stat_tx++;
    kfree(buf);
}

void udp_send_broadcast(uint16_t src_port, uint16_t dst_port,
                        const void* data, uint16_t len) {
    // Build IPv4 + UDP by hand: src 0.0.0.0, dst 255.255.255.255, sent to the
    // Ethernet broadcast address without ARP (the interface has no IP yet).
    static const uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t udp_total = (uint16_t)(sizeof(udp_header_t) + len);
    uint16_t ip_total  = (uint16_t)(sizeof(ip_header_t) + udp_total);

    uint8_t* buf = (uint8_t*)kzalloc(ip_total);
    if (!buf) return;

    ip_header_t* ip = (ip_header_t*)buf;
    ip->ihl_version = 0x45;
    ip->tos = 0;
    ip->total_length = htons(ip_total);
    ip->id = htons(0x5342); // 'SB'
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = 0x00000000;
    ip->dst_ip = 0xFFFFFFFF;

    // IPv4 header checksum.
    {
        uint32_t sum = 0;
        const uint16_t* p = (const uint16_t*)ip;
        for (size_t i = 0; i < sizeof(ip_header_t) / 2; i++) sum += *p++;
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        ip->checksum = (uint16_t)(~sum);
    }

    udp_header_t* udp = (udp_header_t*)(buf + sizeof(ip_header_t));
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons(udp_total);
    udp->checksum = 0;
    if (data && len > 0) memcpy((uint8_t*)udp + sizeof(udp_header_t), data, len);
    udp->checksum = udp_checksum(0x00000000, 0xFFFFFFFF, udp, data, len);

    net_send_eth(bcast_mac, ETHERTYPE_IPV4, buf, ip_total);
    stat_tx++;
    kfree(buf);
}

void udp_get_stats(uint64_t* rx, uint64_t* tx, uint64_t* drop) {
    if (rx) *rx = stat_rx;
    if (tx) *tx = stat_tx;
    if (drop) *drop = stat_drop;
}

int udp_get_bindings(uint16_t* ports_out, int max) {
    int n = 0;
    for (int i = 0; i < UDP_MAX_BINDINGS && n < max; i++) {
        if (bindings[i].active) ports_out[n++] = bindings[i].port;
    }
    return n;
}
