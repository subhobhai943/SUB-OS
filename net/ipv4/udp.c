#include <net/udp.h>
#include <lib/string.h>
#include <kernel/printk.h>

void udp_init(void) {
    printk(KERN_INFO "NET: UDP (User Datagram Protocol) transport layer online\n");
}

void udp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip) {
    (void)packet; (void)length; (void)src_ip;
}

void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len) {
    uint8_t buffer[1500];
    udp_header_t* udp = (udp_header_t*)buffer;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons((uint16_t)(sizeof(udp_header_t) + len));
    udp->checksum = 0;

    if (data && len > 0) {
        memcpy(buffer + sizeof(udp_header_t), data, len);
    }

    net_send_ip(dst_ip, IP_PROTO_UDP, buffer, (uint16_t)(sizeof(udp_header_t) + len));
}
