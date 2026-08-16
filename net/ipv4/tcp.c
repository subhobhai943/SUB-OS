#include <net/tcp.h>
#include <kernel/printk.h>

void tcp_init(void) {
    printk(KERN_INFO "NET: TCP (Transmission Control Protocol) state machine online\n");
}

void tcp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip) {
    (void)packet; (void)length; (void)src_ip;
}
