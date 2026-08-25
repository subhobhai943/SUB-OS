#include <kernel/sched.h>
#include <kernel/task.h>
#include <kernel/sync.h>
#include <kernel/printk.h>
#include <arch/arch.h>

/*
 * SUB-OS "Weave" scheduler
 * ========================
 * See include/kernel/sched.h for the design overview. In short: WEAVE_NLANES
 * priority lanes, always dispatch the lowest non-empty lane round-robin, and
 * let a task's lane float based on how it gives up the CPU -- volunteers are
 * woven up (toward lower latency), quantum-burners are woven down.
 *
 * The switch itself is real: sub_ctx_switch() saves and restores callee-saved
 * state on each thread's own kernel stack. weave_current always names the
 * thread physically executing; the invariant is maintained by having the
 * switcher publish the target as current *before* the register swap.
 */

extern void sub_ctx_switch(void** save_sp, void* load_sp);

/* Quantum (in ticks) granted when a task (re)enters each lane. Lower lanes are
 * for interactive work: short quanta, quick rotation. Higher lanes are for
 * CPU-bound work: longer quanta, fewer switches. */
static const uint32_t weave_quantum[WEAVE_NLANES] = { 2, 4, 8, 16 };

static struct list_head weave_lane[WEAVE_NLANES];
static task_t*  weave_current = NULL;
static spinlock_t weave_lock = SPINLOCK_INIT;
static bool     weave_preempt = false;   /* timer-driven preemption armed? */
static uint32_t weave_nready = 0;        /* runnable tasks parked in lanes  */
static uint64_t weave_switches = 0;      /* lifetime context switches       */

/* ---- lane primitives (call with weave_lock held) ------------------------ */

static void weave_enqueue(task_t* t) {
    if (t->weave_lane >= WEAVE_NLANES)
        t->weave_lane = WEAVE_NLANES - 1;
    list_add_tail(&t->list, &weave_lane[t->weave_lane]);
    weave_nready++;
}

static task_t* weave_pick(void) {
    for (int l = 0; l < WEAVE_NLANES; l++) {
        if (!list_empty(&weave_lane[l]))
            return list_entry(weave_lane[l].next, task_t, list);
    }
    return NULL;
}

/* Publish `next` as the running thread and swap register/stack context.
 * Returns (into the *previous* thread) only when it is later switched back in.
 * Must be called WITHOUT weave_lock held -- the switched-in thread will resume
 * wherever it last called into the switch and expects the lock free. */
static void weave_switch_to(task_t* next) {
    task_t* prev = weave_current;
    if (next == prev)
        return;

    next->state = TASK_STATE_RUNNING;
    next->sched_runs++;
    weave_switches++;
    weave_current = next;
    task_set_current(next);

    sub_ctx_switch(&prev->ctx_sp, next->ctx_sp);
    /* control resumes here as `prev` on some later switch-in */
}

/* ---- public API --------------------------------------------------------- */

void sched_init(void) {
    for (int l = 0; l < WEAVE_NLANES; l++)
        INIT_LIST_HEAD(&weave_lane[l]);
    weave_nready = 0;
    weave_switches = 0;
    weave_preempt = false;

    task_init_idle();                 /* sets current_task = swapper/0 */
    weave_current = task_current();
    weave_current->weave_lane = 0;
    weave_current->weave_credit = weave_quantum[0];
    weave_current->weave_on = 0;      /* idle isn't part of the rotation yet */
}

void sched_add_task(task_t* task) {
    if (!task) return;
    spin_lock(&weave_lock);
    task->weave_lane = 0;
    task->weave_credit = weave_quantum[0];
    task->weave_on = 1;
    task->state = TASK_STATE_READY;
    weave_enqueue(task);
    spin_unlock(&weave_lock);
}

/* A node is currently threaded into a lane iff its links are non-NULL and do
 * not point back at itself (list_del() NULLs them; INIT_LIST_HEAD() self-links
 * an idle node). Only such a node may be unlinked. */
static bool weave_linked(task_t* t) {
    return t->list.next != NULL && t->list.next != &t->list;
}

void sched_remove_task(task_t* task) {
    if (!task) return;
    spin_lock(&weave_lock);
    if (weave_linked(task)) {
        list_del(&task->list);
        if (weave_nready) weave_nready--;
    }
    task->weave_on = 0;
    spin_unlock(&weave_lock);
}

void sched_join(void) {
    if (weave_current) weave_current->weave_on = 1;
}

void sched_leave(void) {
    if (weave_current) weave_current->weave_on = 0;
}

void sched_set_preempt(bool armed) {
    weave_preempt = armed;
}

/*
 * Common rotation core. `voluntary` distinguishes a cooperative yield (the
 * caller is rewarded, woven up a lane) from an involuntary/preemptive switch
 * (the caller is woven down). Picks the next thread, requeues the caller if it
 * is still a participant, and performs the switch.
 */
