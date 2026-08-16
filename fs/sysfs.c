#include <fs/sysfs.h>
#include <fs/ramfs.h>
#include <kernel/printk.h>

void sysfs_init(void) {
    vfs_mkdir("/sys", 0755);
    vfs_mkdir("/sys/class", 0755);
    vfs_mkdir("/sys/devices", 0755);
    vfs_mkdir("/sys/bus", 0755);
    vfs_mkdir("/sys/bus/pci", 0755);
    vfs_mkdir("/sys/kernel", 0755);

    vfs_node_t* sys_kernel = vfs_namei("/sys/kernel");
    if (sys_kernel) {
        ramfs_add_file(sys_kernel, "osrelease", "0.2.0-lts\n", 10);
        ramfs_add_file(sys_kernel, "ostype", "SUB-OS\n", 7);
        ramfs_add_file(sys_kernel, "hostname", "sub-node\n", 9);
    }

    printk(KERN_INFO "SYSFS: Mounted /sys kernel object hierarchy\n");
}
