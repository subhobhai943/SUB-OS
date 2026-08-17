#ifndef _DRIVERS_VIRTIO_RNG_H
#define _DRIVERS_VIRTIO_RNG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool virtio_rng_init(void);
bool virtio_rng_is_detected(void);
int  virtio_rng_get_random_bytes(void* buf, size_t len);

#endif // _DRIVERS_VIRTIO_RNG_H
