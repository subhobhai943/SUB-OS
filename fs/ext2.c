#include <fs/ext2.h>
#include <block/block.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

void ext2_init(void) {
    printk(KERN_INFO "EXT2: Second Extended Filesystem driver registered (Rev 0/1)\n");
}

vfs_node_t* ext2_mount(const char* block_device_name) {
    block_device_t* dev = block_get_device(block_device_name);
    if (!dev) return NULL;

    uint8_t buffer[1024];
    // Superblock is at offset 1024 (LBA 2)
    if (dev->read_sectors(dev, 2, 2, buffer) != 0) {
        return NULL;
    }

    ext2_superblock_t* sb = (ext2_superblock_t*)buffer;
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        return NULL; // Not an ext2 filesystem
    }

    vfs_node_t* root = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    if (!root) return NULL;

    strcpy(root->name, "ext2_root");
    root->flags = FS_DIRECTORY;
    root->inode = 2; // ext2 root inode is always 2
    root->mode = 0755;
    return root;
}
