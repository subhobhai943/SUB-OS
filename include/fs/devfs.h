#ifndef _FS_DEVFS_H
#define _FS_DEVFS_H

#include "vfs.h"

void devfs_init(void);
vfs_node_t* devfs_get_root(void);
int devfs_register_device(const char* name, vfs_node_t* dev_node);

#endif // _FS_DEVFS_H
