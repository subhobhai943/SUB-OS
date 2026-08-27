#ifndef _LIB_INFLATE_H
#define _LIB_INFLATE_H

#include <stdint.h>

// DEFLATE / zlib decompression (RFC 1950/1951). Both write into a
// caller-provided buffer; *dest_len is the buffer capacity on entry and the
// decompressed length on success. Return 0 on success, negative on error.
int inflate_raw(uint8_t* dest, unsigned* dest_len,
                const uint8_t* src, unsigned src_len);
int inflate_zlib(uint8_t* dest, unsigned* dest_len,
                 const uint8_t* src, unsigned src_len);

#endif // _LIB_INFLATE_H
