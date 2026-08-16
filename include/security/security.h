#ifndef _SECURITY_SECURITY_H
#define _SECURITY_SECURITY_H

#include <security/capability.h>
#include <kernel/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct vfs_node;

typedef struct security_operations {
    const char* name;
    int (*task_create)(uint32_t clone_flags);
    int (*file_permission)(struct vfs_node* node, int mask);
    int (*file_open)(struct vfs_node* node, int flags);
    int (*capable)(kernel_cap_t caps, int cap);
    int (*socket_create)(int family, int type, int protocol);
} security_ops_t;

void security_init(void);
int security_register_lsm(security_ops_t* ops);
int security_file_permission(struct vfs_node* node, int mask);
int security_file_open(struct vfs_node* node, int flags);
int security_capable(int cap);
const char* security_get_active_lsm_name(void);

#endif // _SECURITY_SECURITY_H
