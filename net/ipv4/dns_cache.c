// DNS Resolver Cache with TTL Management and Metrics for SUB-OS.
//
// IPs are stored in the same convention as the rest of the network stack:
// network byte order in memory (the first octet in the lowest byte), exactly as
// produced by ip_parse() and consumed by ip_to_str(). Storing them any other
// way would hand dns_resolve() callers a byte-swapped address.
#include <net/dns_cache.h>
#include <net/net.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

static dns_cache_entry_t dns_table[DNS_CACHE_MAX_ENTRIES];
static uint64_t total_lookups = 0;
static uint64_t total_hits = 0;

void dns_cache_init(void) {
    memset(dns_table, 0, sizeof(dns_table));
    // Seed common local and well-known hosts (network byte order via ip_parse).
    dns_cache_insert("localhost",       ip_parse("127.0.0.1"),     86400);
    dns_cache_insert("gateway.local",   ip_parse("10.0.2.2"),      3600);
    dns_cache_insert("dns.google",      ip_parse("8.8.8.8"),       7200);
    dns_cache_insert("one.one.one.one", ip_parse("1.1.1.1"),       7200);
    dns_cache_insert("github.com",      ip_parse("140.82.121.4"),  300);

    printk(KERN_INFO "DNS: High-performance DNS Cache table initialized (Capacity: %d)\n", DNS_CACHE_MAX_ENTRIES);
}

bool dns_cache_lookup(const char* hostname, uint32_t* out_ip) {
    if (!hostname || !out_ip) return false;
    total_lookups++;

    for (int i = 0; i < DNS_CACHE_MAX_ENTRIES; i++) {
        if (dns_table[i].active && strcmp(dns_table[i].hostname, hostname) == 0) {
            dns_table[i].hit_count++;
            total_hits++;
            *out_ip = dns_table[i].ip;
            return true;
        }
    }
    return false;
}

void dns_cache_insert(const char* hostname, uint32_t ip, uint32_t ttl) {
    if (!hostname) return;

    // Update existing entry if present
    for (int i = 0; i < DNS_CACHE_MAX_ENTRIES; i++) {
        if (dns_table[i].active && strcmp(dns_table[i].hostname, hostname) == 0) {
            dns_table[i].ip = ip;
            dns_table[i].ttl_remaining = ttl;
            return;
        }
    }

    // Insert in first free slot
    for (int i = 0; i < DNS_CACHE_MAX_ENTRIES; i++) {
        if (!dns_table[i].active) {
            strncpy(dns_table[i].hostname, hostname, DNS_CACHE_MAX_HOST - 1);
            dns_table[i].hostname[DNS_CACHE_MAX_HOST - 1] = '\0';
            dns_table[i].ip = ip;
            dns_table[i].ttl_remaining = ttl ? ttl : 300;
            dns_table[i].hit_count = 0;
            dns_table[i].active = true;
            return;
        }
    }
}

void dns_cache_flush(void) {
    memset(dns_table, 0, sizeof(dns_table));
    dns_cache_insert("localhost", ip_parse("127.0.0.1"), 86400);
    printk(ANSI_YELLOW "DNS: Resolver cache flushed.\n" ANSI_RESET);
}

void dns_cache_dump(void) {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS In-Kernel DNS Cache & Metrics ===\n" ANSI_RESET);
    uint32_t hit_ratio = (total_lookups > 0) ? (uint32_t)((total_hits * 100) / total_lookups) : 0;
    printk("  Total Lookups: " ANSI_YELLOW "%llu" ANSI_RESET "   Hits: " ANSI_BRIGHT_GREEN "%llu" ANSI_RESET "   Hit Ratio: " ANSI_BRIGHT_GREEN "%u%%\n" ANSI_RESET,
           total_lookups, total_hits, hit_ratio);
    printk("-----------------------------------------------------------------\n");
    printk(ANSI_BOLD "%-28s  %-16s  %8s  %8s\n" ANSI_RESET, "HOSTNAME", "IP ADDRESS", "TTL (s)", "HITS");
    printk("-----------------------------------------------------------------\n");

    for (int i = 0; i < DNS_CACHE_MAX_ENTRIES; i++) {
        if (dns_table[i].active) {
            char ip_str[20];
            ip_to_str(dns_table[i].ip, ip_str);

            printk("%-28s  " ANSI_BRIGHT_YELLOW "%-16s" ANSI_RESET "  %8u  %8llu\n",
                   dns_table[i].hostname, ip_str,
                   dns_table[i].ttl_remaining, dns_table[i].hit_count);
        }
    }
    printk("\n");
}
