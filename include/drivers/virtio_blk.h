#ifndef _DRIVERS_VIRTIO_BLK_H
#define _DRIVERS_VIRTIO_BLK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
#define VIRTIO_BLK_T_FLUSH        4
#define VIRTIO_BLK_T_GET_ID       8

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) virtio_blk_req_header_t;

typedef struct {
    char     serial[20];
    uint64_t capacity_sectors;
    uint32_t blk_size;
    uint16_t queue_size;
    bool     initialized;
} virtio_blk_info_t;

bool virtio_blk_init(void);
bool virtio_blk_is_detected(void);
const virtio_blk_info_t* virtio_blk_get_info(void);
int  virtio_blk_read(uint64_t sector, uint32_t count, void* buffer);
int  virtio_blk_write(uint64_t sector, uint32_t count, const void* buffer);

#endif // _DRIVERS_VIRTIO_BLK_H
