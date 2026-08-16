#ifndef _BLOCK_ELEVATOR_H
#define _BLOCK_ELEVATOR_H

#include <block/block.h>

typedef enum {
    ELEVATOR_NOOP = 0,
    ELEVATOR_DEADLINE,
    ELEVATOR_CFQ
} elevator_type_t;

typedef struct elevator {
    elevator_type_t type;
    const char* name;
    void (*queue_request)(block_device_t* dev, block_request_t* req);
    block_request_t* (*dispatch_request)(block_device_t* dev);
} elevator_t;

void elevator_init(void);
elevator_t* elevator_get_active(void);
void elevator_set_scheduler(elevator_type_t type);

#endif // _BLOCK_ELEVATOR_H
