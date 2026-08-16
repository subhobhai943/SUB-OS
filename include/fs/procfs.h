#ifndef _FS_PROCFS_H
#define _FS_PROCFS_H

#include "vfs.h"

void procfs_init(void);
vfs_node_t* procfs_get_root(void);

#endif // _FS_PROCFS_H
