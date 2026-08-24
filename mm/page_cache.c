// Write-back block device page cache for SUB-OS
#include <mm/page_cache.h>
#include <mm/kmalloc.h>
#include <block/block.h>
#include <lib/hashtable.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/sync.h>
#include <kernel/printk.h>
#include <arch/arch.h>

static page_cache_entry_t g_pages[PAGE_CACHE_MAX_PAGES];
static hashtable_t*       g_index = NULL;
static page_cache_stats_t g_stats;
static spinlock_t         g_lock = SPINLOCK_INIT;
static uint32_t           g_clock_hand = 0;
static bool               g_initialized = false;

// The hash key encodes both the device and the page index so a single table
// serves every registered block device.
static void make_key(char* out, size_t out_len, const char* dev, uint64_t idx) {
    snprintf(out, out_len, "%s:%llu", dev ? dev : "?", (unsigned long long)idx);
}

void page_cache_init(void) {
    memset(g_pages, 0, sizeof(g_pages));
    memset(&g_stats, 0, sizeof(g_stats));
    spinlock_init(&g_lock);
    g_clock_hand = 0;

    g_index = hashtable_create(PAGE_CACHE_MAX_PAGES * 2, true);
    g_initialized = (g_index != NULL);
}

static int page_read_from_disk(page_cache_entry_t* e) {
    block_device_t* dev = block_get_device(e->dev_name);
    if (!dev || !dev->read_sectors) return -1;

    uint64_t sector = e->page_index * PAGE_CACHE_SECTORS;
    if (sector + PAGE_CACHE_SECTORS > dev->total_sectors) {
        // Reads past the end of the device yield zeroes rather than an error,
        // matching how the VFS treats sparse tails.
        memset(e->data, 0, PAGE_CACHE_BLOCK_SIZE);
        return 0;
    }

    int rc = dev->read_sectors(dev, sector, PAGE_CACHE_SECTORS, e->data);
    if (rc == 0) g_stats.read_ios++;
    return rc;
}

static int page_write_to_disk(page_cache_entry_t* e) {
    block_device_t* dev = block_get_device(e->dev_name);
    if (!dev || !dev->write_sectors || dev->read_only) return -1;

    uint64_t sector = e->page_index * PAGE_CACHE_SECTORS;
    if (sector + PAGE_CACHE_SECTORS > dev->total_sectors) return -1;

    int rc = dev->write_sectors(dev, sector, PAGE_CACHE_SECTORS, e->data);
    if (rc == 0) {
        g_stats.write_ios++;
        g_stats.writebacks++;
    }
    return rc;
}

static void unindex(page_cache_entry_t* e) {
    char key[64];
    make_key(key, sizeof(key), e->dev_name, e->page_index);
    hashtable_remove(g_index, key);
}

// CLOCK eviction: sweep the ring clearing reference bits until an unpinned,
// unreferenced page turns up. Dirty victims are written back first.
static page_cache_entry_t* evict_one(void) {
    for (int sweep = 0; sweep < PAGE_CACHE_MAX_PAGES * 2; sweep++) {
        page_cache_entry_t* e = &g_pages[g_clock_hand];
        g_clock_hand = (g_clock_hand + 1) % PAGE_CACHE_MAX_PAGES;

        if (!e->valid) return e;
        if (e->pin_count > 0) continue;

        if (e->referenced) {
            e->referenced = false;
            continue;
        }

        if (e->dirty) {
            if (page_write_to_disk(e) != 0) continue; // Cannot flush: skip it
            e->dirty = false;
            if (g_stats.dirty_pages > 0) g_stats.dirty_pages--;
        }

        unindex(e);
        e->valid = false;
        if (g_stats.resident_pages > 0) g_stats.resident_pages--;
        g_stats.evictions++;
        return e;
    }
    return NULL;
}

static page_cache_entry_t* lookup_locked(const char* dev_name, uint64_t page_index) {
    char key[64];
    make_key(key, sizeof(key), dev_name, page_index);
    return (page_cache_entry_t*)hashtable_get(g_index, key);
}

