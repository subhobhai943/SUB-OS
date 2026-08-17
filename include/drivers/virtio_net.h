#ifndef _DRIVERS_VIRTIO_NET_H
#define _DRIVERS_VIRTIO_NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed)) virtio_net_hdr_t;

typedef struct {
    uint8_t  mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    bool     initialized;
} virtio_net_info_t;

bool virtio_net_init(void);
bool virtio_net_is_detected(void);
void virtio_net_get_mac(uint8_t* mac_out);
int  virtio_net_send_packet(const uint8_t* packet, uint16_t length);

#endif // _DRIVERS_VIRTIO_NET_H
