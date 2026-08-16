#ifndef _USR_INITRAMFS_H
#define _USR_INITRAMFS_H

#include <stddef.h>
#include <stdint.h>

/* CPIO newc archive header structure */
struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_maj[8];
    char c_min[8];
    char c_rmaj[8];
    char c_rmin[8];
    char c_namesize[8];
    char c_chksum[8];
};

void initramfs_init(void);
int initramfs_populate_system(void);

#endif // _USR_INITRAMFS_H
