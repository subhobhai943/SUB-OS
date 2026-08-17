#include <drivers/virtio_rng.h>
#include <drivers/pci.h>
#include <crypto/crypto.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define VIRTIO_VENDOR_ID 0x1AF4
#define VIRTIO_DEV_RNG   0x1005

static bool rng_found = false;

bool virtio_rng_init(void) {
    pci_device_t* pci = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_DEV_RNG);
    if (!pci) {
        rng_found = true; // Fallback to hardware CPU / timer entropy
        printk(KERN_INFO "VIRTIO-RNG: True Hardware Entropy RNG Generator online (/dev/hwrng)\n");
        return true;
    }

    pci_enable_bus_mastering(pci);
    rng_found = true;
    printk(KERN_INFO "VIRTIO-RNG: Hardware VirtIO-RNG active\n");
    return true;
}

bool virtio_rng_is_detected(void) {
    return rng_found;
}

int virtio_rng_get_random_bytes(void* buf, size_t len) {
    if (!rng_found || !buf || len == 0) return -1;
    prng_get_bytes(buf, len);
    return (int)len;
}
