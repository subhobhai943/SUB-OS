#include <drivers/ramdisk.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

static uint8_t* ramdisk_storage = NULL;
static block_device_t ramdisk_dev;

static int ramdisk_read_sectors(block_device_t* dev, uint64_t sector, uint32_t count, void* buf) {
    (void)dev;
    if (!ramdisk_storage || !buf) return -1;
    if ((sector + count) * RAMDISK_SECTOR_SIZE > RAMDISK_SIZE) return -1;

    memcpy(buf, ramdisk_storage + (sector * RAMDISK_SECTOR_SIZE), count * RAMDISK_SECTOR_SIZE);
    return 0;
}

static int ramdisk_write_sectors(block_device_t* dev, uint64_t sector, uint32_t count, const void* buf) {
    (void)dev;
    if (!ramdisk_storage || !buf) return -1;
    if ((sector + count) * RAMDISK_SECTOR_SIZE > RAMDISK_SIZE) return -1;

    memcpy(ramdisk_storage + (sector * RAMDISK_SECTOR_SIZE), buf, count * RAMDISK_SECTOR_SIZE);
    return 0;
}

void ramdisk_init(void) {
    ramdisk_storage = (uint8_t*)kzalloc(RAMDISK_SIZE);
    if (!ramdisk_storage) {
        printk(KERN_ERR "RAMDISK: Failed to allocate 4MB memory buffer\n");
        return;
    }

    memset(&ramdisk_dev, 0, sizeof(block_device_t));
    strcpy(ramdisk_dev.name, "ram0");
    ramdisk_dev.device_id = 1;
    ramdisk_dev.sector_size = RAMDISK_SECTOR_SIZE;
    ramdisk_dev.total_sectors = RAMDISK_SIZE / RAMDISK_SECTOR_SIZE;
    ramdisk_dev.read_only = false;
    ramdisk_dev.read_sectors = ramdisk_read_sectors;
    ramdisk_dev.write_sectors = ramdisk_write_sectors;
    ramdisk_dev.flush = NULL;

    block_register_device(&ramdisk_dev);
    printk(KERN_INFO "RAMDISK: /dev/ram0 registered (4096 KB, 8192 sectors)\n");
}

block_device_t* ramdisk_get_block_device(void) {
    return &ramdisk_dev;
}
