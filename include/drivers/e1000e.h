#ifndef _DRIVERS_E1000E_H
#define _DRIVERS_E1000E_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define E1000E_VENDOR_INTEL 0x8086
#define E1000E_DEV_82574L   0x10D3
#define E1000E_DEV_82567LM  0x10F5
#define E1000E_DEV_I217     0x153A
#define E1000E_DEV_I219     0x156F

void e1000e_init(void);
int e1000e_send(const void* packet, uint16_t length);
int e1000e_poll(void* buffer, uint16_t max_len);
void e1000e_get_mac(uint8_t* out_mac);
bool e1000e_is_online(void);
void e1000e_dump_stats(void);

#endif // _DRIVERS_E1000E_H
