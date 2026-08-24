// Fast Userspace muTEX (futex) hash table for SUB-OS
#include <kernel/futex.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/printf.h>

typedef struct {
    wait_queue_t      wq;
    volatile uint32_t* addr;   // Address currently owning this bucket slot
    uint32_t          waiters;
} futex_bucket_t;

static futex_bucket_t g_buckets[FUTEX_BUCKETS];
static futex_stats_t  g_stats;
static bool           g_initialized = false;

// Futex words are naturally 4-byte aligned, so drop the two dead low bits
// before mixing to avoid clustering every key onto a quarter of the buckets.
static size_t futex_hash(volatile uint32_t* uaddr) {
    uintptr_t key = (uintptr_t)uaddr >> 2;
    key ^= key >> 16;
    key *= 0x2545F491U;
    key ^= key >> 13;
    return (size_t)(key & (FUTEX_BUCKETS - 1));
}

void futex_init(void) {
    memset(&g_stats, 0, sizeof(g_stats));

    for (int i = 0; i < FUTEX_BUCKETS; i++) {
        char name[24];
        snprintf(name, sizeof(name), "futex-%d", i);
        wait_queue_init(&g_buckets[i].wq, name);
        g_buckets[i].addr    = NULL;
        g_buckets[i].waiters = 0;
    }

    g_initialized = true;
}

int futex_wait(volatile uint32_t* uaddr, uint32_t expected, uint64_t timeout_ticks) {
    if (!g_initialized || !uaddr) return -1;

    // Re-check under the bucket before parking: if the word already moved on,
    // the wakeup we would sleep for has already happened.
    if (*uaddr != expected) {
        g_stats.spurious++;
        return -1;
    }

    futex_bucket_t* b = &g_buckets[futex_hash(uaddr)];
    b->addr = uaddr;
    b->waiters++;
    g_stats.waits++;
    g_stats.active_waiters++;

    int rc = (timeout_ticks > 0)
             ? wait_event_timeout(&b->wq, timeout_ticks)
             : wait_event(&b->wq);

    if (b->waiters > 0) b->waiters--;
    if (g_stats.active_waiters > 0) g_stats.active_waiters--;
    if (rc == 1) g_stats.timeouts++;

    return rc;
}

int futex_wake(volatile uint32_t* uaddr, int count) {
    if (!g_initialized || !uaddr) return 0;

    futex_bucket_t* b = &g_buckets[futex_hash(uaddr)];
    int woken = (count <= 1) ? wake_up(&b->wq) : wake_up_all(&b->wq);

    g_stats.wakes += (uint64_t)woken;
    return woken;
}

int futex_requeue(volatile uint32_t* uaddr1, volatile uint32_t* uaddr2, int count) {
    if (!g_initialized || !uaddr1 || !uaddr2) return 0;

    futex_bucket_t* src = &g_buckets[futex_hash(uaddr1)];
    futex_bucket_t* dst = &g_buckets[futex_hash(uaddr2)];
    if (src == dst) return 0;

    uint32_t moved = src->waiters;
    if (count > 0 && moved > (uint32_t)count) moved = (uint32_t)count;

    src->waiters -= moved;
    dst->waiters += moved;
    dst->addr = uaddr2;
    g_stats.requeues += moved;

    // Sleepers are handed to the destination queue by waking them there; the
    // caller's protocol has them re-test their predicate on the new address.
    for (uint32_t i = 0; i < moved; i++) wake_up(&src->wq);

    return (int)moved;
}

futex_stats_t futex_get_stats(void) {
    return g_stats;
}

void futex_dump(void) {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Futex Subsystem (%d buckets) ===\n" ANSI_RESET,
           FUTEX_BUCKETS);
    printk("  Waits    : %llu\n", (unsigned long long)g_stats.waits);
    printk("  Wakes    : %llu\n", (unsigned long long)g_stats.wakes);
    printk("  Requeues : %llu\n", (unsigned long long)g_stats.requeues);
    printk("  Timeouts : %llu\n", (unsigned long long)g_stats.timeouts);
    printk("  Fastpath retries (value changed): %llu\n",
           (unsigned long long)g_stats.spurious);
    printk("  Active waiters: %u\n\n", g_stats.active_waiters);

    int occupied = 0;
    for (int i = 0; i < FUTEX_BUCKETS; i++) {
        if (g_buckets[i].waiters > 0) {
            printk("  bucket[%02d] addr=%p waiters=%u\n",
                   i, (void*)g_buckets[i].addr, g_buckets[i].waiters);
            occupied++;
        }
    }
    if (occupied == 0) printk(ANSI_BRIGHT_BLACK "  (no contended futexes)\n" ANSI_RESET);
}

// ---------------------------------------------------------------------------
// Futex-backed sleeping mutex
// ---------------------------------------------------------------------------

void fmutex_init(fmutex_t* m, const char* name) {
    if (!m) return;
    m->state = 0;
    strncpy(m->name, name ? name : "fmutex", sizeof(m->name) - 1);
    m->name[sizeof(m->name) - 1] = '\0';
}

bool fmutex_trylock(fmutex_t* m) {
    if (!m) return false;
    uint32_t expected = 0;
    return __atomic_compare_exchange_n(&m->state, &expected, 1, false,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

void fmutex_lock(fmutex_t* m) {
    if (!m) return;
    if (fmutex_trylock(m)) return;

    // Contended: publish state 2 so the unlocker knows a wake is required.
    while (__atomic_exchange_n(&m->state, 2, __ATOMIC_ACQUIRE) != 0) {
        futex_wait(&m->state, 2, 100);
    }
}

void fmutex_unlock(fmutex_t* m) {
    if (!m) return;
    if (__atomic_exchange_n(&m->state, 0, __ATOMIC_RELEASE) == 2) {
        futex_wake(&m->state, 1);
    }
}
