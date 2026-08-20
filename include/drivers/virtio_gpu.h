#ifndef _DRIVERS_VIRTIO_GPU_H
#define _DRIVERS_VIRTIO_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM 3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM 4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM 67
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM 68

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t* framebuffer;
    bool active;
} virtio_gpu_info_t;

void virtio_gpu_init(void);
bool virtio_gpu_is_active(void);
virtio_gpu_info_t virtio_gpu_get_info(void);
void virtio_gpu_flush_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void virtio_gpu_clear_color(uint32_t argb);
void virtio_gpu_dump_status(void);

#endif // _DRIVERS_VIRTIO_GPU_H