static void weave_rotate(bool voluntary) {
    spin_lock(&weave_lock);

    task_t* cur = weave_current;
    task_t* next = weave_pick();
    if (!next) {
        spin_unlock(&weave_lock);
        return;                       /* nobody else runnable: keep going */
    }
    list_del(&next->list);
    if (weave_nready) weave_nready--;

    if (cur && cur->weave_on &&
        (cur->state == TASK_STATE_RUNNING || cur->state == TASK_STATE_READY)) {
        if (voluntary) {
            if (cur->weave_lane > 0) cur->weave_lane--;             /* weave up */
        } else {
            if (cur->weave_lane < WEAVE_NLANES - 1) cur->weave_lane++; /* down */
        }
        cur->weave_credit = weave_quantum[cur->weave_lane];
        cur->state = TASK_STATE_READY;
        weave_enqueue(cur);
    }

    next->weave_credit = weave_quantum[next->weave_lane];
    spin_unlock(&weave_lock);

    weave_switch_to(next);
}

void sched_yield(void) {
    weave_rotate(true);
}

void sched_schedule(void) {
    weave_rotate(false);
}

void sched_tick(void) {
    task_t* cur = weave_current;
    if (!cur || cur->pid == 0)
        return;                       /* idle thread isn't charged */
    if (cur->weave_credit > 0)
        cur->weave_credit--;
    if (cur->weave_credit == 0 && weave_preempt)
        sched_schedule();             /* quantum spent -> preempt */
}

/* Retire the running thread: switch to the next runnable one and never come
 * back. If the run rotation is empty we simply return so the caller can halt. */
void sched_exit_current(void) {
    spin_lock(&weave_lock);
    task_t* next = weave_pick();
    if (next) {
        list_del(&next->list);
        if (weave_nready) weave_nready--;
    }
    spin_unlock(&weave_lock);

    if (!next)
        return;

    void* graveyard;                  /* dead context's SP goes nowhere */
    next->state = TASK_STATE_RUNNING;
    next->sched_runs++;
    next->weave_credit = weave_quantum[next->weave_lane];
    weave_switches++;
    weave_current = next;
    task_set_current(next);
    sub_ctx_switch(&graveyard, next->ctx_sp);
    /* unreachable */
}

/* ---- diagnostics -------------------------------------------------------- */

void sched_dump(void) {
    spin_lock(&weave_lock);
    printk(ANSI_BRIGHT_CYAN "=== Weave Scheduler ===\n" ANSI_RESET);
    printk("  running: %s (pid %d, lane %u)  switches: %u  ready: %u\n",
           weave_current ? weave_current->name : "?",
           weave_current ? (int)weave_current->pid : -1,
           weave_current ? (unsigned)weave_current->weave_lane : 0,
           (unsigned)weave_switches, (unsigned)weave_nready);
    for (int l = 0; l < WEAVE_NLANES; l++) {
        printk("  lane %d:", l);
        task_t* pos;
        int n = 0;
        list_for_each_entry(pos, &weave_lane[l], list) {
            printk(" %s", pos->name);
            n++;
        }
        if (!n) printk(" (empty)");
        printk("\n");
    }
    spin_unlock(&weave_lock);
}

/* ---- boot-time context-switch self-test --------------------------------- */

static volatile int weave_test_exits = 0;

static void weave_worker(void* arg) {
    long id = (long)(uintptr_t)arg;
    for (int i = 0; i < 4; i++) {
        task_t* self = task_current();
        printk("  [weave] worker %d slice %d (lane %u, runs %u)\n",
               (int)id, i, (unsigned)self->weave_lane, (unsigned)self->sched_runs);
        sched_yield();                /* cooperatively hand off the CPU */
    }
    printk("  [weave] worker %d done\n", (int)id);
    weave_test_exits++;
    /* fall off the end -> trampoline -> task_exit -> switch away, never return */
}

void weave_selftest(void) {
    printk(ANSI_BRIGHT_CYAN
           "WEAVE: cooperative context-switch self-test\n" ANSI_RESET);

    uint64_t before = weave_switches;
    weave_test_exits = 0;

    task_create("weave-a", weave_worker, (void*)(uintptr_t)1, 0);
    task_create("weave-b", weave_worker, (void*)(uintptr_t)2, 0);
    task_create("weave-c", weave_worker, (void*)(uintptr_t)3, 0);

    /* Join the rotation so exiting workers can switch back to us, then drive
     * the whole batch to completion by repeatedly yielding. */
    sched_join();
    while (weave_test_exits < 3)
        sched_yield();
    sched_leave();

    printk(ANSI_BRIGHT_GREEN
           "WEAVE: self-test OK -- %u real context switches, 3/3 workers exited\n"
           ANSI_RESET, (unsigned)(weave_switches - before));
}
