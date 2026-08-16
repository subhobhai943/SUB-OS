#ifndef _VIRT_VIRTIO_H
#define _VIRT_VIRTIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VIRTIO_PCI_VENDOR 0x1AF4

/* VirtIO Device IDs */
#define VIRTIO_ID_NET        1
#define VIRTIO_ID_BLOCK      2
#define VIRTIO_ID_CONSOLE    3
#define VIRTIO_ID_ENTROPY    4
#define VIRTIO_ID_BALLOON    5
#define VIRTIO_ID_IOMEMORY   6
#define VIRTIO_ID_RPMSG      7
#define VIRTIO_ID_SCSI       8
#define VIRTIO_ID_9P         9
#define VIRTIO_ID_GPU        16
#define VIRTIO_ID_INPUT      18

typedef struct virtio_device {
    uint16_t device_id;
    uint32_t io_base;
    uint8_t  irq;
    uint8_t  status;
    char name[32];
    bool active;
} virtio_device_t;

void virtio_init(void);
int virtio_register_device(uint16_t dev_id, uint32_t io_base, uint8_t irq);
size_t virtio_get_device_count(void);
const virtio_device_t* virtio_get_device(size_t index);

#endif // _VIRT_VIRTIO_H