page_cache_entry_t* page_cache_get(const char* dev_name, uint64_t page_index) {
    if (!g_initialized || !dev_name) return NULL;

    spin_lock(&g_lock);

    page_cache_entry_t* e = lookup_locked(dev_name, page_index);
    if (e && e->valid) {
        e->referenced = true;
        e->last_access = pit_get_ticks();
        e->pin_count++;
        e->hits++;
        g_stats.hits++;
        spin_unlock(&g_lock);
        return e;
    }

    g_stats.misses++;

    e = evict_one();
    if (!e) {
        spin_unlock(&g_lock);
        return NULL;
    }

    if (!e->data) {
        e->data = (uint8_t*)kmalloc(PAGE_CACHE_BLOCK_SIZE);
        if (!e->data) {
            spin_unlock(&g_lock);
            return NULL;
        }
        g_stats.memory_bytes += PAGE_CACHE_BLOCK_SIZE;
    }

    strncpy(e->dev_name, dev_name, sizeof(e->dev_name) - 1);
    e->dev_name[sizeof(e->dev_name) - 1] = '\0';
    e->page_index = page_index;
    e->dirty      = false;
    e->referenced = true;
    e->pin_count  = 1;
    e->hits       = 0;
    e->last_access = pit_get_ticks();

    if (page_read_from_disk(e) != 0) {
        memset(e->data, 0, PAGE_CACHE_BLOCK_SIZE);
    }

    e->valid = true;
    g_stats.resident_pages++;

    char key[64];
    make_key(key, sizeof(key), dev_name, page_index);
    hashtable_put(g_index, key, e);

    spin_unlock(&g_lock);
    return e;
}

void page_cache_release(page_cache_entry_t* entry) {
    if (!entry) return;
    spin_lock(&g_lock);
    if (entry->pin_count > 0) entry->pin_count--;
    spin_unlock(&g_lock);
}

void page_cache_mark_dirty(page_cache_entry_t* entry) {
    if (!entry || !entry->valid) return;
    spin_lock(&g_lock);
    if (!entry->dirty) {
        entry->dirty = true;
        g_stats.dirty_pages++;
    }
    spin_unlock(&g_lock);
}

size_t page_cache_read(const char* dev_name, uint64_t offset, void* buf, size_t len) {
    if (!g_initialized || !dev_name || !buf || len == 0) return 0;

    uint8_t* out = (uint8_t*)buf;
    size_t   done = 0;

    while (done < len) {
        uint64_t page_index = (offset + done) / PAGE_CACHE_BLOCK_SIZE;
        size_t   page_off   = (size_t)((offset + done) % PAGE_CACHE_BLOCK_SIZE);
        size_t   chunk      = PAGE_CACHE_BLOCK_SIZE - page_off;
        if (chunk > len - done) chunk = len - done;

        page_cache_entry_t* e = page_cache_get(dev_name, page_index);
        if (!e) break;

        memcpy(out + done, e->data + page_off, chunk);
        page_cache_release(e);
        done += chunk;
    }

    return done;
}

size_t page_cache_write(const char* dev_name, uint64_t offset, const void* buf, size_t len) {
    if (!g_initialized || !dev_name || !buf || len == 0) return 0;

    const uint8_t* in = (const uint8_t*)buf;
    size_t done = 0;

    while (done < len) {
        uint64_t page_index = (offset + done) / PAGE_CACHE_BLOCK_SIZE;
        size_t   page_off   = (size_t)((offset + done) % PAGE_CACHE_BLOCK_SIZE);
        size_t   chunk      = PAGE_CACHE_BLOCK_SIZE - page_off;
        if (chunk > len - done) chunk = len - done;

        page_cache_entry_t* e = page_cache_get(dev_name, page_index);
        if (!e) break;

        memcpy(e->data + page_off, in + done, chunk);
        page_cache_mark_dirty(e);
        page_cache_release(e);
        done += chunk;
    }

    return done;
}

