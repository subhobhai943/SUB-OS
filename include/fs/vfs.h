#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <kernel/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x04
#define FS_BLOCKDEVICE 0x08
#define FS_PIPE        0x10
#define FS_SYMLINK     0x20
#define FS_MOUNTPOINT  0x40

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct vfs_node;

typedef ssize_t (*vfs_read_fn_t)(struct vfs_node* node, off_t offset, size_t size, uint8_t* buffer);
typedef ssize_t (*vfs_write_fn_t)(struct vfs_node* node, off_t offset, size_t size, const uint8_t* buffer);
typedef int     (*vfs_open_fn_t)(struct vfs_node* node, uint32_t flags);
typedef int     (*vfs_close_fn_t)(struct vfs_node* node);
typedef struct vfs_dirent* (*vfs_readdir_fn_t)(struct vfs_node* node, uint32_t index);
typedef struct vfs_node*   (*vfs_finddir_fn_t)(struct vfs_node* node, const char* name);
typedef int     (*vfs_mkdir_fn_t)(struct vfs_node* node, const char* name, mode_t mode);
typedef int     (*vfs_create_fn_t)(struct vfs_node* node, const char* name, mode_t mode);

typedef struct vfs_node {
    char name[128];
    uint32_t flags;
    uint32_t mode;
    uid_t uid;
    gid_t gid;
    size_t length;
    ino_t inode;

    vfs_read_fn_t    read;
    vfs_write_fn_t   write;
    vfs_open_fn_t    open;
    vfs_close_fn_t   close;
    vfs_readdir_fn_t readdir;
    vfs_finddir_fn_t finddir;
    vfs_mkdir_fn_t   mkdir;
    vfs_create_fn_t  create;

    struct vfs_node* ptr; // Used for mountpoints / device nodes
} vfs_node_t;

typedef struct vfs_dirent {
    char name[128];
    ino_t inode;
    uint32_t type;
} vfs_dirent_t;

typedef struct {
    vfs_node_t* node;
    off_t offset;
    uint32_t flags;
} file_descriptor_t;

void vfs_init(void);
vfs_node_t* vfs_namei(const char* path);
int vfs_open(const char* path, uint32_t flags);
ssize_t vfs_read(int fd, void* buffer, size_t size);
ssize_t vfs_write(int fd, const void* buffer, size_t size);
off_t vfs_lseek(int fd, off_t offset, int whence);
int vfs_close(int fd);
int vfs_mkdir(const char* path, mode_t mode);
int vfs_create(const char* path, mode_t mode);

vfs_node_t* vfs_get_root(void);
int vfs_mount(const char* path, vfs_node_t* fs_root);

#endif // _FS_VFS_H
