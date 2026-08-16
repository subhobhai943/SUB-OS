#ifndef _MM_KMALLOC_H
#define _MM_KMALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void heap_init(void);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void* krealloc(void* ptr, size_t new_size);
void  kfree(void* ptr);

size_t heap_get_used_bytes(void);
size_t heap_get_free_bytes(void);
size_t heap_get_total_bytes(void);

static inline size_t kmalloc_get_used(void) { return heap_get_used_bytes(); }
static inline size_t kmalloc_get_free(void) { return heap_get_free_bytes(); }
static inline size_t kmalloc_get_total(void) { return heap_get_total_bytes(); }

#endif // _MM_KMALLOC_H
