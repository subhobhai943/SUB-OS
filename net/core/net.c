#include <net/net.h>
#include <net/tcp.h>
#include <net/filter.h>
#include <drivers/e1000.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <mm/kmalloc.h>
#include <arch/arch.h>
#include <kernel/printk.h>

#define MAX_ARP_ENTRIES 16

static net_if_t primary_if;
static arp_entry_t arp_cache[MAX_ARP_ENTRIES];
static int arp_cache_count = 0;

static volatile bool ping_received = false;
static volatile uint16_t ping_id = 0;
static volatile uint16_t ping_seq = 0;
static volatile uint64_t ping_time = 0;

static const uint8_t eth_broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint16_t net_checksum(const void* data, size_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(const uint8_t*)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

void ip_to_str(uint32_t ip, char* buf) {
    uint8_t* p = (uint8_t*)&ip;
    sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
}

void mac_to_str(const uint8_t* mac, char* buf) {
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

uint32_t ip_parse(const char* ip_str) {
    if (!ip_str) return 0;
    uint32_t parts[4] = {0, 0, 0, 0};
    int idx = 0;

    while (*ip_str && idx < 4) {
        if (*ip_str >= '0' && *ip_str <= '9') {
            parts[idx] = parts[idx] * 10 + (*ip_str - '0');
        } else if (*ip_str == '.') {
            idx++;
        }
        ip_str++;
    }
    if (idx != 3) return 0;
    return parts[0] | (parts[1] << 8) | (parts[2] << 16) | (parts[3] << 24);
}

static void arp_cache_insert(uint32_t ip, const uint8_t* mac) {
    for (int i = 0; i < arp_cache_count; i++) {
        if (arp_cache[i].ip == ip) {
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = true;
            return;
        }
    }
    if (arp_cache_count < MAX_ARP_ENTRIES) {
        arp_cache[arp_cache_count].ip = ip;
        memcpy(arp_cache[arp_cache_count].mac, mac, 6);
        arp_cache[arp_cache_count].valid = true;
        arp_cache_count++;
    } else {
        arp_cache[0].ip = ip;
        memcpy(arp_cache[0].mac, mac, 6);
        arp_cache[0].valid = true;
    }
}

static bool arp_cache_lookup(uint32_t ip, uint8_t* mac_out) {
    for (int i = 0; i < arp_cache_count; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac_out, arp_cache[i].mac, 6);
            return true;
        }
    }
    return false;
}

void net_send_eth(const uint8_t* dst_mac, uint16_t ethertype, const void* payload, uint16_t len) {
    uint16_t total_len = sizeof(eth_header_t) + len;
    if (total_len < 60) total_len = 60; // Minimum Ethernet frame

    uint8_t* frame = (uint8_t*)kzalloc(total_len);
    if (!frame) return;

    eth_header_t* eth = (eth_header_t*)frame;
    memcpy(eth->dst_mac, dst_mac, 6);
    memcpy(eth->src_mac, primary_if.mac, 6);
    eth->ethertype = htons(ethertype);

    if (payload && len > 0) {
        memcpy(frame + sizeof(eth_header_t), payload, len);
    }

    e1000_send(frame, total_len);
    kfree(frame);
}

void net_send_arp_req(uint32_t target_ip) {
    arp_header_t arp;
    arp.htype = htons(1);
    arp.ptype = htons(ETHERTYPE_IPV4);
    arp.hlen = 6;
    arp.plen = 4;
    arp.opcode = htons(1); // ARP Request
    memcpy(arp.sender_mac, primary_if.mac, 6);
    arp.sender_ip = primary_if.ip;
    memset(arp.target_mac, 0, 6);
    arp.target_ip = target_ip;

    net_send_eth(eth_broadcast, ETHERTYPE_ARP, &arp, sizeof(arp_header_t));
}

