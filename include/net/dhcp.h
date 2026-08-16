#ifndef _NET_DHCP_H
#define _NET_DHCP_H

#include <stdint.h>
#include <stdbool.h>

void dhcp_init(void);
int  dhcp_request_lease(void);

#endif // _NET_DHCP_H
