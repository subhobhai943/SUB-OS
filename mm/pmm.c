#include <mm/pmm.h>
#include <lib/bitmap.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_PHYSICAL_PAGES (1024 * 1024) // 4 GB (1,048,576 4KB pages)
#define BITMAP_SIZE        (MAX_PHYSICAL_PAGES / 64)

static bitmap_word_t page_bitmap[BITMAP_SIZE];
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;
static uint64_t total_memory_bytes = 0;
static uint64_t usable_memory_bytes = 0;

void pmm_init(void* memory_map, uint64_t memory_map_count) {
    // Initially mark all pages as reserved (1 in bitmap)
    memset(page_bitmap, 0xFF, sizeof(page_bitmap));
    total_pages = 0;
    used_pages = 0;
    total_memory_bytes = 0;
    usable_memory_bytes = 0;

    e820_entry_t* entries = (e820_entry_t*)memory_map;

    for (uint64_t i = 0; i < memory_map_count; i++) {
        uint64_t base = entries[i].base;
        uint64_t length = entries[i].length;
        uint32_t type = entries[i].type;

        total_memory_bytes += length;

        if (type == E820_TYPE_USABLE) {
            usable_memory_bytes += length;

            uint64_t start_page = base / PMM_PAGE_SIZE;
            uint64_t page_count = length / PMM_PAGE_SIZE;

            for (uint64_t p = 0; p < page_count; p++) {
                uint64_t page = start_page + p;
                if (page < MAX_PHYSICAL_PAGES) {
                    bitmap_clear(page_bitmap, page);
                    if (page >= total_pages) {
                        total_pages = page + 1;
                    }
                }
            }
        }
    }

    // Fallback if no E820 usable regions
    if (usable_memory_bytes == 0 || total_pages == 0) {
#if defined(__aarch64__) || defined(__arm__) || defined(__armv8i__)
        uint64_t dram_start = 0x40000000;
        uint64_t dram_size  = 128ULL * 1024 * 1024;
        uint64_t start_page = dram_start / PMM_PAGE_SIZE;
        uint64_t count_pages = dram_size / PMM_PAGE_SIZE;

        total_pages = start_page + count_pages;
        total_memory_bytes = dram_size;
        usable_memory_bytes = dram_size;

        for (uint64_t p = 0; p < count_pages; p++) {
            bitmap_clear(page_bitmap, start_page + p);
        }

        // Reserve kernel & boot area (0x40000000 - 0x40800000: 8MB / 2048 pages)
        for (uint64_t p = start_page; p < start_page + 2048; p++) {
            bitmap_set(page_bitmap, p);
        }
#else
        total_pages = 32768; // 128 MB
        total_memory_bytes = 128ULL * 1024 * 1024;
        usable_memory_bytes = 128ULL * 1024 * 1024;
        for (uint64_t p = 0; p < total_pages; p++) {
            bitmap_clear(page_bitmap, p);
        }

        // Reserve lower 4 MB (bootloader, page tables, kernel image)
        size_t kernel_reserved_pages = (4 * 1024 * 1024) / PMM_PAGE_SIZE; // 1024 pages
        for (size_t p = 0; p < kernel_reserved_pages; p++) {
            bitmap_set(page_bitmap, p);
        }
#endif
    }

    // Count actual used pages (bits set in bitmap up to total_pages)
    used_pages = 0;
    for (uint64_t p = 0; p < total_pages; p++) {
        if (bitmap_test(page_bitmap, p)) {
            used_pages++;
        }
    }

    printk(KERN_INFO "PMM: Total Memory: %llu MB, Usable: %llu MB, Free Pages: %llu\n",
           total_memory_bytes / (1024 * 1024),
           usable_memory_bytes / (1024 * 1024),
           pmm_get_free_pages());
}

void* pmm_alloc_page(void) {
    int64_t page = bitmap_find_first_free(page_bitmap, total_pages);
    if (page < 0) {
        return NULL; // Out of memory
    }

    bitmap_set(page_bitmap, (size_t)page);
    used_pages++;
    return (void*)(page * PMM_PAGE_SIZE);
}

void* pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    if (count == 1) return pmm_alloc_page();

    int64_t start_page = bitmap_find_contiguous_free(page_bitmap, total_pages, count);
    if (start_page < 0) return NULL;

    for (size_t i = 0; i < count; i++) {
        bitmap_set(page_bitmap, (size_t)(start_page + i));
    }
    used_pages += count;
    return (void*)(start_page * PMM_PAGE_SIZE);
}

void pmm_free_page(void* page) {
    uint64_t page_idx = (uint64_t)page / PMM_PAGE_SIZE;
    if (page_idx < total_pages && bitmap_test(page_bitmap, page_idx)) {
        bitmap_clear(page_bitmap, page_idx);
        used_pages--;
    }
}

void pmm_free_pages(void* page, size_t count) {
    uint64_t start_idx = (uint64_t)page / PMM_PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        uint64_t page_idx = start_idx + i;
        if (page_idx < total_pages && bitmap_test(page_bitmap, page_idx)) {
            bitmap_clear(page_bitmap, page_idx);
            used_pages--;
        }
    }
}

uint64_t pmm_get_total_pages(void)   { return total_pages; }
uint64_t pmm_get_used_pages(void)    { return used_pages; }
uint64_t pmm_get_free_pages(void)    { return (total_pages > used_pages) ? (total_pages - used_pages) : 0; }
uint64_t pmm_get_total_memory(void)  { return total_memory_bytes; }
uint64_t pmm_get_usable_memory(void) { return usable_memory_bytes; }
