#include <fs/devfs.h>
#include <fs/ramfs.h>
#include <drivers/tty.h>
#include <crypto/crypto.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

static vfs_node_t* devfs_root = NULL;

static ssize_t dev_null_write(vfs_node_t* node, off_t offset, size_t size, const uint8_t* buffer) {
    (void)node; (void)offset; (void)buffer;
    return (ssize_t)size; // Discard everything
}

static ssize_t dev_null_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0; // EOF immediately
}

static ssize_t dev_zero_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node; (void)offset;
    memset(buffer, 0, size);
    return (ssize_t)size;
}

static ssize_t dev_random_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node; (void)offset;
    prng_get_bytes(buffer, size);
    return (ssize_t)size;
}

static ssize_t dev_tty_write(vfs_node_t* node, off_t offset, size_t size, const uint8_t* buffer) {
    (void)node; (void)offset;
    tty_write((const char*)buffer, size);
    return (ssize_t)size;
}

void devfs_init(void) {
    devfs_root = ramfs_add_dir(NULL, "dev");

    // 1. /dev/null
    vfs_node_t* null_dev = ramfs_add_file(devfs_root, "null", NULL, 0);
    if (null_dev) {
        null_dev->flags = FS_CHARDEVICE;
        null_dev->read  = dev_null_read;
        null_dev->write = dev_null_write;
    }

    // 2. /dev/zero
    vfs_node_t* zero_dev = ramfs_add_file(devfs_root, "zero", NULL, 0);
    if (zero_dev) {
        zero_dev->flags = FS_CHARDEVICE;
        zero_dev->read  = dev_zero_read;
        zero_dev->write = dev_null_write;
    }

    // 3. /dev/random & /dev/urandom
    vfs_node_t* rand_dev = ramfs_add_file(devfs_root, "random", NULL, 0);
    if (rand_dev) {
        rand_dev->flags = FS_CHARDEVICE;
        rand_dev->read  = dev_random_read;
    }
    vfs_node_t* urand_dev = ramfs_add_file(devfs_root, "urandom", NULL, 0);
    if (urand_dev) {
        urand_dev->flags = FS_CHARDEVICE;
        urand_dev->read  = dev_random_read;
    }

    // 4. /dev/tty
    vfs_node_t* tty_dev = ramfs_add_file(devfs_root, "tty", NULL, 0);
    if (tty_dev) {
        tty_dev->flags = FS_CHARDEVICE;
        tty_dev->write = dev_tty_write;
    }
}

vfs_node_t* devfs_get_root(void) {
    return devfs_root;
}
