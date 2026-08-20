#ifndef _NET_DNS_CACHE_H
#define _NET_DNS_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DNS_CACHE_MAX_ENTRIES 32
#define DNS_CACHE_MAX_HOST    64

typedef struct {
    char hostname[DNS_CACHE_MAX_HOST];
    uint32_t ip;
    uint32_t ttl_remaining;
    uint64_t hit_count;
    bool active;
} dns_cache_entry_t;

void dns_cache_init(void);
bool dns_cache_lookup(const char* hostname, uint32_t* out_ip);
void dns_cache_insert(const char* hostname, uint32_t ip, uint32_t ttl);
void dns_cache_flush(void);
void dns_cache_dump(void);

#endif // _NET_DNS_CACHE_H
