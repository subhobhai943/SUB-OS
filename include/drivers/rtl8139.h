#ifndef _DRIVERS_RTL8139_H
#define _DRIVERS_RTL8139_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define RTL8139_REG_MAC0   0x00
#define RTL8139_REG_MAR0   0x08
#define RTL8139_REG_TSD0   0x10
#define RTL8139_REG_TSAD0  0x20
#define RTL8139_REG_RBSTART 0x30
#define RTL8139_REG_CR     0x37
#define RTL8139_REG_CAPR   0x38
#define RTL8139_REG_CBR    0x3A
#define RTL8139_REG_IMR    0x3C
#define RTL8139_REG_ISR    0x3E
#define RTL8139_REG_TCR    0x40
#define RTL8139_REG_RCR    0x44

typedef struct {
    uint8_t  mac[6];
    uint16_t io_base;
    uint32_t rx_buffer_phys;
    uint8_t* rx_buffer;
    uint32_t tx_buffer_phys[4];
    uint8_t* tx_buffers[4];
    uint8_t  tx_cur;
    bool     initialized;
} rtl8139_device_t;

bool rtl8139_init(void);
bool rtl8139_is_detected(void);
void rtl8139_get_mac(uint8_t* mac_out);
int  rtl8139_send_packet(const uint8_t* packet, uint16_t length);
void rtl8139_handle_interrupt(void);

#endif // _DRIVERS_RTL8139_H
