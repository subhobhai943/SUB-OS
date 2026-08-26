// DNS (Domain Name System) stub resolver for SUB-OS.
//
// Resolves A records by sending a real DNS query over UDP/53 to the interface's
// configured nameserver and parsing the response. Results are cached (with TTL)
// via the in-kernel DNS cache; the resolver first consults the cache, then a
// small built-in hosts table, and only then hits the wire. Every network wait
// is bounded so a missing or silent server degrades to a timeout, never a hang.
#include <net/dns.h>
#include <net/net.h>
#include <net/udp.h>
#include <net/dns_cache.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>
#include <arch/arch.h>

#define DNS_PORT       53
#define DNS_TIMEOUT_MS 2000
#define DNS_MAX_MSG    512

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

// Filled from UDP receive context when a matching reply arrives.
static volatile bool     dns_reply_ready = false;
static volatile uint16_t dns_reply_id = 0;
static volatile uint32_t dns_reply_ip = 0;
static volatile uint32_t dns_reply_ttl = 0;

void dns_init(void) {
    dns_reply_ready = false;
    printk(KERN_INFO "DNS: Domain Name System Resolver client ready (Nameserver: 10.0.2.3)\n");
}

// Encode "www.example.com" as DNS labels: 3www7example3com0.
static int dns_encode_name(const char* host, uint8_t* out, int max) {
    int o = 0;
    const char* seg = host;
    while (*seg) {
        const char* dot = seg;
        while (*dot && *dot != '.') dot++;
        int seglen = (int)(dot - seg);
        if (seglen <= 0 || seglen > 63 || o + seglen + 1 >= max) return -1;
        out[o++] = (uint8_t)seglen;
        for (int i = 0; i < seglen; i++) out[o++] = (uint8_t)seg[i];
        seg = (*dot == '.') ? dot + 1 : dot;
    }
    if (o + 1 >= max) return -1;
    out[o++] = 0; // root label
    return o;
}

// Skip a (possibly compressed) name in the answer section, returning the offset
// just past it. Compression pointers (0xC0) end the name in two bytes.
static int dns_skip_name(const uint8_t* msg, int off, int len) {
    while (off < len) {
        uint8_t b = msg[off];
        if (b == 0) return off + 1;
        if ((b & 0xC0) == 0xC0) return off + 2;
        off += b + 1;
    }
    return off;
}

static void dns_udp_cb(uint32_t src_ip, uint16_t src_port, uint16_t dst_port,
                       const uint8_t* data, uint16_t len, void* ctx) {
    (void)src_ip; (void)src_port; (void)dst_port; (void)ctx;
    if (len < sizeof(dns_header_t)) return;

    const dns_header_t* h = (const dns_header_t*)data;
    uint16_t id = ntohs(h->id);
    uint16_t ancount = ntohs(h->ancount);
    uint16_t qdcount = ntohs(h->qdcount);
    if (ancount == 0) { dns_reply_id = id; dns_reply_ready = true; return; }

    int off = sizeof(dns_header_t);
    // Skip the echoed question(s): name + qtype(2) + qclass(2).
    for (uint16_t q = 0; q < qdcount; q++) {
        off = dns_skip_name(data, off, len);
        off += 4;
    }

    for (uint16_t a = 0; a < ancount && off + 12 <= len; a++) {
        off = dns_skip_name(data, off, len);
        if (off + 10 > len) break;
        uint16_t type = ntohs(*(const uint16_t*)(data + off));
        uint32_t ttl  = ntohl(*(const uint32_t*)(data + off + 4));
        uint16_t rdlen = ntohs(*(const uint16_t*)(data + off + 8));
        off += 10;
        if (off + rdlen > len) break;
        if (type == 1 && rdlen == 4) { // A record
            uint32_t ip;
            memcpy(&ip, data + off, 4); // already network byte order == our storage
            dns_reply_ip = ip;
            dns_reply_ttl = ttl ? ttl : 300;
            dns_reply_id = id;
            dns_reply_ready = true;
            return;
        }
        off += rdlen;
    }
    dns_reply_id = id;
    dns_reply_ready = true; // answered, but no A record found
}

// Built-in hosts fallback for when the wire query yields nothing.
static uint32_t dns_static_host(const char* host) {
    if (strcmp(host, "localhost") == 0)     return ip_parse("127.0.0.1");
    if (strcmp(host, "gateway") == 0)       return ip_parse("10.0.2.2");
    if (strcmp(host, "dns.google") == 0)    return 0x08080808;
    if (strcmp(host, "google.com") == 0)    return ip_parse("142.250.190.46");
    if (strcmp(host, "github.com") == 0)    return ip_parse("140.82.121.4");
    return 0;
}

uint32_t dns_resolve(const char* hostname) {
    if (!hostname || !*hostname) return 0;

    // Dotted-quad literal: no lookup needed.
    {
        bool numeric = true;
        for (const char* p = hostname; *p; p++) {
            if (!((*p >= '0' && *p <= '9') || *p == '.')) { numeric = false; break; }
        }
        if (numeric) return ip_parse(hostname);
    }

    // 1) Cache.
    uint32_t cached;
    if (dns_cache_lookup(hostname, &cached)) return cached;

    // 2) Wire query.
    net_if_t* nif = net_get_primary_if();
    uint32_t server = (nif && nif->dns) ? nif->dns : ip_parse("10.0.2.3");
    uint16_t sport = udp_alloc_ephemeral_port();
    if (sport && udp_bind(sport, dns_udp_cb, NULL) == 0) {
        uint8_t query[DNS_MAX_MSG];
        dns_header_t* h = (dns_header_t*)query;
        uint16_t qid = (uint16_t)(0x5300u ^ (pit_get_ticks() & 0xFF));
        h->id = htons(qid);
        h->flags = htons(0x0100); // standard query, recursion desired
        h->qdcount = htons(1);
        h->ancount = 0; h->nscount = 0; h->arcount = 0;

        int off = sizeof(dns_header_t);
        int nl = dns_encode_name(hostname, query + off, DNS_MAX_MSG - off - 4);
        if (nl > 0) {
            off += nl;
            *(uint16_t*)(query + off) = htons(1); off += 2; // QTYPE A
            *(uint16_t*)(query + off) = htons(1); off += 2; // QCLASS IN

            dns_reply_ready = false;
            dns_reply_ip = 0;
            uint64_t t0 = pit_get_ticks();
            udp_send(server, sport, DNS_PORT, query, (uint16_t)off);

            uint64_t wait_ticks = DNS_TIMEOUT_MS / 10;
            while (!dns_reply_ready && (pit_get_ticks() - t0 < wait_ticks)) {
                arch_halt();
            }

            if (dns_reply_ready && dns_reply_id == qid && dns_reply_ip != 0) {
                uint32_t ip = dns_reply_ip;
                udp_unbind(sport);
                dns_cache_insert(hostname, ip, dns_reply_ttl);
                return ip;
            }
        }
        udp_unbind(sport);
    }

    // 3) Static hosts fallback.
    uint32_t st = dns_static_host(hostname);
    if (st) { dns_cache_insert(hostname, st, 300); return st; }
    return 0;
}
