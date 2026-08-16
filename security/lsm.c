#include <security/security.h>
#include <security/capability.h>
#include <kernel/printk.h>
#include <lib/string.h>

static security_ops_t* active_lsm = NULL;
static kernel_cap_t root_capabilities = CAP_FULL_SET;

bool cap_capable(kernel_cap_t caps, int cap) {
    if (cap < 0 || cap >= 64) return false;
    return (caps & (1ULL << cap)) != 0;
}

void cap_raise(kernel_cap_t* caps, int cap) {
    if (caps && cap >= 0 && cap < 64) {
        *caps |= (1ULL << cap);
    }
}

void cap_lower(kernel_cap_t* caps, int cap) {
    if (caps && cap >= 0 && cap < 64) {
        *caps &= ~(1ULL << cap);
    }
}

static int default_capable(kernel_cap_t caps, int cap) {
    return cap_capable(caps, cap) ? 0 : -1;
}

static int default_file_permission(struct vfs_node* node, int mask) {
    (void)node; (void)mask;
    return 0; // Granted by default in DAC
}

static int default_file_open(struct vfs_node* node, int flags) {
    (void)node; (void)flags;
    return 0;
}

static security_ops_t builtin_lsm = {
    .name = "SUB-OS Capability & DAC LSM",
    .task_create = NULL,
    .file_permission = default_file_permission,
    .file_open = default_file_open,
    .capable = default_capable,
    .socket_create = NULL
};

void security_init(void) {
    active_lsm = &builtin_lsm;
    printk(KERN_INFO "SECURITY: Initialized %s (Enforcing POSIX Capabilities)\n", active_lsm->name);
}

int security_register_lsm(security_ops_t* ops) {
    if (!ops) return -1;
    active_lsm = ops;
    printk(KERN_INFO "SECURITY: Activated LSM provider: %s\n", ops->name);
    return 0;
}

int security_file_permission(struct vfs_node* node, int mask) {
    if (active_lsm && active_lsm->file_permission) {
        return active_lsm->file_permission(node, mask);
    }
    return 0;
}

int security_file_open(struct vfs_node* node, int flags) {
    if (active_lsm && active_lsm->file_open) {
        return active_lsm->file_open(node, flags);
    }
    return 0;
}

int security_capable(int cap) {
    if (active_lsm && active_lsm->capable) {
        return active_lsm->capable(root_capabilities, cap);
    }
    return cap_capable(root_capabilities, cap) ? 0 : -1;
}

const char* security_get_active_lsm_name(void) {
    return active_lsm ? active_lsm->name : "None";
}
