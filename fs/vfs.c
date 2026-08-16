#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_OPEN_FILES 64

static vfs_node_t* vfs_root = NULL;
static file_descriptor_t file_descriptors[MAX_OPEN_FILES];

void vfs_init(void) {
    memset(file_descriptors, 0, sizeof(file_descriptors));

    // 1. Create root RAM filesystem
    vfs_root = ramfs_create_root();

    // 2. Initialize and mount /dev
    devfs_init();
    vfs_mount("/dev", devfs_get_root());

    // 3. Initialize and mount /proc
    procfs_init();
    vfs_mount("/proc", procfs_get_root());

    printk(KERN_INFO "VFS: Mounted root ramfs, /dev synthetic devfs, and /proc procfs\n");
}

vfs_node_t* vfs_get_root(void) {
    return vfs_root;
}

vfs_node_t* vfs_namei(const char* path) {
    if (!path || !vfs_root) return NULL;
    if (strcmp(path, "/") == 0) return vfs_root;

    if (*path == '/') path++; // Skip leading slash

    char buffer[256];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    vfs_node_t* current = vfs_root;
    char* token = buffer;

    while (*token) {
        char* next_slash = strchr(token, '/');
        if (next_slash) {
            *next_slash = '\0';
        }

        if (current->finddir) {
            current = current->finddir(current, token);
            if (!current) return NULL;

            // Follow mountpoints
            if (current->ptr && (current->flags & FS_MOUNTPOINT)) {
                current = current->ptr;
            }
        } else {
            return NULL;
        }

        if (next_slash) {
            token = next_slash + 1;
        } else {
            break;
        }
    }

    return current;
}

int vfs_open(const char* path, uint32_t flags) {
    vfs_node_t* node = vfs_namei(path);
    if (!node) {
        if (flags & O_CREAT) {
            // Find parent and create
            char parent_path[256];
            strncpy(parent_path, path, sizeof(parent_path) - 1);
            char* last_slash = strrchr(parent_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                const char* filename = last_slash + 1;
                vfs_node_t* parent = vfs_namei(parent_path[0] ? parent_path : "/");
                if (parent && parent->create) {
                    parent->create(parent, filename, 0644);
                    node = vfs_namei(path);
                }
            }
        }
        if (!node) return -1;
    }

    if (node->open) {
        if (node->open(node, flags) != 0) return -1;
    }

    // Find free file descriptor
    for (int fd = 3; fd < MAX_OPEN_FILES; fd++) {
        if (!file_descriptors[fd].node) {
            file_descriptors[fd].node = node;
            file_descriptors[fd].offset = 0;
            file_descriptors[fd].flags = flags;
            return fd;
        }
    }

    return -1; // Out of file descriptors
}

int vfs_open_node(vfs_node_t* node, uint32_t flags) {
    if (!node) return -1;
    if (node->open) {
        if (node->open(node, flags) != 0) return -1;
    }

    for (int fd = 3; fd < MAX_OPEN_FILES; fd++) {
        if (!file_descriptors[fd].node) {
            file_descriptors[fd].node = node;
            file_descriptors[fd].offset = 0;
            file_descriptors[fd].flags = flags;
            return fd;
        }
    }
    return -1;
}

ssize_t vfs_read(int fd, void* buffer, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].node) return -1;

    file_descriptor_t* desc = &file_descriptors[fd];
    if (desc->node->read) {
        ssize_t bytes_read = desc->node->read(desc->node, desc->offset, size, (uint8_t*)buffer);
        if (bytes_read > 0) {
            desc->offset += bytes_read;
        }
        return bytes_read;
    }
    return -1;
}

ssize_t vfs_write(int fd, const void* buffer, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].node) return -1;

    file_descriptor_t* desc = &file_descriptors[fd];
    if (desc->node->write) {
        ssize_t bytes_written = desc->node->write(desc->node, desc->offset, size, (const uint8_t*)buffer);
        if (bytes_written > 0) {
            desc->offset += bytes_written;
        }
        return bytes_written;
    }
    return -1;
}

off_t vfs_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].node) return -1;

    file_descriptor_t* desc = &file_descriptors[fd];
    if (whence == SEEK_SET) desc->offset = offset;
    else if (whence == SEEK_CUR) desc->offset += offset;
    else if (whence == SEEK_END) desc->offset = desc->node->length + offset;
    return desc->offset;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].node) return -1;

    if (file_descriptors[fd].node->close) {
        file_descriptors[fd].node->close(file_descriptors[fd].node);
    }
    file_descriptors[fd].node = NULL;
    file_descriptors[fd].offset = 0;
    return 0;
}

int vfs_mkdir(const char* path, mode_t mode) {
    if (!path || !*path) return -1;
    char parent_path[256];
    strncpy(parent_path, path, sizeof(parent_path) - 1);
    parent_path[sizeof(parent_path) - 1] = '\0';

    char* last_slash = strrchr(parent_path, '/');
    const char* dirname = path;
    vfs_node_t* parent = vfs_get_root();

    if (last_slash) {
        if (last_slash == parent_path) {
            parent = vfs_namei("/");
            dirname = last_slash + 1;
        } else {
            *last_slash = '\0';
            dirname = last_slash + 1;
            parent = vfs_namei(parent_path);
        }
    }

    if (parent && parent->mkdir) {
        return parent->mkdir(parent, dirname, mode);
    }
    return -1;
}

int vfs_create(const char* path, mode_t mode) {
    if (!path || !*path) return -1;
    char parent_path[256];
    strncpy(parent_path, path, sizeof(parent_path) - 1);
    parent_path[sizeof(parent_path) - 1] = '\0';

    char* last_slash = strrchr(parent_path, '/');
    const char* filename = path;
    vfs_node_t* parent = vfs_get_root();

    if (last_slash) {
        if (last_slash == parent_path) {
            parent = vfs_namei("/");
            filename = last_slash + 1;
        } else {
            *last_slash = '\0';
            filename = last_slash + 1;
            parent = vfs_namei(parent_path);
        }
    }

    if (parent && parent->create) {
        return parent->create(parent, filename, mode);
    }
    return -1;
}

int vfs_mount(const char* path, vfs_node_t* fs_root) {
    if (!path || !fs_root) return -1;
    vfs_node_t* mount_node = vfs_namei(path);
    if (!mount_node) {
        // Create directory node for mount point
        char parent_path[256];
        strncpy(parent_path, path, sizeof(parent_path) - 1);
        char* last_slash = strrchr(parent_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            const char* dirname = last_slash + 1;
            vfs_node_t* parent = vfs_namei(parent_path[0] ? parent_path : "/");
            if (parent && parent->mkdir) {
                parent->mkdir(parent, dirname, 0755);
                mount_node = vfs_namei(path);
            }
        }
    }

    if (mount_node) {
        mount_node->flags |= FS_MOUNTPOINT;
        mount_node->ptr = fs_root;
        return 0;
    }
    return -1;
}
