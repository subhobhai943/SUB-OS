#ifndef _FS_RAMFS_H
#define _FS_RAMFS_H

#include "vfs.h"

vfs_node_t* ramfs_create_root(void);
vfs_node_t* ramfs_add_file(vfs_node_t* parent, const char* name, const void* content, size_t size);
vfs_node_t* ramfs_add_dir(vfs_node_t* parent, const char* name);

#endif // _FS_RAMFS_H
