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

#endif // _MM_KMALLOC_H
