#ifndef _LIB_FONT8X8_H
#define _LIB_FONT8X8_H

#include <stdint.h>

// Shared 8x8 bitmap console font.
//
// The framebuffer console, the 2D canvas rasterizer and the desktop compositor
// all render text from this one table. Each glyph is 8 bytes, one per scanline,
// with the most significant bit at the left of the cell.

#define FONT8X8_WIDTH  8
#define FONT8X8_HEIGHT 8
#define FONT8X8_FIRST  32
#define FONT8X8_LAST   127
#define FONT8X8_GLYPHS (FONT8X8_LAST - FONT8X8_FIRST + 1)

extern const uint8_t font8x8_basic[FONT8X8_GLYPHS][FONT8X8_HEIGHT];

// Scanlines for `c`, substituting '?' for anything unprintable.
const uint8_t* font8x8_glyph(char c);

#endif // _LIB_FONT8X8_H