int page_cache_sync_dev(const char* dev_name) {
    if (!g_initialized) return -1;

    int flushed = 0;
    spin_lock(&g_lock);
    for (int i = 0; i < PAGE_CACHE_MAX_PAGES; i++) {
        page_cache_entry_t* e = &g_pages[i];
        if (!e->valid || !e->dirty) continue;
        if (dev_name && strcmp(e->dev_name, dev_name) != 0) continue;

        if (page_write_to_disk(e) == 0) {
            e->dirty = false;
            if (g_stats.dirty_pages > 0) g_stats.dirty_pages--;
            flushed++;
        }
    }
    spin_unlock(&g_lock);
    return flushed;
}

int page_cache_sync(void) {
    return page_cache_sync_dev(NULL);
}

int page_cache_invalidate(const char* dev_name) {
    if (!g_initialized) return -1;

    int dropped = 0;
    spin_lock(&g_lock);
    for (int i = 0; i < PAGE_CACHE_MAX_PAGES; i++) {
        page_cache_entry_t* e = &g_pages[i];
        if (!e->valid || e->dirty || e->pin_count > 0) continue;
        if (dev_name && strcmp(e->dev_name, dev_name) != 0) continue;

        unindex(e);
        e->valid = false;
        if (g_stats.resident_pages > 0) g_stats.resident_pages--;
        dropped++;
    }
    spin_unlock(&g_lock);
    return dropped;
}

void page_cache_drop_all(void) {
    page_cache_sync();
    page_cache_invalidate(NULL);
}

page_cache_stats_t page_cache_get_stats(void) {
    return g_stats;
}

uint32_t page_cache_hit_percent(void) {
    uint64_t total = g_stats.hits + g_stats.misses;
    if (total == 0) return 0;
    return (uint32_t)((g_stats.hits * 100) / total);
}

void page_cache_dump(void) {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Block Page Cache ===\n" ANSI_RESET);
    if (!g_initialized) {
        printk(ANSI_YELLOW "  Page cache not initialized\n" ANSI_RESET);
        return;
    }

    printk("  Page size    : %d bytes (%d sectors)\n",
           PAGE_CACHE_BLOCK_SIZE, PAGE_CACHE_SECTORS);
    printk("  Resident     : %u / %d pages (%llu KB allocated)\n",
           g_stats.resident_pages, PAGE_CACHE_MAX_PAGES,
           (unsigned long long)(g_stats.memory_bytes / 1024));
    printk("  Dirty        : %u pages\n", g_stats.dirty_pages);
    printk("  Hits/Misses  : %llu / %llu (" ANSI_BRIGHT_GREEN "%u%% hit rate" ANSI_RESET ")\n",
           (unsigned long long)g_stats.hits,
           (unsigned long long)g_stats.misses,
           page_cache_hit_percent());
    printk("  Evictions    : %llu (CLOCK second-chance)\n",
           (unsigned long long)g_stats.evictions);
    printk("  Block I/O    : %llu reads, %llu writes (%llu write-backs)\n\n",
           (unsigned long long)g_stats.read_ios,
           (unsigned long long)g_stats.write_ios,
           (unsigned long long)g_stats.writebacks);

    printk(ANSI_YELLOW "  %-12s %10s %6s %6s %6s %8s\n" ANSI_RESET,
           "DEVICE", "PAGE", "DIRTY", "PIN", "REF", "HITS");

    int shown = 0;
    for (int i = 0; i < PAGE_CACHE_MAX_PAGES && shown < 16; i++) {
        page_cache_entry_t* e = &g_pages[i];
        if (!e->valid) continue;
        printk("  %-12s %10llu %6s %6u %6s %8llu\n",
               e->dev_name, (unsigned long long)e->page_index,
               e->dirty ? ANSI_RED "yes" ANSI_RESET : "no",
               e->pin_count,
               e->referenced ? "yes" : "no",
               (unsigned long long)e->hits);
        shown++;
    }
    if (shown == 0) printk(ANSI_BRIGHT_BLACK "  (cache is empty)\n" ANSI_RESET);

    printk("\n  Index: %llu entries in %llu buckets, longest chain %llu\n",
           (unsigned long long)hashtable_size(g_index),
           (unsigned long long)hashtable_buckets(g_index),
           (unsigned long long)hashtable_longest_chain(g_index));
}
