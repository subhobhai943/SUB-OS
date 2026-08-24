#ifndef _LIB_KFIFO_H
#define _LIB_KFIFO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/sync.h>

// Power-of-two byte ring buffer with an unsigned index-wrap discipline,
// mirroring the classic Linux kfifo. Producer and consumer indices are
// free-running; masking happens only at access time.

typedef struct kfifo {
    uint8_t*  buffer;
    size_t    size;      // Always a power of two
    size_t    mask;      // size - 1
    size_t    in;        // Free-running producer index
    size_t    out;       // Free-running consumer index
    bool      owns_buffer;
    spinlock_t lock;
} kfifo_t;

int    kfifo_alloc(kfifo_t* fifo, size_t size);
int    kfifo_init_static(kfifo_t* fifo, void* buffer, size_t size);
void   kfifo_free(kfifo_t* fifo);
void   kfifo_reset(kfifo_t* fifo);

size_t kfifo_in(kfifo_t* fifo, const void* src, size_t len);
size_t kfifo_out(kfifo_t* fifo, void* dst, size_t len);
size_t kfifo_peek(const kfifo_t* fifo, void* dst, size_t len);

size_t kfifo_len(const kfifo_t* fifo);
size_t kfifo_avail(const kfifo_t* fifo);
bool   kfifo_is_empty(const kfifo_t* fifo);
bool   kfifo_is_full(const kfifo_t* fifo);

int    kfifo_put_byte(kfifo_t* fifo, uint8_t byte);
int    kfifo_get_byte(kfifo_t* fifo, uint8_t* byte_out);

#endif // _LIB_KFIFO_H
