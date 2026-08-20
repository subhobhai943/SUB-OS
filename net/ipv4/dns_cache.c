// DNS Resolver Cache with TTL Management and Metrics for SUB-OS
#include <net/dns_cache.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

static dns_cache_entry_t dns_table[DNS_CACHE_MAX_ENTRIES];
static uint64_t total_lookups = 0;
static uint64_t total_hits = 0;

void dns_cache_init(void) {
    memset(dns_table, 0, sizeof(dns_table));
    // Seed common local and well-known hosts
    dns_cache_insert("localhost", 0x7F000001, 86400);            // 127.0.0.1
    dns_cache_insert("gateway.local", 0x0A000202, 3600);         // 10.0.2.2
    dns_cache_insert("dns.google", 0x08080808, 7200);            // 8.8.8.8
    dns_cache_insert("one.one.one.one", 0x01010101, 7200);       // 1.1.1.1
    dns_cache_insert("github.com", 0x8C527964, 300);             // 140.82.121.4

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
    dns_cache_insert("localhost", 0x7F000001, 86400);
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
            uint32_t ip = dns_table[i].ip;
            char ip_str[20];
            snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
                     (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                     (ip >> 8) & 0xFF, ip & 0xFF);

            printk("%-28s  " ANSI_BRIGHT_YELLOW "%-16s" ANSI_RESET "  %8u  %8llu\n",
                   dns_table[i].hostname, ip_str,
                   dns_table[i].ttl_remaining, dns_table[i].hit_count);
        }
    }
    printk("\n");
}
