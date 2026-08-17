#ifndef _DRIVERS_RAMDISK_H
#define _DRIVERS_RAMDISK_H

#include <block/block.h>

#define RAMDISK_SIZE (512 * 1024) // 512 KB Ramdisk (1024 sectors)
#define RAMDISK_SECTOR_SIZE 512

void ramdisk_init(void);
block_device_t* ramdisk_get_block_device(void);

#endif // _DRIVERS_RAMDISK_H
