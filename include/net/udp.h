#ifndef _NET_UDP_H
#define _NET_UDP_H

#include <net/net.h>

// A bound UDP port dispatches every matching datagram to this callback. It runs
// in soft-IRQ / receive context, so handlers must be quick and non-blocking:
// stash the datagram and set a completion flag, then process it from a thread.
typedef void (*udp_handler_t)(uint32_t src_ip, uint16_t src_port,
                              uint16_t dst_port, const uint8_t* data,
                              uint16_t len, void* ctx);

void udp_init(void);

// Demultiplex an inbound IPv4 payload (protocol 17) to a bound port.
void udp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip);

// Emit a datagram to dst_ip:dst_port from src_port (checksummed).
void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void* data, uint16_t len);

// Emit a datagram to the limited broadcast address (255.255.255.255) from
// source IP 0.0.0.0 — used before the interface has an address (DHCP).
void udp_send_broadcast(uint16_t src_port, uint16_t dst_port,
                        const void* data, uint16_t len);

// Register/unregister a receive handler for a local port. udp_bind returns 0 on
// success, -1 if the port is taken or the table is full.
int  udp_bind(uint16_t port, udp_handler_t handler, void* ctx);
void udp_unbind(uint16_t port);

// Allocate an unused ephemeral port in the 49152-65535 range.
uint16_t udp_alloc_ephemeral_port(void);

// Counters: datagrams received, transmitted, and dropped (no listener / bad).
void udp_get_stats(uint64_t* rx, uint64_t* tx, uint64_t* drop);
int  udp_get_bindings(uint16_t* ports_out, int max);

#endif // _NET_UDP_H
