#ifndef _LIB_IMAGE_H
#define _LIB_IMAGE_H

#include <stdint.h>
#include <stdbool.h>

// Decoded image: a heap-allocated buffer of 0xAARRGGBB pixels, row-major, ready
// to hand to gui_gfx_blit. Free with image_free.
typedef struct {
    int       width;
    int       height;
    uint32_t* pixels;
} image_t;

// True if the bytes begin with a signature this decoder supports (PNG or BMP).
bool image_sniff(const uint8_t* data, unsigned len);

// Decode PNG (8-bit grayscale/RGB/RGBA/palette, non-interlaced) or BMP
// (24/32-bit uncompressed) into img. Returns 0 on success. On failure img is
// left zeroed and nothing needs freeing.
int  image_decode(const uint8_t* data, unsigned len, image_t* img);

void image_free(image_t* img);

#endif // _LIB_IMAGE_H
