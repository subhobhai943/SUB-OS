#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <lib/string.h>
#include <kernel/sync.h>
#include <kernel/printk.h>

#define HEAP_MAGIC 0x53554248 // "SUBH"
#define HEAP_INITIAL_PAGES 4096 // 16 MB initial kernel heap

typedef struct heap_block {
    uint32_t magic;
    bool is_free;
    size_t size; // payload size in bytes
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

#define BLOCK_HEADER_SIZE sizeof(heap_block_t)

static heap_block_t* heap_start = NULL;
static size_t heap_used_bytes = 0;
static size_t heap_free_bytes = 0;
static size_t heap_total_bytes = 0;
static spinlock_t heap_lock = SPINLOCK_INIT;

void heap_init(void) {
    void* initial_mem = pmm_alloc_pages(HEAP_INITIAL_PAGES);
    if (!initial_mem) {
        printk(KERN_ERR "Heap: Failed to allocate initial %d pages!\n", HEAP_INITIAL_PAGES);
        return;
    }

    heap_start = (heap_block_t*)initial_mem;
    heap_total_bytes = HEAP_INITIAL_PAGES * PMM_PAGE_SIZE;

    heap_start->magic = HEAP_MAGIC;
    heap_start->is_free = true;
    heap_start->size = heap_total_bytes - BLOCK_HEADER_SIZE;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    heap_used_bytes = 0;
    heap_free_bytes = heap_start->size;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes for SIMD/64-bit performance
    size = (size + 15) & ~15;

    spin_lock(&heap_lock);

    heap_block_t* curr = heap_start;
    while (curr) {
        if (curr->magic != HEAP_MAGIC) {
            printk(KERN_ERR "Heap Corruption detected at %p!\n", curr);
            spin_unlock(&heap_lock);
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            // Check if block can be split
            if (curr->size >= size + BLOCK_HEADER_SIZE + 16) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)curr + BLOCK_HEADER_SIZE + size);
                new_block->magic = HEAP_MAGIC;
                new_block->is_free = true;
                new_block->size = curr->size - size - BLOCK_HEADER_SIZE;
                new_block->next = curr->next;
                new_block->prev = curr;

                if (curr->next) {
                    curr->next->prev = new_block;
                }
                curr->next = new_block;
                curr->size = size;
            }

            curr->is_free = false;
            heap_used_bytes += curr->size;
            heap_free_bytes -= curr->size;

            spin_unlock(&heap_lock);
            return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
        }

        curr = curr->next;
    }

    spin_unlock(&heap_lock);
    printk(KERN_WARNING "Heap: Out of memory for size %llu bytes!\n", size);
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

    // Coalesce with next block if free
    if (block->next && block->next->is_free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    // Coalesce with prev block if free
    if (block->prev && block->prev->is_free) {
        block->prev->size += BLOCK_HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
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

    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}

size_t heap_get_used_bytes(void)  { return heap_used_bytes; }
size_t heap_get_free_bytes(void)  { return heap_free_bytes; }
size_t heap_get_total_bytes(void) { return heap_total_bytes; }