void net_send_ip(uint32_t dst_ip, uint8_t protocol, const void* payload, uint16_t len) {
    uint8_t dst_mac[6];
    uint32_t lookup_ip = dst_ip;

    if ((dst_ip & primary_if.subnet) != (primary_if.ip & primary_if.subnet)) {
        lookup_ip = primary_if.gateway;
    }

    if (!arp_cache_lookup(lookup_ip, dst_mac)) {
        net_send_arp_req(lookup_ip);
        memcpy(dst_mac, eth_broadcast, 6);
    }

    uint16_t ip_len = sizeof(ip_header_t) + len;
    uint8_t* buf = (uint8_t*)kmalloc(ip_len);
    if (!buf) return;

    ip_header_t* ip = (ip_header_t*)buf;
    ip->ihl_version = 0x45;
    ip->tos = 0;
    ip->total_length = htons(ip_len);
    ip->id = htons((uint16_t)pit_get_ticks());
    ip->flags_fragment = htons(0x4000);
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = primary_if.ip;
    ip->dst_ip = dst_ip;

    ip->checksum = net_checksum(ip, sizeof(ip_header_t));

    if (payload && len > 0) {
        memcpy(buf + sizeof(ip_header_t), payload, len);
    }

    net_send_eth(dst_mac, ETHERTYPE_IPV4, buf, ip_len);
    kfree(buf);
}

static void net_handle_arp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(arp_header_t)) return;
    const arp_header_t* arp = (const arp_header_t*)data;

    uint16_t op = ntohs(arp->opcode);
    arp_cache_insert(arp->sender_ip, arp->sender_mac);

    if (op == 1) { // Request for our IP
        if (arp->target_ip == primary_if.ip) {
            arp_header_t reply;
            reply.htype = htons(1);
            reply.ptype = htons(ETHERTYPE_IPV4);
            reply.hlen = 6;
            reply.plen = 4;
            reply.opcode = htons(2); // Reply
            memcpy(reply.sender_mac, primary_if.mac, 6);
            reply.sender_ip = primary_if.ip;
            memcpy(reply.target_mac, arp->sender_mac, 6);
            reply.target_ip = arp->sender_ip;

            net_send_eth(arp->sender_mac, ETHERTYPE_ARP, &reply, sizeof(arp_header_t));
        }
    }
}

static void net_handle_icmp(uint32_t src_ip, const uint8_t* data, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;
    const icmp_header_t* icmp = (const icmp_header_t*)data;

    if (icmp->type == 8) { // Echo Request -> Auto-Reply
        uint8_t* rep = (uint8_t*)kmalloc(len);
        if (!rep) return;
        memcpy(rep, data, len);

        icmp_header_t* rep_icmp = (icmp_header_t*)rep;
        rep_icmp->type = 0; // Echo Reply
        rep_icmp->code = 0;
        rep_icmp->checksum = 0;
        rep_icmp->checksum = net_checksum(rep, len);

        net_send_ip(src_ip, IP_PROTO_ICMP, rep, len);
        kfree(rep);
    } else if (icmp->type == 0) { // Echo Reply
        ping_id = ntohs(icmp->id);
        ping_seq = ntohs(icmp->sequence);
        ping_time = pit_get_ticks();
        ping_received = true;
    }
}

void net_receive(const uint8_t* packet, uint16_t length) {
    if (!packet || length < sizeof(eth_header_t)) return;

    const eth_header_t* eth = (const eth_header_t*)packet;
    uint16_t type = ntohs(eth->ethertype);

    const uint8_t* payload = packet + sizeof(eth_header_t);
    uint16_t payload_len = length - sizeof(eth_header_t);

    if (type == ETHERTYPE_ARP) {
        net_handle_arp(payload, payload_len);
    } else if (type == ETHERTYPE_IPV4) {
        if (payload_len >= sizeof(ip_header_t)) {
            const ip_header_t* ip = (const ip_header_t*)payload;
            uint8_t ihl = (ip->ihl_version & 0x0F) * 4;
            if (payload_len >= ihl) {
                // Dynamically learn sender's MAC address in ARP cache
                arp_cache_insert(ip->src_ip, eth->src_mac);

                // NetFilter inspection hook
                if (filter_evaluate(FILTER_HOOK_LOCAL_IN, ip->protocol, ip->src_ip, ip->dst_ip, 0, 0, payload_len) == FILTER_ACTION_DROP) {
                    return; // Dropped by firewall
                }

                if (ip->protocol == IP_PROTO_ICMP) {
                    net_handle_icmp(ip->src_ip, payload + ihl, payload_len - ihl);
                } else if (ip->protocol == IP_PROTO_TCP) {
                    tcp_receive(payload + ihl, payload_len - ihl, ip->src_ip);
                }
            }
        }
    }
}

