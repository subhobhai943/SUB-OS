#ifndef _NET_DHCP_H
#define _NET_DHCP_H

#include <stdint.h>
#include <stdbool.h>

void dhcp_init(void);

// Run a full DHCP handshake (DISCOVER/OFFER/REQUEST/ACK) and, on success, apply
// the offered IP, mask, gateway and DNS to the primary interface. Returns 0 on
// a granted lease, -1 on timeout (the static configuration is left in place).
int  dhcp_request_lease(void);

// Last-lease details for status reporting (valid after a successful lease).
typedef struct {
    uint32_t ip;
    uint32_t mask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t server;
    uint32_t lease_secs;
    bool     bound;
} dhcp_lease_t;

const dhcp_lease_t* dhcp_get_lease(void);

#endif // _NET_DHCP_H
