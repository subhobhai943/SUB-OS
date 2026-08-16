#include <virt/virtio.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_VIRTIO_DEVS 8

static virtio_device_t virtio_devices[MAX_VIRTIO_DEVS];
static size_t virtio_count = 0;

void virtio_init(void) {
    memset(virtio_devices, 0, sizeof(virtio_devices));
    virtio_count = 0;
    printk(KERN_INFO "VIRTIO: Subsystem initialized (Ready for VirtIO devices)\n");
}

int virtio_register_device(uint16_t dev_id, uint32_t io_base, uint8_t irq) {
    if (virtio_count >= MAX_VIRTIO_DEVS) return -1;

    virtio_device_t* dev = &virtio_devices[virtio_count];
    dev->device_id = dev_id;
    dev->io_base   = io_base;
    dev->irq       = irq;
    dev->status    = 1; // Acknowledge
    dev->active    = true;

    switch (dev_id) {
        case VIRTIO_ID_NET:     strcpy(dev->name, "VirtIO Net Device"); break;
        case VIRTIO_ID_BLOCK:   strcpy(dev->name, "VirtIO Block Storage"); break;
        case VIRTIO_ID_CONSOLE: strcpy(dev->name, "VirtIO Console"); break;
        case VIRTIO_ID_ENTROPY: strcpy(dev->name, "VirtIO RNG Entropy"); break;
        case VIRTIO_ID_BALLOON: strcpy(dev->name, "VirtIO Memory Balloon"); break;
        case VIRTIO_ID_GPU:     strcpy(dev->name, "VirtIO 2D/3D GPU"); break;
        default:                strcpy(dev->name, "VirtIO Unknown Device"); break;
    }

    printk(KERN_INFO "VIRTIO: Registered %s at IO 0x%x (IRQ %d)\n", dev->name, io_base, irq);
    virtio_count++;
    return 0;
}

size_t virtio_get_device_count(void) {
    return virtio_count;
}

const virtio_device_t* virtio_get_device(size_t index) {
    if (index >= virtio_count) return NULL;
    return &virtio_devices[index];
}
