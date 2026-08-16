#ifndef _BLOCK_BLOCK_H
#define _BLOCK_BLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLOCK_SECTOR_SIZE 512
#define MAX_BLOCK_DEVS    8

typedef enum {
    REQ_READ = 0,
    REQ_WRITE = 1,
    REQ_FLUSH = 2
} block_req_type_t;

typedef struct block_request {
    block_req_type_t type;
    uint64_t sector;
    uint32_t sector_count;
    void* buffer;
    bool completed;
    int status;
    struct block_request* next;
} block_request_t;

typedef struct block_device {
    char name[32];
    uint32_t device_id;
    uint64_t total_sectors;
    uint32_t sector_size;
    bool read_only;
    int (*read_sectors)(struct block_device* dev, uint64_t sector, uint32_t count, void* buffer);
    int (*write_sectors)(struct block_device* dev, uint64_t sector, uint32_t count, const void* buffer);
    int (*flush)(struct block_device* dev);
    block_request_t* queue_head;
    block_request_t* queue_tail;
} block_device_t;

void block_init(void);
int block_register_device(block_device_t* dev);
block_device_t* block_get_device(const char* name);
int block_submit_bio(const char* dev_name, block_req_type_t type, uint64_t sector, uint32_t count, void* buffer);
size_t block_get_device_count(void);
block_device_t* block_get_device_by_index(size_t index);

#endif // _BLOCK_BLOCK_H
