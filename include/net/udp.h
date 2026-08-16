#ifndef _NET_UDP_H
#define _NET_UDP_H

#include <net/net.h>

void udp_init(void);
void udp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip);
void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len);

#endif // _NET_UDP_H
