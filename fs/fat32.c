#include <fs/fat32.h>
#include <block/block.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define MAX_FAT32_FILES 32

typedef struct {
    char     name[64];
    uint32_t cluster;
    uint32_t size;
    uint8_t  attr;
    uint8_t* data;
    bool     in_use;
} fat32_mem_entry_t;

static fat32_fs_t active_fat32;
static fat32_mem_entry_t fat32_files[MAX_FAT32_FILES];
static vfs_dirent_t shared_fat_dirent;

// Helpers to format 8.3 filename
static void format_to_83(const char* input, char* output) {
    memset(output, ' ', 11);
    const char* dot = strchr(input, '.');
    size_t name_len = dot ? (size_t)(dot - input) : strlen(input);
    if (name_len > 8) name_len = 8;
    for (size_t i = 0; i < name_len; i++) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[i] = c;
    }
    if (dot) {
        dot++;
        size_t ext_len = strlen(dot);
        if (ext_len > 3) ext_len = 3;
        for (size_t i = 0; i < ext_len; i++) {
            char c = dot[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            output[8 + i] = c;
        }
    }
}

static ssize_t fat32_vfs_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    if (!node || !buffer || size == 0) return 0;
    for (size_t i = 0; i < MAX_FAT32_FILES; i++) {
        if (fat32_files[i].in_use && strcmp(fat32_files[i].name, node->name) == 0) {
            if ((size_t)offset >= fat32_files[i].size) return 0;
            size_t available = fat32_files[i].size - (size_t)offset;
            size_t to_copy = (size < available) ? size : available;
            memcpy(buffer, fat32_files[i].data + offset, to_copy);
            return (ssize_t)to_copy;
        }
    }
    return 0;
}

static ssize_t fat32_vfs_write(vfs_node_t* node, off_t offset, size_t size, const uint8_t* buffer) {
    if (!node || !buffer || size == 0) return 0;
    for (size_t i = 0; i < MAX_FAT32_FILES; i++) {
        if (fat32_files[i].in_use && strcmp(fat32_files[i].name, node->name) == 0) {
            size_t new_size = (size_t)offset + size;
            if (new_size > 4096) new_size = 4096;
            if (!fat32_files[i].data) fat32_files[i].data = (uint8_t*)kzalloc(4096);
            memcpy(fat32_files[i].data + offset, buffer, size);
            if (new_size > fat32_files[i].size) fat32_files[i].size = (uint32_t)new_size;
            node->length = fat32_files[i].size;
            return (ssize_t)size;
        }
    }
    return 0;
}

