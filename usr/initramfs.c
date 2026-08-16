#include <usr/initramfs.h>
#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <lib/string.h>
#include <kernel/printk.h>

void initramfs_init(void) {
    initramfs_populate_system();
    printk(KERN_INFO "USR: Initialized root userspace tree and embedded initramfs binaries\n");
}

int initramfs_populate_system(void) {
    vfs_node_t* root = vfs_get_root();
    if (!root) return -1;

    // Create /usr, /usr/bin, /usr/lib, /var, /var/log, /sbin
    vfs_mkdir("/usr", 0755);
    vfs_mkdir("/usr/bin", 0755);
    vfs_mkdir("/usr/lib", 0755);
    vfs_mkdir("/var", 0755);
    vfs_mkdir("/var/log", 0755);
    vfs_mkdir("/sbin", 0755);
    vfs_mkdir("/root", 0700);

    // Add standard system config files
    vfs_node_t* etc = vfs_namei("/etc");
    if (etc) {
        ramfs_add_file(etc, "issue", "SUB-OS 64-Bit Production Kernel (Titan) \\n \\l\n\n", 47);
        ramfs_add_file(etc, "fstab", "# /etc/fstab: static file system info\n/dev/ram0 / ramfs defaults 0 0\n/dev/sda1 /mnt ext2 defaults 0 1\n", 102);
        ramfs_add_file(etc, "passwd", "root:x:0:0:root:/root:/bin/lazybox\nuser:x:1000:1000:user:/home/user:/bin/lazybox\n", 81);
        ramfs_add_file(etc, "group", "root:x:0:\nwheel:x:10:root,user\nusers:x:100:user\n", 48);
    }

    // Populate /var/log/boot.log
    vfs_node_t* var_log = vfs_namei("/var/log");
    if (var_log) {
        ramfs_add_file(var_log, "boot.log", "[OK] System initialized in 64-bit production mode.\n[OK] Virtual File System online.\n", 83);
    }

    return 0;
}
