// Power-of-two lock-protected byte ring buffer for SUB-OS
#include <lib/kfifo.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

static size_t round_up_pow2(size_t v) {
    if (v < 2) return 2;
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

int kfifo_alloc(kfifo_t* fifo, size_t size) {
    if (!fifo || size == 0) return -1;

    size = round_up_pow2(size);
    fifo->buffer = (uint8_t*)kmalloc(size);
    if (!fifo->buffer) return -1;

    fifo->size        = size;
    fifo->mask        = size - 1;
    fifo->in          = 0;
    fifo->out         = 0;
    fifo->owns_buffer = true;
    spinlock_init(&fifo->lock);
    return 0;
}

int kfifo_init_static(kfifo_t* fifo, void* buffer, size_t size) {
    if (!fifo || !buffer || size == 0) return -1;
    // A caller-supplied buffer must already be a power of two.
    if (size & (size - 1)) return -1;

    fifo->buffer      = (uint8_t*)buffer;
    fifo->size        = size;
    fifo->mask        = size - 1;
    fifo->in          = 0;
    fifo->out         = 0;
    fifo->owns_buffer = false;
    spinlock_init(&fifo->lock);
    return 0;
}

void kfifo_free(kfifo_t* fifo) {
    if (!fifo) return;
    if (fifo->owns_buffer && fifo->buffer) kfree(fifo->buffer);
    fifo->buffer = NULL;
    fifo->size = fifo->mask = fifo->in = fifo->out = 0;
    fifo->owns_buffer = false;
}

void kfifo_reset(kfifo_t* fifo) {
    if (!fifo) return;
    spin_lock(&fifo->lock);
    fifo->in = fifo->out = 0;
    spin_unlock(&fifo->lock);
}

size_t kfifo_len(const kfifo_t* fifo) {
    if (!fifo || !fifo->buffer) return 0;
    return fifo->in - fifo->out;
}

size_t kfifo_avail(const kfifo_t* fifo) {
    if (!fifo || !fifo->buffer) return 0;
    return fifo->size - (fifo->in - fifo->out);
}

bool kfifo_is_empty(const kfifo_t* fifo) { return kfifo_len(fifo) == 0; }
bool kfifo_is_full(const kfifo_t* fifo)  { return kfifo_avail(fifo) == 0; }

// Copy into the ring at a free-running offset, wrapping at most once.
static void copy_in(kfifo_t* fifo, const uint8_t* src, size_t len, size_t off) {
    size_t start = off & fifo->mask;
    size_t first = fifo->size - start;
    if (first > len) first = len;

    memcpy(fifo->buffer + start, src, first);
    if (len > first) memcpy(fifo->buffer, src + first, len - first);
}

static void copy_out(const kfifo_t* fifo, uint8_t* dst, size_t len, size_t off) {
    size_t start = off & fifo->mask;
    size_t first = fifo->size - start;
    if (first > len) first = len;

    memcpy(dst, fifo->buffer + start, first);
    if (len > first) memcpy(dst + first, fifo->buffer, len - first);
}

size_t kfifo_in(kfifo_t* fifo, const void* src, size_t len) {
    if (!fifo || !fifo->buffer || !src || len == 0) return 0;

    spin_lock(&fifo->lock);
    size_t space = fifo->size - (fifo->in - fifo->out);
    if (len > space) len = space;

    copy_in(fifo, (const uint8_t*)src, len, fifo->in);
    fifo->in += len;
    spin_unlock(&fifo->lock);
    return len;
}

size_t kfifo_out(kfifo_t* fifo, void* dst, size_t len) {
    if (!fifo || !fifo->buffer || !dst || len == 0) return 0;

    spin_lock(&fifo->lock);
    size_t used = fifo->in - fifo->out;
    if (len > used) len = used;

    copy_out(fifo, (uint8_t*)dst, len, fifo->out);
    fifo->out += len;
    spin_unlock(&fifo->lock);
    return len;
}

size_t kfifo_peek(const kfifo_t* fifo, void* dst, size_t len) {
    if (!fifo || !fifo->buffer || !dst || len == 0) return 0;

    size_t used = fifo->in - fifo->out;
    if (len > used) len = used;

    copy_out(fifo, (uint8_t*)dst, len, fifo->out);
    return len;
}

int kfifo_put_byte(kfifo_t* fifo, uint8_t byte) {
    return kfifo_in(fifo, &byte, 1) == 1 ? 0 : -1;
}

int kfifo_get_byte(kfifo_t* fifo, uint8_t* byte_out) {
    if (!byte_out) return -1;
    return kfifo_out(fifo, byte_out, 1) == 1 ? 0 : -1;
}
