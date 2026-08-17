#include <fs/ramfs.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

#define MAX_CHILDREN 32

typedef struct ramfs_entry {
    vfs_node_t node;
    uint8_t* data;
    size_t capacity;
    struct ramfs_entry* children[MAX_CHILDREN];
    uint32_t child_count;
} ramfs_entry_t;

static vfs_dirent_t shared_dirent;

static int ramfs_mkdir_impl(vfs_node_t* parent, const char* name, mode_t mode);
static int ramfs_create_impl(vfs_node_t* parent, const char* name, mode_t mode);
static struct vfs_dirent* ramfs_readdir(vfs_node_t* node, uint32_t index);
static struct vfs_node*   ramfs_finddir(vfs_node_t* node, const char* name);

static ssize_t ramfs_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    ramfs_entry_t* entry = (ramfs_entry_t*)node;
    if (!entry || !entry->data || (size_t)offset >= node->length) return 0;

    if (offset + size > node->length) {
        size = node->length - offset;
    }

    memcpy(buffer, entry->data + offset, size);
    return (ssize_t)size;
}

static ssize_t ramfs_write(vfs_node_t* node, off_t offset, size_t size, const uint8_t* buffer) {
    ramfs_entry_t* entry = (ramfs_entry_t*)node;
    if (!entry) return -1;

    if (offset + size > entry->capacity) {
        size_t new_cap = (offset + size + 256) & ~255;
        uint8_t* new_data = (uint8_t*)kmalloc(new_cap);
        if (!new_data) return -1;

        if (entry->data) {
            memcpy(new_data, entry->data, node->length);
            kfree(entry->data);
        }
        entry->data = new_data;
        entry->capacity = new_cap;
    }

    memcpy(entry->data + offset, buffer, size);
    if (offset + size > node->length) {
        node->length = offset + size;
    }
    return (ssize_t)size;
}

static struct vfs_dirent* ramfs_readdir(vfs_node_t* node, uint32_t index) {
    ramfs_entry_t* entry = (ramfs_entry_t*)node;
    if (!entry || index >= entry->child_count) return NULL;

    strncpy(shared_dirent.name, entry->children[index]->node.name, sizeof(shared_dirent.name) - 1);
    shared_dirent.inode = entry->children[index]->node.inode;
    shared_dirent.type  = entry->children[index]->node.flags;
    return &shared_dirent;
}

static struct vfs_node* ramfs_finddir(vfs_node_t* node, const char* name) {
    ramfs_entry_t* entry = (ramfs_entry_t*)node;
    if (!entry) return NULL;

    for (uint32_t i = 0; i < entry->child_count; i++) {
        if (strcmp(entry->children[i]->node.name, name) == 0) {
            return &entry->children[i]->node;
        }
    }
    return NULL;
}

static int ramfs_mkdir_impl(vfs_node_t* parent, const char* name, mode_t mode) {
    ramfs_entry_t* p = (ramfs_entry_t*)parent;
    if (!p || p->child_count >= MAX_CHILDREN) return -1;

    ramfs_entry_t* child = (ramfs_entry_t*)kzalloc(sizeof(ramfs_entry_t));
    if (!child) return -1;

    strncpy(child->node.name, name, sizeof(child->node.name) - 1);
    child->node.flags = FS_DIRECTORY;
    child->node.mode  = mode;
    child->node.readdir = ramfs_readdir;
    child->node.finddir = ramfs_finddir;
    child->node.mkdir   = ramfs_mkdir_impl;
    child->node.create  = ramfs_create_impl;

    p->children[p->child_count++] = child;
    return 0;
}

static int ramfs_create_impl(vfs_node_t* parent, const char* name, mode_t mode) {
    ramfs_entry_t* p = (ramfs_entry_t*)parent;
    if (!p || p->child_count >= MAX_CHILDREN) return -1;

    ramfs_entry_t* child = (ramfs_entry_t*)kzalloc(sizeof(ramfs_entry_t));
    if (!child) return -1;

    strncpy(child->node.name, name, sizeof(child->node.name) - 1);
    child->node.flags = FS_FILE;
    child->node.mode  = mode;
    child->node.read  = ramfs_read;
    child->node.write = ramfs_write;

    p->children[p->child_count++] = child;
    return 0;
}

