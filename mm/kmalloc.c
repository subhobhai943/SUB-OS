#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <lib/string.h>
#include <kernel/sync.h>
#include <kernel/printk.h>

#define HEAP_MAGIC 0x53554248 // "SUBH"
#define HEAP_INITIAL_PAGES 8192 // 32 MB initial kernel heap
#define HEAP_GROW_PAGES    4096 // Grow the heap 16 MB at a time on demand

typedef struct heap_block {
    uint32_t magic;
    bool is_free;
    size_t size; // payload size in bytes
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

#define BLOCK_HEADER_SIZE sizeof(heap_block_t)

static heap_block_t* heap_start = NULL;
static heap_block_t* heap_tail  = NULL; // last block in address order, for O(1) growth
static size_t heap_used_bytes = 0;
static size_t heap_free_bytes = 0;
static size_t heap_total_bytes = 0;
static size_t heap_grow_count = 0;
static spinlock_t heap_lock = SPINLOCK_INIT;

/* Two blocks may only be coalesced when they are physically adjacent. The heap
 * is made of one or more regions carved from the PMM; a region boundary shows
 * up as a gap between a block and its list successor, so this guard keeps the
 * free-list merge from ever spanning two independent PMM allocations. */
static inline bool blocks_adjacent(heap_block_t* a, heap_block_t* b) {
    return (heap_block_t*)((uint8_t*)a + BLOCK_HEADER_SIZE + a->size) == b;
}

void heap_init(void) {
    void* initial_mem = pmm_alloc_pages(HEAP_INITIAL_PAGES);
    if (!initial_mem) {
        printk(KERN_ERR "Heap: Failed to allocate initial %d pages!\n", HEAP_INITIAL_PAGES);
        return;
    }

    heap_start = (heap_block_t*)initial_mem;
    heap_total_bytes = (size_t)HEAP_INITIAL_PAGES * PMM_PAGE_SIZE;

    heap_start->magic = HEAP_MAGIC;
    heap_start->is_free = true;
    heap_start->size = heap_total_bytes - BLOCK_HEADER_SIZE;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    heap_tail = heap_start;
    heap_used_bytes = 0;
    heap_free_bytes = heap_start->size;
}

/* Append a fresh region from the PMM to the tail of the free list. Must be
 * called with heap_lock held. Returns the new free block, or NULL if the PMM
 * cannot satisfy a contiguous run large enough for the request. */
static heap_block_t* heap_grow(size_t need) {
    size_t need_pages = (need + BLOCK_HEADER_SIZE + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    size_t grow_pages = need_pages > HEAP_GROW_PAGES ? need_pages : HEAP_GROW_PAGES;

    void* mem = pmm_alloc_pages(grow_pages);
    if (!mem && grow_pages != need_pages) {
        /* Could not get the generous chunk; fall back to exactly what is needed. */
        grow_pages = need_pages;
        mem = pmm_alloc_pages(grow_pages);
    }
    if (!mem) {
        return NULL;
    }

    heap_block_t* region = (heap_block_t*)mem;
    region->magic = HEAP_MAGIC;
    region->is_free = true;
    region->size = (size_t)grow_pages * PMM_PAGE_SIZE - BLOCK_HEADER_SIZE;
    region->next = NULL;
    region->prev = heap_tail;

    if (heap_tail) {
        heap_tail->next = region;
    }
    heap_tail = region;
    if (!heap_start) {
        heap_start = region;
    }

    heap_total_bytes += (size_t)grow_pages * PMM_PAGE_SIZE;
    heap_free_bytes  += region->size;
    heap_grow_count++;

    /* A PMM run handed back right after the previous tail is a real neighbour;
     * fuse it so the heap does not fragment purely from growing. */
    if (region->prev && region->prev->is_free && blocks_adjacent(region->prev, region)) {
        heap_block_t* prev = region->prev;
        prev->size += BLOCK_HEADER_SIZE + region->size;
        prev->next = region->next; // NULL
        heap_tail = prev;
        region = prev;
    }

    return region;
}

/* Carve `size` bytes out of a free block, splitting off the remainder. Must be
 * called with heap_lock held; `curr` must be free and large enough. */
static void* heap_take(heap_block_t* curr, size_t size) {
    if (curr->size >= size + BLOCK_HEADER_SIZE + 16) {
        heap_block_t* new_block = (heap_block_t*)((uint8_t*)curr + BLOCK_HEADER_SIZE + size);
        new_block->magic = HEAP_MAGIC;
        new_block->is_free = true;
        new_block->size = curr->size - size - BLOCK_HEADER_SIZE;
        new_block->next = curr->next;
        new_block->prev = curr;

        if (curr->next) {
            curr->next->prev = new_block;
        } else {
            heap_tail = new_block;
        }
        curr->next = new_block;
        curr->size = size;
    }

    curr->is_free = false;
    heap_used_bytes += curr->size;
    heap_free_bytes -= curr->size;

    return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes for SIMD/64-bit performance
    size = (size + 15) & ~15;

    spin_lock(&heap_lock);

    for (int attempt = 0; attempt < 2; attempt++) {
        heap_block_t* curr = heap_start;
        while (curr) {
            if (curr->magic != HEAP_MAGIC) {
                printk(KERN_ERR "Heap Corruption detected at %p!\n", curr);
                spin_unlock(&heap_lock);
                return NULL;
            }

            if (curr->is_free && curr->size >= size) {
                void* ptr = heap_take(curr, size);
                spin_unlock(&heap_lock);
                return ptr;
            }

            curr = curr->next;
        }

        /* No block fit. Pull a new region from the PMM and try once more. */
        if (attempt == 0 && heap_grow(size)) {
            continue;
        }
        break;
    }

    spin_unlock(&heap_lock);
    printk(KERN_WARNING "Heap: Out of memory for size %llu bytes!\n", (uint64_t)size);
    return NULL;
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    spin_lock(&heap_lock);

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != HEAP_MAGIC) {
        printk(KERN_ERR "kfree: Invalid pointer / heap corruption %p!\n", ptr);
        spin_unlock(&heap_lock);
        return;
    }

    block->is_free = true;
    heap_used_bytes -= block->size;
    heap_free_bytes += block->size;

    // Coalesce with next block if free AND physically adjacent (same region).
    if (block->next && block->next->is_free && blocks_adjacent(block, block->next)) {
        heap_block_t* nxt = block->next;
        block->size += BLOCK_HEADER_SIZE + nxt->size;
        block->next = nxt->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            heap_tail = block;
        }
    }

    // Coalesce with prev block if free AND physically adjacent (same region).
    if (block->prev && block->prev->is_free && blocks_adjacent(block->prev, block)) {
        heap_block_t* prev = block->prev;
        prev->size += BLOCK_HEADER_SIZE + block->size;
        prev->next = block->next;
        if (block->next) {
            block->next->prev = prev;
        } else {
            heap_tail = prev;
        }
    }

    spin_unlock(&heap_lock);
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != HEAP_MAGIC) return NULL;

    if (block->size >= new_size) {
        return ptr;
    }

    size_t old_size = block->size;
    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size);
        kfree(ptr);
    }
    return new_ptr;
}

size_t heap_get_used_bytes(void)  { return heap_used_bytes; }
size_t heap_get_free_bytes(void)  { return heap_free_bytes; }
size_t heap_get_total_bytes(void) { return heap_total_bytes; }
size_t heap_get_grow_count(void)  { return heap_grow_count; }
