#ifndef _MM_PAGE_CACHE_H
#define _MM_PAGE_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Write-back page cache sitting between the VFS and the block layer.
//
// Every entry caches PAGE_CACHE_BLOCK_SIZE bytes of one block device, keyed by
// (device, page index). Lookups go through a hash table; eviction follows a
// CLOCK approximation of LRU, and dirty pages are written back before reuse.

#define PAGE_CACHE_BLOCK_SIZE   4096
#define PAGE_CACHE_SECTORS      (PAGE_CACHE_BLOCK_SIZE / 512)
#define PAGE_CACHE_MAX_PAGES    64

typedef struct page_cache_entry {
    char      dev_name[32];
    uint64_t  page_index;      // Offset in PAGE_CACHE_BLOCK_SIZE units
    uint8_t*  data;
    bool      valid;
    bool      dirty;
    bool      referenced;      // CLOCK second-chance bit
    uint32_t  pin_count;
    uint64_t  last_access;
    uint64_t  hits;
} page_cache_entry_t;

typedef struct {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t writebacks;
    uint64_t read_ios;
    uint64_t write_ios;
    uint32_t resident_pages;
    uint32_t dirty_pages;
    size_t   memory_bytes;
} page_cache_stats_t;

void page_cache_init(void);

// Read/write through the cache. Offsets and lengths are arbitrary; the cache
// splits them across pages internally. Both return bytes transferred.
size_t page_cache_read(const char* dev_name, uint64_t offset, void* buf, size_t len);
size_t page_cache_write(const char* dev_name, uint64_t offset, const void* buf, size_t len);

// Borrow a page directly (pinned until page_cache_release).
page_cache_entry_t* page_cache_get(const char* dev_name, uint64_t page_index);
void                page_cache_release(page_cache_entry_t* entry);
void                page_cache_mark_dirty(page_cache_entry_t* entry);

int  page_cache_sync(void);                       // Flush every dirty page
int  page_cache_sync_dev(const char* dev_name);   // Flush one device
int  page_cache_invalidate(const char* dev_name); // Drop clean pages
void page_cache_drop_all(void);

page_cache_stats_t page_cache_get_stats(void);
uint32_t           page_cache_hit_percent(void);
void               page_cache_dump(void);

#endif // _MM_PAGE_CACHE_H
