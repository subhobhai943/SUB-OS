#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define MAX_OPEN_FILES 64

static vfs_node_t* vfs_root = NULL;
static file_descriptor_t file_descriptors[MAX_OPEN_FILES];
static char vfs_cwd[256] = "/";

void vfs_init(void) {
    memset(file_descriptors, 0, sizeof(file_descriptors));
    strcpy(vfs_cwd, "/");

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

const char* vfs_getcwd(void) {
    return vfs_cwd;
}

void vfs_resolve_path(const char* path, char* resolved_out, size_t max_len) {
    if (!resolved_out || max_len == 0) return;

    char temp[512];
    if (!path || path[0] == '\0') {
        strncpy(resolved_out, vfs_cwd, max_len - 1);
        resolved_out[max_len - 1] = '\0';
        return;
    }

    if (path[0] == '/') {
        strncpy(temp, path, sizeof(temp) - 1);
    } else {
        if (strcmp(vfs_cwd, "/") == 0) {
            snprintf(temp, sizeof(temp), "/%s", path);
        } else {
            snprintf(temp, sizeof(temp), "%s/%s", vfs_cwd, path);
        }
    }
    temp[sizeof(temp) - 1] = '\0';

    // Normalize '.' and '..' components
    char* stack[32];
    int top = 0;

    char* token = temp;
    while (*token == '/') token++;

    while (*token) {
        char* next_slash = strchr(token, '/');
        if (next_slash) *next_slash = '\0';

        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            // Do nothing
        } else if (strcmp(token, "..") == 0) {
            if (top > 0) top--;
        } else {
            if (top < 32) {
                stack[top++] = token;
            }
        }

        if (next_slash) {
            token = next_slash + 1;
            while (*token == '/') token++;
        } else {
            break;
        }
    }

    if (top == 0) {
        strcpy(resolved_out, "/");
        return;
    }

    resolved_out[0] = '\0';
    for (int i = 0; i < top; i++) {
        strcat(resolved_out, "/");
        strcat(resolved_out, stack[i]);
    }
}

int vfs_chdir(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char abs_path[256];
    vfs_resolve_path(path, abs_path, sizeof(abs_path));

    vfs_node_t* node = vfs_namei(abs_path);
    if (!node) {
        return -1; // Directory does not exist
    }

    if (!(node->flags & FS_DIRECTORY)) {
        return -2; // Not a directory
    }

    strncpy(vfs_cwd, abs_path, sizeof(vfs_cwd) - 1);
    vfs_cwd[sizeof(vfs_cwd) - 1] = '\0';
    return 0;
}

vfs_node_t* vfs_namei(const char* path) {
    if (!path || !vfs_root) return NULL;

    char abs_path[256];
    vfs_resolve_path(path, abs_path, sizeof(abs_path));

    if (strcmp(abs_path, "/") == 0) return vfs_root;

    char buffer[256];
    const char* p = abs_path;
    if (*p == '/') p++;
    strncpy(buffer, p, sizeof(buffer) - 1);
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
            while (*token == '/') token++;
        } else {
            break;
        }
    }

    return current;
}

int vfs_open(const char* path, uint32_t flags) {
    if (!path) return -1;

    char abs_path[256];
    vfs_resolve_path(path, abs_path, sizeof(abs_path));

    vfs_node_t* node = vfs_namei(abs_path);
    if (!node) {
        if (flags & O_CREAT) {
            // Find parent and create
            char parent_path[256];
            strncpy(parent_path, abs_path, sizeof(parent_path) - 1);
            parent_path[sizeof(parent_path) - 1] = '\0';

            char* last_slash = strrchr(parent_path, '/');
            if (last_slash) {
                char filename[128];
                strncpy(filename, last_slash + 1, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';

                if (last_slash == parent_path) {
                    parent_path[1] = '\0'; // Root "/"
                } else {
                    *last_slash = '\0';
                }

                vfs_node_t* parent = vfs_namei(parent_path);
                if (parent && parent->create) {
                    parent->create(parent, filename, 0644);
                    node = vfs_namei(abs_path);
                }
            }
        }
        if (!node) return -1;
    }

    if (flags & O_TRUNC) {
        node->length = 0;
    }

    if (node->open) {
        if (node->open(node, flags) != 0) return -1;
    }

    // Find free file descriptor
    for (int fd = 3; fd < MAX_OPEN_FILES; fd++) {
        if (!file_descriptors[fd].node) {
            file_descriptors[fd].node = node;
            file_descriptors[fd].offset = (flags & O_APPEND) ? (off_t)node->length : 0;
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
    switch (whence) {
        case SEEK_SET:
            desc->offset = offset;
            break;
        case SEEK_CUR:
            desc->offset += offset;
            break;
        case SEEK_END:
            desc->offset = (off_t)desc->node->length + offset;
            break;
        default:
            return -1;
    }
    return desc->offset;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].node) return -1;

    file_descriptor_t* desc = &file_descriptors[fd];
    if (desc->node->close) {
        desc->node->close(desc->node);
    }

    desc->node = NULL;
    desc->offset = 0;
    desc->flags = 0;
    return 0;
}

int vfs_mkdir(const char* path, mode_t mode) {
    if (!path) return -1;

    char abs_path[256];
    vfs_resolve_path(path, abs_path, sizeof(abs_path));

    char parent_path[256];
    strncpy(parent_path, abs_path, sizeof(parent_path) - 1);
    parent_path[sizeof(parent_path) - 1] = '\0';

    char* last_slash = strrchr(parent_path, '/');
    if (!last_slash) return -1;

    char dirname[128];
    strncpy(dirname, last_slash + 1, sizeof(dirname) - 1);
    dirname[sizeof(dirname) - 1] = '\0';

    if (last_slash == parent_path) {
        parent_path[1] = '\0'; // Root "/"
    } else {
        *last_slash = '\0';
    }

    vfs_node_t* parent = vfs_namei(parent_path);
    if (!parent || !parent->mkdir) return -1;

    return parent->mkdir(parent, dirname, mode);
}

int vfs_create(const char* path, mode_t mode) {
    int fd = vfs_open(path, O_CREAT | O_RDWR);
    if (fd >= 0) {
        vfs_close(fd);
        return 0;
    }
    return -1;
}

int vfs_unlink(const char* path) {
    (void)path;
    return 0;
}

int vfs_mount(const char* path, vfs_node_t* fs_root) {
    if (!path || !fs_root) return -1;

    vfs_node_t* mountpoint = vfs_namei(path);
    if (!mountpoint) {
        // Automatically create directory if missing
        if (vfs_mkdir(path, 0755) == 0) {
            mountpoint = vfs_namei(path);
        }
    }

    if (!mountpoint) return -1;

    mountpoint->flags |= FS_MOUNTPOINT;
    mountpoint->ptr = fs_root;
    return 0;
}
