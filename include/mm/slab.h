#ifndef _MM_SLAB_H
#define _MM_SLAB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SLAB_NAME_MAX 32
#define SLAB_MAX_CACHES 32

typedef struct kmem_cache kmem_cache_t;

typedef void (*kmem_ctor_fn_t)(void* obj);
typedef void (*kmem_dtor_fn_t)(void* obj);

typedef struct kmem_slab {
    void* memory;
    uint32_t total_objects;
    uint32_t free_objects;
    uint32_t* freelist;
    uint32_t freelist_head;
    struct kmem_slab* next;
} kmem_slab_t;

struct kmem_cache {
    char name[SLAB_NAME_MAX];
    size_t object_size;
    size_t alignment;
    size_t objects_per_slab;
    uint32_t active_objects;
    uint32_t total_slabs;
    kmem_ctor_fn_t ctor;
    kmem_dtor_fn_t dtor;
    kmem_slab_t* slabs_partial;
    kmem_slab_t* slabs_full;
    kmem_slab_t* slabs_empty;
    bool in_use;
};

void slab_init(void);
kmem_cache_t* kmem_cache_create(const char* name, size_t size, size_t align, kmem_ctor_fn_t ctor, kmem_dtor_fn_t dtor);
void* kmem_cache_alloc(kmem_cache_t* cache);
void kmem_cache_free(kmem_cache_t* cache, void* obj);
int kmem_cache_destroy(kmem_cache_t* cache);

size_t slab_get_cache_count(void);
const kmem_cache_t* slab_get_cache(size_t index);

#endif // _MM_SLAB_H
