#include <block/block.h>
#include <block/elevator.h>
#include <lib/string.h>
#include <kernel/printk.h>

static block_device_t* registered_devices[MAX_BLOCK_DEVS];
static size_t device_count = 0;

void block_init(void) {
    memset(registered_devices, 0, sizeof(registered_devices));
    device_count = 0;
    elevator_init();
    printk(KERN_INFO "BLOCK: Generic Block Device Layer initialized\n");
}

int block_register_device(block_device_t* dev) {
    if (!dev || device_count >= MAX_BLOCK_DEVS) return -1;
    registered_devices[device_count++] = dev;
    printk(KERN_INFO "BLOCK: Registered device '%s' (%llu sectors, %u B/sec)\n",
           dev->name, dev->total_sectors, dev->sector_size);
    return 0;
}

block_device_t* block_get_device(const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < device_count; i++) {
        if (registered_devices[i] && strcmp(registered_devices[i]->name, name) == 0) {
            return registered_devices[i];
        }
    }
    return NULL;
}

int block_submit_bio(const char* dev_name, block_req_type_t type, uint64_t sector, uint32_t count, void* buffer) {
    block_device_t* dev = block_get_device(dev_name);
    if (!dev) return -1;

    switch (type) {
        case REQ_READ:
            if (!dev->read_sectors) return -1;
            return dev->read_sectors(dev, sector, count, buffer);
        case REQ_WRITE:
            if (dev->read_only || !dev->write_sectors) return -1;
            return dev->write_sectors(dev, sector, count, (const void*)buffer);
        case REQ_FLUSH:
            if (dev->flush) return dev->flush(dev);
            return 0;
    }
    return -1;
}

size_t block_get_device_count(void) {
    return device_count;
}

block_device_t* block_get_device_by_index(size_t index) {
    if (index >= device_count) return NULL;
    return registered_devices[index];
}
