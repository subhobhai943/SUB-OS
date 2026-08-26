// DHCP (Dynamic Host Configuration Protocol) client for SUB-OS.
//
// Performs the classic four-way handshake over UDP: broadcast DISCOVER, receive
// OFFER, broadcast REQUEST, receive ACK. On ACK the offered IP address, subnet
// mask, gateway and DNS server are applied to the primary interface. Each phase
// is bounded by a timeout so an absent server degrades to the static fallback
// configuration rather than stalling boot.
#include <net/dhcp.h>
#include <net/udp.h>
#include <net/net.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>
#include <arch/arch.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC       0x63825363u
#define DHCP_TIMEOUT_MS  2500

// BOOTP message ops and DHCP message types (option 53).
#define BOOTREQUEST 1
#define BOOTREPLY   2
#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPACK      5
#define DHCPNAK      6

typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t cookie;
    uint8_t  options[312];
} __attribute__((packed)) dhcp_packet_t;

static dhcp_lease_t g_lease;

// Filled from UDP receive context on OFFER/ACK.
static volatile bool     reply_ready = false;
static volatile uint8_t  reply_type = 0;
static volatile uint32_t reply_xid = 0;
static volatile uint32_t off_yiaddr = 0;
static volatile uint32_t off_mask = 0;
static volatile uint32_t off_router = 0;
static volatile uint32_t off_dns = 0;
static volatile uint32_t off_server = 0;
static volatile uint32_t off_lease = 0;

void dhcp_init(void) {
    memset(&g_lease, 0, sizeof(g_lease));
    reply_ready = false;
    printk(KERN_INFO "DHCP: Dynamic Host Configuration Protocol client ready\n");
}

const dhcp_lease_t* dhcp_get_lease(void) { return &g_lease; }

// Append a TLV option; returns the new write offset.
static int opt_put(uint8_t* opts, int o, uint8_t code, uint8_t len, const void* val) {
    opts[o++] = code;
    opts[o++] = len;
    if (len && val) { memcpy(&opts[o], val, len); o += len; }
    return o;
}

static void dhcp_udp_cb(uint32_t src_ip, uint16_t src_port, uint16_t dst_port,
                        const uint8_t* data, uint16_t len, void* ctx) {
    (void)src_ip; (void)src_port; (void)dst_port; (void)ctx;
    if (len < 240) return;
    const dhcp_packet_t* p = (const dhcp_packet_t*)data;
    if (p->op != BOOTREPLY) return;
    if (ntohl(p->cookie) != DHCP_MAGIC) return;

    uint32_t mask = 0, router = 0, dns = 0, server = 0, lease = 0;
    uint8_t mtype = 0;

    int optlen = len - (int)((const uint8_t*)p->options - (const uint8_t*)p);
    int i = 0;
    while (i < optlen) {
        uint8_t code = p->options[i++];
        if (code == 0xFF) break;       // end
        if (code == 0x00) continue;    // pad
        if (i >= optlen) break;
        uint8_t l = p->options[i++];
        if (i + l > optlen) break;
        const uint8_t* v = &p->options[i];
        switch (code) {
            case 53: mtype  = v[0]; break;
            case 1:  if (l >= 4) memcpy(&mask,   v, 4); break;
            case 3:  if (l >= 4) memcpy(&router, v, 4); break;
            case 6:  if (l >= 4) memcpy(&dns,    v, 4); break;
            case 54: if (l >= 4) memcpy(&server, v, 4); break;
            case 51: if (l >= 4) { uint32_t t; memcpy(&t, v, 4); lease = ntohl(t); } break;
            default: break;
        }
        i += l;
    }

    reply_type = mtype;
    reply_xid = ntohl(p->xid);
    off_yiaddr = p->yiaddr;
    off_mask = mask;
    off_router = router;
    off_dns = dns;
    off_server = server;
    off_lease = lease;
    reply_ready = true;
}

