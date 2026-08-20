#ifndef _DRIVERS_VIRTIO_INPUT_H
#define _DRIVERS_VIRTIO_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#define VIRTIO_INPUT_EV_SYN 0x00
#define VIRTIO_INPUT_EV_KEY 0x01
#define VIRTIO_INPUT_EV_REL 0x02
#define VIRTIO_INPUT_EV_ABS 0x03

typedef struct {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed)) virtio_input_event_t;

void virtio_input_init(void);
bool virtio_input_is_active(void);
void virtio_input_dump_status(void);

#endif // _DRIVERS_VIRTIO_INPUT_H