int net_ping(uint32_t target_ip, uint32_t count, uint32_t timeout_ms) {
    char target_str[16];
    ip_to_str(target_ip, target_str);
    printk(KERN_INFO "PING %s (56 data bytes):\n", target_str);

    int received = 0;
    uint16_t p_id = 0x5355; // 'SU'

    for (uint32_t seq = 1; seq <= count; seq++) {
        ping_received = false;
        uint64_t t_start = pit_get_ticks();

        const char* msg = "SUB-OS ICMP 2026";
        uint16_t msg_len = (uint16_t)strlen(msg);
        uint16_t icmp_sz = sizeof(icmp_header_t) + msg_len;

        uint8_t* ibuf = (uint8_t*)kmalloc(icmp_sz);
        if (ibuf) {
            icmp_header_t* icmp = (icmp_header_t*)ibuf;
            icmp->type = 8;
            icmp->code = 0;
            icmp->checksum = 0;
            icmp->id = htons(p_id);
            icmp->sequence = htons((uint16_t)seq);
            memcpy(ibuf + sizeof(icmp_header_t), msg, msg_len);
            icmp->checksum = net_checksum(ibuf, icmp_sz);

            net_send_ip(target_ip, IP_PROTO_ICMP, ibuf, icmp_sz);
            kfree(ibuf);
        }

        uint64_t wait_ticks = timeout_ms / 10;
        if (wait_ticks == 0) wait_ticks = 100;

        while (!ping_received && (pit_get_ticks() - t_start < wait_ticks)) {
            arch_halt();
        }

        if (ping_received && ping_id == p_id && ping_seq == seq) {
            uint64_t elapsed_ms = (ping_time - t_start) * 10;
            printk(KERN_INFO "64 bytes from %s: icmp_seq=%u ttl=64 time=%llu ms\n", target_str, seq, elapsed_ms);
            received++;
        } else {
            printk(KERN_INFO "Request timeout for icmp_seq %u\n", seq);
        }

        pit_sleep(400);
    }

    printk(KERN_INFO "--- %s ping statistics ---\n", target_str);
    printk(KERN_INFO "%u packets transmitted, %d received, %u%% packet loss\n",
           count, received, ((count - received) * 100) / count);
    return received;
}

void net_init(void) {
    strcpy(primary_if.name, "eth0");
    e1000_get_mac(primary_if.mac);

    primary_if.ip      = ip_parse("10.0.2.15");
    primary_if.gateway = ip_parse("10.0.2.2");
    primary_if.subnet  = ip_parse("255.255.255.0");
    primary_if.dns     = ip_parse("10.0.2.3");
    primary_if.is_up   = true;

    // Seed gateway in ARP cache (QEMU Virtual Router MAC)
    uint8_t qemu_gw[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};
    arp_cache_insert(primary_if.gateway, qemu_gw);

    printk(KERN_INFO "NET: eth0 configured (IP: 10.0.2.15, GW: 10.0.2.2, Mask: 255.255.255.0)\n");
}

net_if_t* net_get_primary_if(void) {
    return &primary_if;
}

arp_entry_t* net_get_arp_table(int* count_out) {
    if (count_out) *count_out = arp_cache_count;
    return arp_cache;
}
