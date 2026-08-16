#ifndef _DRIVERS_E1000_H
#define _DRIVERS_E1000_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define E1000_VENDOR_ID   0x8086
#define E1000_DEV_82540EM 0x100E
#define E1000_DEV_82545EM 0x100F

bool e1000_init(void);
void e1000_send(const void* data, uint16_t len);
void e1000_get_mac(uint8_t* mac_out);
bool e1000_is_link_up(void);

uint64_t e1000_get_rx_packets(void);
uint64_t e1000_get_tx_packets(void);
uint64_t e1000_get_rx_bytes(void);
uint64_t e1000_get_tx_bytes(void);

#endif // _DRIVERS_E1000_H