// Build a DISCOVER or REQUEST and broadcast it. requested_ip/server are set for
// REQUEST (0 for DISCOVER).
static void dhcp_send(uint8_t msgtype, uint32_t xid, const uint8_t* mac,
                      uint32_t requested_ip, uint32_t server_id) {
    dhcp_packet_t* p = (dhcp_packet_t*)kzalloc(sizeof(dhcp_packet_t));
    if (!p) return;

    p->op = BOOTREQUEST;
    p->htype = 1;   // Ethernet
    p->hlen = 6;
    p->xid = htonl(xid);
    p->flags = htons(0x8000); // ask the server to broadcast its reply
    memcpy(p->chaddr, mac, 6);
    p->cookie = htonl(DHCP_MAGIC);

    int o = 0;
    o = opt_put(p->options, o, 53, 1, &msgtype);
    if (msgtype == DHCPREQUEST) {
        if (requested_ip) o = opt_put(p->options, o, 50, 4, &requested_ip);
        if (server_id)    o = opt_put(p->options, o, 54, 4, &server_id);
    }
    uint8_t plist[4] = {1, 3, 6, 15}; // mask, router, dns, domain
    o = opt_put(p->options, o, 55, sizeof(plist), plist);
    p->options[o++] = 0xFF; // end

    int fixed = (int)((uint8_t*)p->options - (uint8_t*)p);
    udp_send_broadcast(DHCP_CLIENT_PORT, DHCP_SERVER_PORT, p, (uint16_t)(fixed + o));
    kfree(p);
}

static bool wait_reply(uint32_t xid, uint8_t want_type) {
    uint64_t t0 = pit_get_ticks();
    uint64_t wait_ticks = DHCP_TIMEOUT_MS / 10;
    while (pit_get_ticks() - t0 < wait_ticks) {
        if (reply_ready && reply_xid == xid && reply_type == want_type) return true;
        arch_halt();
    }
    return false;
}

int dhcp_request_lease(void) {
    net_if_t* nif = net_get_primary_if();
    if (!nif) return -1;

    printk(KERN_INFO "DHCP: Discovering network configuration via broadcast...\n");

    if (udp_bind(DHCP_CLIENT_PORT, dhcp_udp_cb, NULL) != 0) {
        printk(ANSI_YELLOW "DHCP: client port 68 unavailable\n" ANSI_RESET);
        return -1;
    }

    uint32_t xid = (uint32_t)(0x53554200u ^ pit_get_ticks());
    int rc = -1;

    // DISCOVER -> OFFER
    reply_ready = false;
    dhcp_send(DHCPDISCOVER, xid, nif->mac, 0, 0);
    if (wait_reply(xid, DHCPOFFER)) {
        uint32_t offered = off_yiaddr;
        uint32_t server  = off_server;
        char ips[16]; ip_to_str(offered, ips);
        printk(KERN_INFO "DHCP: OFFER received: %s\n", ips);

        // REQUEST -> ACK
        reply_ready = false;
        dhcp_send(DHCPREQUEST, xid, nif->mac, offered, server);
        if (wait_reply(xid, DHCPACK)) {
            nif->ip = off_yiaddr;
            if (off_mask)   nif->subnet  = off_mask;
            if (off_router) nif->gateway = off_router;
            if (off_dns)    nif->dns     = off_dns;

            g_lease.ip = nif->ip;
            g_lease.mask = nif->subnet;
            g_lease.gateway = nif->gateway;
            g_lease.dns = nif->dns;
            g_lease.server = off_server;
            g_lease.lease_secs = off_lease ? off_lease : 86400;
            g_lease.bound = true;

            char a[16], m[16], gw[16], d[16];
            ip_to_str(nif->ip, a); ip_to_str(nif->subnet, m);
            ip_to_str(nif->gateway, gw); ip_to_str(nif->dns, d);
            printk(ANSI_BRIGHT_GREEN
                   "DHCP: Lease granted: IP %s, Mask %s, GW %s, DNS %s (lease %us)\n"
                   ANSI_RESET, a, m, gw, d, g_lease.lease_secs);
            rc = 0;
        }
    }

    udp_unbind(DHCP_CLIENT_PORT);

    if (rc != 0) {
        char a[16]; ip_to_str(nif->ip, a);
        printk(ANSI_YELLOW "DHCP: no response; keeping static config %s\n" ANSI_RESET, a);
    }
    return rc;
}
