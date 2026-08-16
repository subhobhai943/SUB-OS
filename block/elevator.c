#include <block/elevator.h>
#include <kernel/printk.h>

static void noop_queue_request(block_device_t* dev, block_request_t* req) {
    if (!dev || !req) return;
    req->next = NULL;
    if (!dev->queue_tail) {
        dev->queue_head = req;
        dev->queue_tail = req;
    } else {
        dev->queue_tail->next = req;
        dev->queue_tail = req;
    }
}

static block_request_t* noop_dispatch_request(block_device_t* dev) {
    if (!dev || !dev->queue_head) return NULL;
    block_request_t* req = dev->queue_head;
    dev->queue_head = req->next;
    if (!dev->queue_head) {
        dev->queue_tail = NULL;
    }
    return req;
}

static elevator_t noop_elevator = {
    .type = ELEVATOR_NOOP,
    .name = "noop",
    .queue_request = noop_queue_request,
    .dispatch_request = noop_dispatch_request
};

static elevator_t* current_elevator = &noop_elevator;

void elevator_init(void) {
    current_elevator = &noop_elevator;
    printk(KERN_INFO "ELEVATOR: Initialized default '%s' I/O scheduler\n", current_elevator->name);
}

elevator_t* elevator_get_active(void) {
    return current_elevator;
}

void elevator_set_scheduler(elevator_type_t type) {
    (void)type;
    current_elevator = &noop_elevator;
}