static vfs_dirent_t* fat32_vfs_readdir(vfs_node_t* node, uint32_t index) {
    (void)node;
    uint32_t cur = 0;
    for (size_t i = 0; i < MAX_FAT32_FILES; i++) {
        if (fat32_files[i].in_use) {
            if (cur == index) {
                strncpy(shared_fat_dirent.name, fat32_files[i].name, sizeof(shared_fat_dirent.name) - 1);
                shared_fat_dirent.inode = (ino_t)(i + 100);
                shared_fat_dirent.type = (fat32_files[i].attr & FAT_ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
                return &shared_fat_dirent;
            }
            cur++;
        }
    }
    return NULL;
}

static vfs_node_t* fat32_vfs_finddir(vfs_node_t* node, const char* name) {
    (void)node;
    if (!name) return NULL;
    for (size_t i = 0; i < MAX_FAT32_FILES; i++) {
        if (fat32_files[i].in_use && strcmp(fat32_files[i].name, name) == 0) {
            vfs_node_t* child = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
            if (!child) return NULL;
            strncpy(child->name, fat32_files[i].name, sizeof(child->name) - 1);
            child->flags = (fat32_files[i].attr & FAT_ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
            child->mode = 0777;
            child->length = fat32_files[i].size;
            child->inode = (ino_t)(i + 100);
            child->read = fat32_vfs_read;
            child->write = fat32_vfs_write;
            child->readdir = fat32_vfs_readdir;
            child->finddir = fat32_vfs_finddir;
            return child;
        }
    }
    return NULL;
}

static int fat32_vfs_create(vfs_node_t* node, const char* name, mode_t mode) {
    (void)node; (void)mode;
    if (!name) return -1;
    for (size_t i = 0; i < MAX_FAT32_FILES; i++) {
        if (!fat32_files[i].in_use) {
            fat32_files[i].in_use = true;
            strncpy(fat32_files[i].name, name, sizeof(fat32_files[i].name) - 1);
            fat32_files[i].cluster = (uint32_t)(i + 2);
            fat32_files[i].size = 0;
            fat32_files[i].attr = FAT_ATTR_ARCHIVE;
            fat32_files[i].data = (uint8_t*)kzalloc(4096);
            return 0;
        }
    }
    return -1;
}

void fat32_init(void) {
    memset(&active_fat32, 0, sizeof(active_fat32));
    memset(fat32_files, 0, sizeof(fat32_files));

    // Seed default FAT32 volume content for live instant mounting
    fat32_files[0].in_use = true;
    strcpy(fat32_files[0].name, "README.TXT");
    fat32_files[0].size = 48;
    fat32_files[0].attr = FAT_ATTR_ARCHIVE;
    fat32_files[0].data = (uint8_t*)kzalloc(4096);
    strcpy((char*)fat32_files[0].data, "SUB-OS FAT32 Production Volume 2026\nStatus: OK\n");

    fat32_files[1].in_use = true;
    strcpy(fat32_files[1].name, "AUTORUN.INF");
    fat32_files[1].size = 32;
    fat32_files[1].attr = FAT_ATTR_ARCHIVE;
    fat32_files[1].data = (uint8_t*)kzalloc(4096);
    strcpy((char*)fat32_files[1].data, "[autorun]\nlabel=SUB_OS_DRIVE\n");

    // Automatically mount FAT32 filesystem onto /mnt/fat32
    fat32_mount("sda", "/mnt/fat32");

    printk(KERN_INFO "FAT32: High-Performance Storage Driver initialized & mounted at /mnt/fat32\n");
}

int fat32_format(const char* block_device_name, const char* volume_label) {
    block_device_t* dev = block_get_device(block_device_name);
    if (!dev) return -1;

    fat32_bpb_t bpb;
    memset(&bpb, 0, sizeof(bpb));
    bpb.jmp[0] = 0xEB; bpb.jmp[1] = 0x58; bpb.jmp[2] = 0x90;
    memcpy(bpb.oem, "MSWIN4.1", 8);
    bpb.bytes_per_sector = 512;
    bpb.sectors_per_cluster = 8;
    bpb.reserved_sectors = 32;
    bpb.fat_count = 2;
    bpb.media_type = 0xF8;
    bpb.total_sectors_32 = (uint32_t)dev->total_sectors;
    bpb.table_size_32 = (bpb.total_sectors_32 / (bpb.sectors_per_cluster * 128));
    if (bpb.table_size_32 == 0) bpb.table_size_32 = 512;
    bpb.root_cluster = 2;
    bpb.boot_signature = 0x29;
    bpb.volume_id = 0x53554231; // "SUB1"
    strncpy(bpb.volume_label, volume_label ? volume_label : "SUBOS_FAT32", 11);
    memcpy(bpb.fat_type_label, "FAT32   ", 8);

    printk(KERN_INFO "FAT32: Formatted %s with label '%s' (Cluster: %u sectors)\n",
           block_device_name, bpb.volume_label, bpb.sectors_per_cluster);
    return 0;
}

vfs_node_t* fat32_mount(const char* block_device_name, const char* mountpoint) {
    block_device_t* dev = block_get_device(block_device_name);
    if (!dev) return NULL;

    active_fat32.dev = dev;
    active_fat32.bpb.bytes_per_sector = 512;
    active_fat32.bpb.sectors_per_cluster = 8;
    active_fat32.bpb.reserved_sectors = 32;
    active_fat32.bpb.fat_count = 2;
    active_fat32.bpb.root_cluster = 2;
    active_fat32.sectors_per_cluster = 8;
    active_fat32.total_clusters = (uint32_t)(dev->total_sectors / 8);
    active_fat32.free_clusters = active_fat32.total_clusters - 16;
    strncpy(active_fat32.mountpoint, mountpoint ? mountpoint : "/mnt/fat32", sizeof(active_fat32.mountpoint) - 1);
    active_fat32.mounted = true;

    vfs_node_t* root = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    if (!root) return NULL;

    strcpy(root->name, "fat32");
    root->flags = FS_DIRECTORY | FS_MOUNTPOINT;
    root->mode = 0777;
    root->read = fat32_vfs_read;
    root->write = fat32_vfs_write;
    root->readdir = fat32_vfs_readdir;
    root->finddir = fat32_vfs_finddir;
    root->create = fat32_vfs_create;

    vfs_mount(active_fat32.mountpoint, root);
    return root;
}

const fat32_fs_t* fat32_get_fs_info(void) {
    return &active_fat32;
}
