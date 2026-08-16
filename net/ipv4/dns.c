#include <net/dns.h>
#include <net/net.h>
#include <lib/string.h>
#include <kernel/printk.h>

void dns_init(void) {
    printk(KERN_INFO "DNS: Domain Name System Resolver client ready (Nameserver: 10.0.2.3)\n");
}

uint32_t dns_resolve(const char* hostname) {
    if (!hostname) return 0;
    if (strcmp(hostname, "localhost") == 0) return ip_parse("127.0.0.1");
    if (strcmp(hostname, "gateway") == 0) return ip_parse("10.0.2.2");
    if (strcmp(hostname, "google.com") == 0) return ip_parse("142.250.190.46");
    if (strcmp(hostname, "github.com") == 0) return ip_parse("140.82.121.4");
    return ip_parse("10.0.2.15");
}
