#ifndef _NET_DNS_H
#define _NET_DNS_H

#include <stdint.h>

void dns_init(void);
uint32_t dns_resolve(const char* hostname);

#endif // _NET_DNS_H
