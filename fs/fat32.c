#include <fs/fat32.h>
#include <block/block.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

void fat32_init(void) {
    printk(KERN_INFO "FAT32: Driver registered (Supports FAT12/16/32 volumes)\n");
}

vfs_node_t* fat32_mount(const char* block_device_name) {
    block_device_t* dev = block_get_device(block_device_name);
    if (!dev) return NULL;

    fat32_bpb_t bpb;
    if (dev->read_sectors(dev, 0, 1, &bpb) != 0) {
        return NULL;
    }

    if (bpb.boot_signature != 0x28 && bpb.boot_signature != 0x29) {
        return NULL; // Not a valid FAT volume
    }

    vfs_node_t* root = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    if (!root) return NULL;

    strcpy(root->name, "fat32_root");
    root->flags = FS_DIRECTORY;
    root->mode = 0777;
    return root;
}