vfs_node_t* ramfs_add_file(vfs_node_t* parent, const char* name, const void* content, size_t size) {
    ramfs_entry_t* p = (ramfs_entry_t*)parent;
    if (!p || p->child_count >= MAX_CHILDREN) return NULL;

    ramfs_entry_t* child = (ramfs_entry_t*)kzalloc(sizeof(ramfs_entry_t));
    if (!child) return NULL;

    strncpy(child->node.name, name, sizeof(child->node.name) - 1);
    child->node.flags = FS_FILE;
    child->node.mode = 0644;
    child->node.length = size;
    child->node.read = ramfs_read;
    child->node.write = ramfs_write;

    if (size > 0 && content) {
        child->capacity = size;
        child->data = (uint8_t*)kmalloc(size);
        if (child->data) {
            memcpy(child->data, content, size);
        }
    }

    p->children[p->child_count++] = child;
    return &child->node;
}

vfs_node_t* ramfs_add_dir(vfs_node_t* parent, const char* name) {
    ramfs_entry_t* child = (ramfs_entry_t*)kzalloc(sizeof(ramfs_entry_t));
    if (!child) return NULL;

    strncpy(child->node.name, name, sizeof(child->node.name) - 1);
    child->node.flags = FS_DIRECTORY;
    child->node.mode = 0755;
    child->node.readdir = ramfs_readdir;
    child->node.finddir = ramfs_finddir;
    child->node.mkdir   = ramfs_mkdir_impl;
    child->node.create  = ramfs_create_impl;

    if (parent) {
        ramfs_entry_t* p = (ramfs_entry_t*)parent;
        if (p->child_count < MAX_CHILDREN) {
            p->children[p->child_count++] = child;
        }
    }

    return &child->node;
}

vfs_node_t* ramfs_create_root(void) {
    ramfs_entry_t* root = (ramfs_entry_t*)kzalloc(sizeof(ramfs_entry_t));
    if (!root) return NULL;

    strcpy(root->node.name, "/");
    root->node.flags = FS_DIRECTORY;
    root->node.mode = 0755;
    root->node.readdir = ramfs_readdir;
    root->node.finddir = ramfs_finddir;
    root->node.mkdir   = ramfs_mkdir_impl;
    root->node.create  = ramfs_create_impl;

    vfs_node_t* etc = ramfs_add_dir(&root->node, "etc");
    ramfs_add_dir(&root->node, "bin");
    ramfs_add_dir(&root->node, "dev");
    ramfs_add_dir(&root->node, "proc");
    ramfs_add_dir(&root->node, "sys");
    ramfs_add_dir(&root->node, "home");
    ramfs_add_dir(&root->node, "tmp");
    vfs_node_t* mnt = ramfs_add_dir(&root->node, "mnt");
    if (mnt) {
        ramfs_add_dir(mnt, "fat32");
        ramfs_add_dir(mnt, "ramdisk");
        ramfs_add_dir(mnt, "nvme");
    }

    if (etc) {
        ramfs_add_file(etc, "hostname", "sub-os\n", 7);
        ramfs_add_file(etc, "os-release", "NAME=\"SUB-OS\"\nVERSION=\"0.2.0-lts\"\nARCH=\"x86_64\"\n", 48);
        ramfs_add_file(etc, "resolv.conf", "nameserver 10.0.2.3\nsearch localdomain\n", 39);
        ramfs_add_file(etc, "motd", "====================================================\n  Welcome to SUB-OS Modular Monolithic Linux Core!\n====================================================\n", 137);
    }

    const char* readme = "SUB-OS Production-Level Modular Operating System\nArchitecture: x86_64 Long Mode\nSubsystems: PMM, Heap, VFS, PCI, E1000, Net, Crypto, LazyBox\n";
    ramfs_add_file(&root->node, "readme.txt", readme, strlen(readme));

    return &root->node;
}
