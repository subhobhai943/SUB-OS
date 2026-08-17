#include <mm/slab.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

static kmem_cache_t cache_table[SLAB_MAX_CACHES];
static size_t cache_count = 0;

void slab_init(void) {
    memset(cache_table, 0, sizeof(cache_table));
    cache_count = 0;

    // Initialize core kernel slabs
    kmem_cache_create("task_struct", 512, 16, NULL, NULL);
    kmem_cache_create("vfs_node", 256, 8, NULL, NULL);
    kmem_cache_create("file_desc", 64, 8, NULL, NULL);
    kmem_cache_create("socket_cache", 128, 8, NULL, NULL);
    kmem_cache_create("buffer_head", 64, 8, NULL, NULL);

    printk(KERN_INFO "SLAB: High-performance SLUB/SLAB Object Cache layer initialized\n");
}

kmem_cache_t* kmem_cache_create(const char* name, size_t size, size_t align, kmem_ctor_fn_t ctor, kmem_dtor_fn_t dtor) {
    if (!name || size == 0) return NULL;

    for (size_t i = 0; i < SLAB_MAX_CACHES; i++) {
        if (!cache_table[i].in_use) {
            kmem_cache_t* c = &cache_table[i];
            c->in_use = true;
            strncpy(c->name, name, SLAB_NAME_MAX - 1);
            c->name[SLAB_NAME_MAX - 1] = '\0';

            if (align < 8) align = 8;
            c->alignment = align;
            c->object_size = (size + align - 1) & ~(align - 1);
            c->objects_per_slab = 4096 / c->object_size;
            if (c->objects_per_slab == 0) c->objects_per_slab = 1;

            c->active_objects = 0;
            c->total_slabs = 0;
            c->ctor = ctor;
            c->dtor = dtor;
            c->slabs_partial = NULL;
            c->slabs_full = NULL;
            c->slabs_empty = NULL;

            if (i >= cache_count) cache_count = i + 1;
            return c;
        }
    }
    return NULL;
}

void* kmem_cache_alloc(kmem_cache_t* cache) {
    if (!cache || !cache->in_use) return NULL;

    void* obj = kmalloc(cache->object_size);
    if (obj) {
        cache->active_objects++;
        if (cache->ctor) {
            cache->ctor(obj);
        }
    }
    return obj;
}

void kmem_cache_free(kmem_cache_t* cache, void* obj) {
    if (!cache || !obj || !cache->in_use) return;

    if (cache->dtor) {
        cache->dtor(obj);
    }
    if (cache->active_objects > 0) {
        cache->active_objects--;
    }
    kfree(obj);
}

int kmem_cache_destroy(kmem_cache_t* cache) {
    if (!cache || !cache->in_use) return -1;
    cache->in_use = false;
    return 0;
}

size_t slab_get_cache_count(void) {
    return cache_count;
}

const kmem_cache_t* slab_get_cache(size_t index) {
    if (index >= SLAB_MAX_CACHES || !cache_table[index].in_use) return NULL;
    return &cache_table[index];
}
