#ifndef _NET_NET_H
#define _NET_NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

static inline uint16_t htons(uint16_t host) {
    return (uint16_t)((host << 8) | (host >> 8));
}
static inline uint16_t ntohs(uint16_t net) {
    return htons(net);
}
static inline uint32_t htonl(uint32_t host) {
    return (uint32_t)(((host & 0x000000FF) << 24) |
                     ((host & 0x0000FF00) << 8)  |
                     ((host & 0x00FF0000) >> 8)  |
                     ((host & 0xFF000000) >> 24));
}
static inline uint32_t ntohl(uint32_t net) {
    return htonl(net);
}

typedef struct {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

typedef struct {
    uint8_t  ihl_version;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    char     name[16];
    uint8_t  mac[6];
    uint32_t ip;
    uint32_t subnet;
    uint32_t gateway;
    uint32_t dns;
    bool     is_up;
} net_if_t;

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    bool     valid;
} arp_entry_t;

void net_init(void);
void net_receive(const uint8_t* packet, uint16_t length);
void net_send_eth(const uint8_t* dst_mac, uint16_t ethertype, const void* payload, uint16_t len);
void net_send_ip(uint32_t dst_ip, uint8_t protocol, const void* payload, uint16_t len);
void net_send_arp_req(uint32_t target_ip);
int  net_ping(uint32_t target_ip, uint32_t count, uint32_t timeout_ms);

net_if_t* net_get_primary_if(void);
arp_entry_t* net_get_arp_table(int* count_out);

uint32_t ip_parse(const char* ip_str);
void ip_to_str(uint32_t ip, char* buf);

// One's-complement checksum, summed a byte at a time so the loads cannot be
// reordered around a caller's store into the buffer being summed, and so no
// alignment is assumed of a packed header. Words are read big-endian;
// net_csum_finish returns the result already in network byte order, ready to
// be assigned straight to a header's checksum field.
uint32_t net_csum_add(uint32_t sum, const void* data, size_t len);
uint16_t net_csum_finish(uint32_t sum);
uint16_t net_checksum(const void* data, size_t len);
void mac_to_str(const uint8_t* mac, char* buf);

#endif // _NET_NET_H
