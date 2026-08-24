// Wait queues and completion barriers for SUB-OS
#include <kernel/wait.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <arch/arch.h>
#include <lib/string.h>

#define WAIT_MAX_SLEEPERS 32
#define WAIT_TRACKED_QUEUES 16

// Wait entries live in a static pool so that a sleeping path never needs the
// heap (kmalloc may itself block on a wait queue in future revisions).
static wait_entry_t g_entry_pool[WAIT_MAX_SLEEPERS];
static bool         g_entry_used[WAIT_MAX_SLEEPERS];
static spinlock_t   g_pool_lock = SPINLOCK_INIT;

static wait_queue_t* g_tracked[WAIT_TRACKED_QUEUES];
static int           g_tracked_count = 0;

static wait_entry_t* entry_alloc(void) {
    spin_lock(&g_pool_lock);
    for (int i = 0; i < WAIT_MAX_SLEEPERS; i++) {
        if (!g_entry_used[i]) {
            g_entry_used[i] = true;
            spin_unlock(&g_pool_lock);
            memset(&g_entry_pool[i], 0, sizeof(wait_entry_t));
            return &g_entry_pool[i];
        }
    }
    spin_unlock(&g_pool_lock);
    return NULL;
}

static void entry_free(wait_entry_t* e) {
    if (!e) return;
    size_t idx = (size_t)(e - g_entry_pool);
    if (idx >= WAIT_MAX_SLEEPERS) return;
    spin_lock(&g_pool_lock);
    g_entry_used[idx] = false;
    spin_unlock(&g_pool_lock);
}

void wait_queue_init(wait_queue_t* wq, const char* name) {
    if (!wq) return;

    memset(wq, 0, sizeof(*wq));
    INIT_LIST_HEAD(&wq->waiters);
    spinlock_init(&wq->lock);
    strncpy(wq->name, name ? name : "wq", sizeof(wq->name) - 1);

    if (g_tracked_count < WAIT_TRACKED_QUEUES) {
        g_tracked[g_tracked_count++] = wq;
    }
}

// Enqueue the caller and return its pool entry, or NULL when the pool is dry.
static wait_entry_t* wait_prepare(wait_queue_t* wq) {
    wait_entry_t* e = entry_alloc();
    if (!e) return NULL;

    e->task  = task_current();
    e->woken = false;

    spin_lock(&wq->lock);
    list_add_tail(&e->node, &wq->waiters);
    wq->nr_waiting++;
    wq->sleeps++;
    spin_unlock(&wq->lock);

    if (e->task) e->task->state = TASK_STATE_BLOCKED;
    return e;
}

static void wait_finish(wait_queue_t* wq, wait_entry_t* e) {
    spin_lock(&wq->lock);
    if (e->node.next) {
        list_del(&e->node);
        if (wq->nr_waiting > 0) wq->nr_waiting--;
    }
    spin_unlock(&wq->lock);

    if (e->task) e->task->state = TASK_STATE_READY;
    entry_free(e);
}

int wait_event(wait_queue_t* wq) {
    if (!wq) return -1;

    wait_entry_t* e = wait_prepare(wq);
    if (!e) return -1;

    while (!e->woken) {
        sched_yield();
    }

    wait_finish(wq, e);
    return 0;
}

int wait_event_timeout(wait_queue_t* wq, uint64_t timeout_ticks) {
    if (!wq) return -1;

    wait_entry_t* e = wait_prepare(wq);
    if (!e) return -1;

    uint64_t deadline = pit_get_ticks() + timeout_ticks;
    int result = 0;

    while (!e->woken) {
        if (pit_get_ticks() >= deadline) {
            result = 1;
            break;
        }
        sched_yield();
    }

    wait_finish(wq, e);
    return result;
}

// The waker only flips flags; each sleeper unlinks its own entry so the pool
// slot is never recycled while its owner is still spinning on `woken`.
static int wake_up_nr(wait_queue_t* wq, int max) {
    if (!wq) return 0;

    int woken = 0;
    spin_lock(&wq->lock);

    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &wq->waiters) {
        if (max > 0 && woken >= max) break;

        wait_entry_t* e = list_entry(pos, wait_entry_t, node);
        if (e->woken) continue;

        e->woken = true;
        if (e->task) e->task->state = TASK_STATE_READY;
        woken++;
        wq->wakeups++;
    }

    spin_unlock(&wq->lock);
    return woken;
}

int wake_up(wait_queue_t* wq)     { return wake_up_nr(wq, 1); }
int wake_up_all(wait_queue_t* wq) { return wake_up_nr(wq, 0); }

uint32_t wait_queue_waiters(const wait_queue_t* wq) {
    return wq ? wq->nr_waiting : 0;
}

// ---------------------------------------------------------------------------
// Completions
// ---------------------------------------------------------------------------

void completion_init(completion_t* c, const char* name) {
    if (!c) return;
    wait_queue_init(&c->wq, name ? name : "completion");
    c->done = false;
}

void wait_for_completion(completion_t* c) {
    if (!c) return;
    while (!c->done) {
        if (wait_event_timeout(&c->wq, 100) < 0) {
            // Entry pool exhausted: degrade to a plain spin-and-yield.
            sched_yield();
        }
    }
}

int wait_for_completion_timeout(completion_t* c, uint64_t timeout_ticks) {
    if (!c) return -1;
    if (c->done) return 0;

    uint64_t deadline = pit_get_ticks() + timeout_ticks;
    while (!c->done) {
        if (pit_get_ticks() >= deadline) return 1;
        wait_event_timeout(&c->wq, 10);
    }
    return 0;
}

void complete(completion_t* c) {
    if (!c) return;
    c->done = true;
    wake_up(&c->wq);
}

void complete_all(completion_t* c) {
    if (!c) return;
    c->done = true;
    wake_up_all(&c->wq);
}

bool completion_done(const completion_t* c) {
    return c ? c->done : false;
}

void wait_subsystem_init(void) {
    memset(g_entry_used, 0, sizeof(g_entry_used));
    spinlock_init(&g_pool_lock);
    g_tracked_count = 0;
}

void wait_dump_stats(void) {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Wait Queue Subsystem ===\n" ANSI_RESET);

    int in_use = 0;
    for (int i = 0; i < WAIT_MAX_SLEEPERS; i++) {
        if (g_entry_used[i]) in_use++;
    }
    printk("  Sleeper pool : %d/%d entries in use\n", in_use, WAIT_MAX_SLEEPERS);
    printk("  Tracked queues: %d\n\n", g_tracked_count);

    printk(ANSI_YELLOW "  %-20s %10s %10s %10s\n" ANSI_RESET,
           "NAME", "WAITING", "SLEEPS", "WAKEUPS");
    for (int i = 0; i < g_tracked_count; i++) {
        wait_queue_t* wq = g_tracked[i];
        if (!wq) continue;
        printk("  %-20s %10u %10llu %10llu\n",
               wq->name, wq->nr_waiting,
               (unsigned long long)wq->sleeps,
               (unsigned long long)wq->wakeups);
    }
}
