#ifndef _LIB_JPEG_H
#define _LIB_JPEG_H

#include <stdint.h>
#include <lib/image.h>

// Baseline JPEG (SOF0) decoder. Fills img with a heap-allocated 0xAARRGGBB
// buffer on success (return 0); progressive/arithmetic JPEGs are rejected.
int jpeg_decode(const uint8_t* data, unsigned len, image_t* img);

#endif // _LIB_JPEG_H
