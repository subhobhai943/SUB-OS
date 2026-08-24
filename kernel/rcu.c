// Tiny RCU implementation for SUB-OS
#include <kernel/rcu.h>
#include <kernel/sched.h>
#include <kernel/sync.h>
#include <kernel/printk.h>
#include <mm/kmalloc.h>
#include <arch/arch.h>
#include <lib/string.h>

typedef struct {
    rcu_callback_t func;
    void*          arg;
    uint64_t       gp_target;  // Grace period this callback is waiting on
    bool           in_use;
} rcu_head_t;

static rcu_head_t  g_callbacks[RCU_MAX_CALLBACKS];
static spinlock_t  g_rcu_lock = SPINLOCK_INIT;
static rcu_stats_t g_stats;
static uint64_t    g_grace_period = 0;
static uint32_t    g_reader_nesting = 0;
static bool        g_initialized = false;

static void rcu_free_cb(void* arg) {
    if (arg) kfree(arg);
}

void rcu_init(void) {
    spin_lock(&g_rcu_lock);
    memset(g_callbacks, 0, sizeof(g_callbacks));
    memset(&g_stats, 0, sizeof(g_stats));
    g_grace_period   = 1;
    g_reader_nesting = 0;
    g_initialized    = true;
    spin_unlock(&g_rcu_lock);
}

void rcu_read_lock(void) {
    __atomic_fetch_add(&g_reader_nesting, 1, __ATOMIC_ACQUIRE);
    g_stats.read_locks++;
}

void rcu_read_unlock(void) {
    if (__atomic_load_n(&g_reader_nesting, __ATOMIC_RELAXED) > 0) {
        __atomic_fetch_sub(&g_reader_nesting, 1, __ATOMIC_RELEASE);
    }
}

bool rcu_read_lock_held(void) {
    return __atomic_load_n(&g_reader_nesting, __ATOMIC_RELAXED) > 0;
}

uint64_t rcu_get_grace_period(void) {
    return g_grace_period;
}

// A grace period ends once no reader is inside a critical section. On this
// cooperative kernel that is observable directly from the nesting counter.
static bool grace_period_elapsed(void) {
    return __atomic_load_n(&g_reader_nesting, __ATOMIC_ACQUIRE) == 0;
}

void rcu_process_callbacks(void) {
    if (!g_initialized) return;
    if (!grace_period_elapsed()) return;

    spin_lock(&g_rcu_lock);
    uint64_t completed = ++g_grace_period;
    g_stats.grace_periods++;

    // Snapshot the ready callbacks, then run them outside the lock so a
    // reclaimer is free to allocate, free or queue more RCU work.
    rcu_head_t ready[RCU_MAX_CALLBACKS];
    int nready = 0;

    for (int i = 0; i < RCU_MAX_CALLBACKS; i++) {
        if (g_callbacks[i].in_use && completed >= g_callbacks[i].gp_target) {
            ready[nready++] = g_callbacks[i];
            g_callbacks[i].in_use = false;
            if (g_stats.pending > 0) g_stats.pending--;
        }
    }
    spin_unlock(&g_rcu_lock);

    for (int i = 0; i < nready; i++) {
        if (ready[i].func) ready[i].func(ready[i].arg);
        g_stats.callbacks_invoked++;
    }
}

void rcu_synchronize(void) {
    if (!g_initialized) return;

    uint64_t start = g_grace_period;
    uint64_t deadline = pit_get_ticks() + 200; // 2 s ceiling at 100 Hz

    while (g_grace_period <= start) {
        rcu_process_callbacks();
        if (g_grace_period > start) break;
        if (pit_get_ticks() >= deadline) break;
        sched_yield();
    }
}

int call_rcu(rcu_callback_t func, void* arg) {
    if (!g_initialized || !func) return -1;

    spin_lock(&g_rcu_lock);
    for (int i = 0; i < RCU_MAX_CALLBACKS; i++) {
        if (!g_callbacks[i].in_use) {
            g_callbacks[i].func      = func;
            g_callbacks[i].arg       = arg;
            g_callbacks[i].gp_target = g_grace_period + 1;
            g_callbacks[i].in_use    = true;
            g_stats.callbacks_queued++;
            g_stats.pending++;
            spin_unlock(&g_rcu_lock);
            return 0;
        }
    }
    spin_unlock(&g_rcu_lock);

    // Callback ring is full: force a grace period and retry once.
    rcu_process_callbacks();

    spin_lock(&g_rcu_lock);
    for (int i = 0; i < RCU_MAX_CALLBACKS; i++) {
        if (!g_callbacks[i].in_use) {
            g_callbacks[i].func      = func;
            g_callbacks[i].arg       = arg;
            g_callbacks[i].gp_target = g_grace_period + 1;
            g_callbacks[i].in_use    = true;
            g_stats.callbacks_queued++;
            g_stats.pending++;
            spin_unlock(&g_rcu_lock);
            return 0;
        }
    }
    spin_unlock(&g_rcu_lock);
    return -1;
}

int kfree_rcu(void* ptr) {
    if (!ptr) return 0;
    return call_rcu(rcu_free_cb, ptr);
}

rcu_stats_t rcu_get_stats(void) {
    rcu_stats_t s = g_stats;
    s.nesting_depth = __atomic_load_n(&g_reader_nesting, __ATOMIC_RELAXED);
    return s;
}

void rcu_dump(void) {
    rcu_stats_t s = rcu_get_stats();

    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Tiny RCU Subsystem ===\n" ANSI_RESET);
    printk("  State             : %s\n",
           g_initialized ? ANSI_BRIGHT_GREEN "active" ANSI_RESET
                         : ANSI_YELLOW "uninitialized" ANSI_RESET);
    printk("  Grace period      : #%llu\n", (unsigned long long)g_grace_period);
    printk("  Completed periods : %llu\n", (unsigned long long)s.grace_periods);
    printk("  Reader nesting    : %u %s\n", s.nesting_depth,
           s.nesting_depth ? "(grace period stalled)" : "(quiescent)");
    printk("  rcu_read_lock()   : %llu calls\n", (unsigned long long)s.read_locks);
    printk("  Callbacks queued  : %llu\n", (unsigned long long)s.callbacks_queued);
    printk("  Callbacks invoked : %llu\n", (unsigned long long)s.callbacks_invoked);
    printk("  Callbacks pending : %u / %d slots\n", s.pending, RCU_MAX_CALLBACKS);
}
