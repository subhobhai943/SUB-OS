#ifndef _FS_SYSFS_H
#define _FS_SYSFS_H

#include <fs/vfs.h>

void sysfs_init(void);
vfs_node_t* sysfs_create_entry(const char* path, const char* value);

#endif // _FS_SYSFS_H
