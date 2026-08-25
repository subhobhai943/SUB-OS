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
static uint64_t weave_preemptions = 0;   /* switches forced by the timer    */

/*
 * Deferred-preemption state.
 *
 * The timer ISR must not switch mid-handler: it would strand the PIC EOI and
 * could fire while a thread holds a non-IRQ-safe lock. Instead sched_tick()
 * only raises weave_need_resched, and the actual switch happens at IRQ return
 * (sched_preempt_on_return), after the EOI, on the interrupted thread's own
 * kernel stack.
 *
 * weave_preempt_depth is a preemption-disable count: code that holds a plain
 * (non-IRQ-safe) spinlock raises it so a preemptive switch cannot run while
 * that lock is held. printk uses this; see kernel/printk.c.
 */
static volatile uint32_t weave_need_resched = 0;
static volatile int32_t  weave_preempt_depth = 0;

/* weave_lock, held with interrupts masked so the timer ISR can never fire
 * while the run lanes are being mutated -- that is what makes a plain spinlock
 * safe to share between thread context and the tick. */
static inline unsigned long weave_lock_irq(void) {
    unsigned long flags = arch_irq_save();
    spin_lock(&weave_lock);
    return flags;
}

static inline void weave_unlock_irq(unsigned long flags) {
    spin_unlock(&weave_lock);
    arch_irq_restore(flags);
}

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
    unsigned long flags = weave_lock_irq();
    task->weave_lane = 0;
    task->weave_credit = weave_quantum[0];
    task->weave_on = 1;
    task->state = TASK_STATE_READY;
    weave_enqueue(task);
    weave_unlock_irq(flags);
}

/* A node is currently threaded into a lane iff its links are non-NULL and do
 * not point back at itself (list_del() NULLs them; INIT_LIST_HEAD() self-links
 * an idle node). Only such a node may be unlinked. */
static bool weave_linked(task_t* t) {
    return t->list.next != NULL && t->list.next != &t->list;
}

void sched_remove_task(task_t* task) {
    if (!task) return;
    unsigned long flags = weave_lock_irq();
    if (weave_linked(task)) {
        list_del(&task->list);
        if (weave_nready) weave_nready--;
    }
    task->weave_on = 0;
    weave_unlock_irq(flags);
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

/* Raise/lower the preemption-disable count. While non-zero, the tick will not
 * force a switch -- callers holding a plain spinlock use this so a preemptive
 * switch cannot run with that lock held. Nesting is allowed. */
void sched_preempt_disable(void) {
    __atomic_add_fetch(&weave_preempt_depth, 1, __ATOMIC_SEQ_CST);
}

void sched_preempt_enable(void) {
    __atomic_sub_fetch(&weave_preempt_depth, 1, __ATOMIC_SEQ_CST);
}

/*
 * Common rotation core. `voluntary` distinguishes a cooperative yield (the
 * caller is rewarded, woven up a lane) from an involuntary/preemptive switch
 * (the caller is woven down). Picks the next thread, requeues the caller if it
 * is still a participant, and performs the switch.
 */
static void weave_rotate(bool voluntary) {
    /* Interrupts stay masked from here through the register swap: the pick, the
     * requeue and the switch must be one indivisible step against the tick.
     * The saved flags live on this thread's stack, so each thread restores its
     * own interrupt state when it is eventually switched back in. */
    unsigned long flags = arch_irq_save();
    spin_lock(&weave_lock);

    task_t* cur = weave_current;
    task_t* next = weave_pick();
    if (!next) {
        spin_unlock(&weave_lock);
        arch_irq_restore(flags);
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
    arch_irq_restore(flags);
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
    /*
     * Do NOT switch here: this runs inside the timer ISR, before the PIC EOI,
     * and possibly while a plain lock is held. Just record that the quantum is
     * spent; sched_preempt_on_return() performs the switch as the IRQ returns.
     */
    if (cur->weave_credit == 0 && weave_preempt &&
        __atomic_load_n(&weave_preempt_depth, __ATOMIC_SEQ_CST) == 0) {
        weave_need_resched = 1;
    }
}

/*
 * Preemption tail, called from the IRQ dispatcher after the EOI. If the tick
 * asked for a reschedule and it is safe (preemption armed, not disabled, not
 * already reentered), switch away from the interrupted thread. We are on that
 * thread's kernel stack with its full trap frame below us, so when it is later
 * switched back in it resumes here and returns through the normal IRQ epilogue.
 */
void sched_preempt_on_return(void) {
    if (!weave_preempt || !weave_need_resched)
        return;
    if (__atomic_load_n(&weave_preempt_depth, __ATOMIC_SEQ_CST) != 0)
        return;
    task_t* cur = weave_current;
    if (!cur || cur->pid == 0)
        return;                       /* never preempt the idle/boot thread */

    /*
     * Clear the request, then switch. sched_schedule() runs with interrupts
     * masked and does not return to us until this same thread is scheduled
     * back in -- at which point we count the preemption and unwind through the
     * IRQ epilogue. No re-entrancy guard is needed: the masked switch means no
     * nested timer can land on this stack before the swap completes.
     */
    weave_need_resched = 0;
    uint64_t before = weave_switches;
    sched_schedule();                 /* involuntary: caller woven down */
    if (weave_switches != before)
        weave_preemptions++;          /* a switch really happened */
}

/* Retire the running thread: switch to the next runnable one and never come
 * back. If the run rotation is empty we simply return so the caller can halt. */
void sched_exit_current(void) {
    unsigned long flags = arch_irq_save();
    spin_lock(&weave_lock);
    task_t* next = weave_pick();
    if (next) {
        list_del(&next->list);
        if (weave_nready) weave_nready--;
    }
    spin_unlock(&weave_lock);

    if (!next) {
        arch_irq_restore(flags);
        return;
    }

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
    unsigned long flags = weave_lock_irq();
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
    weave_unlock_irq(flags);
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

/* ---- boot-time PREEMPTION self-test ------------------------------------- */
/*
 * Proves timer-driven preemption: two worker threads that NEVER yield are made
 * runnable, preemption is armed, and the boot thread watches the tick force
 * context switches between them. Because the workers never cooperate, every
 * switch counted here is involuntary. The workers only touch their own
 * counters -- no printk, no locks -- so a preemptive switch mid-loop is always
 * safe. Each also self-caps its iteration count, so even if preemption failed
 * to fire the boot could not hang.
 */
static volatile uint8_t  weave_pt_stop = 0;
static volatile uint64_t weave_pt_work[2];
static volatile uint32_t weave_pt_lane[2];
static volatile uint8_t  weave_pt_live = 0;

static void weave_burn(void* arg) {
    long id = (long)(uintptr_t)arg;
    /* Preemptible only with interrupts enabled; a freshly forged frame starts
     * with them masked, so opt in explicitly. */
    arch_enable_interrupts();
    __atomic_add_fetch(&weave_pt_live, 1, __ATOMIC_SEQ_CST);

    /* Tight non-yielding loop. It runs until the observer sets weave_pt_stop;
     * the huge self-cap is only a safety net so a failure to preempt could not
     * hang the boot instead of just ending the test. */
    for (uint64_t i = 0; i < 60000000ULL && !weave_pt_stop; i++) {
        weave_pt_work[id] = i;
        weave_pt_lane[id] = task_current()->weave_lane;
    }
    __atomic_sub_fetch(&weave_pt_live, 1, __ATOMIC_SEQ_CST);
    /* fall off -> trampoline -> task_exit */
}

void weave_preempt_selftest(void) {
#if defined(__x86_64__)
    printk(ANSI_BRIGHT_CYAN
           "WEAVE: preemptive-scheduling self-test\n" ANSI_RESET);

    weave_pt_stop = 0;
    weave_pt_work[0] = weave_pt_work[1] = 0;
    weave_pt_lane[0] = weave_pt_lane[1] = 0;
    weave_pt_live = 0;

    uint64_t preempt_before = weave_preemptions;

    task_create("burn-0", weave_burn, (void*)(uintptr_t)0, 0);
    task_create("burn-1", weave_burn, (void*)(uintptr_t)1, 0);

    /* Arm timer-driven preemption and join the rotation so the boot thread can
     * be scheduled back in to observe progress. */
    sched_set_preempt(true);
    sched_join();

    /*
     * Observe until the timer has forced enough switches to be convincing, or a
     * generous tick ceiling elapses. weave_preemptions counts ONLY involuntary
     * (timer-forced) switches, so it is unaffected by the boot thread's own
     * cooperative yields here. The tight tick ceiling bounds the test either
     * way, so it can never hang the boot.
     */
    uint64_t start_tick = arch_get_ticks();
    uint64_t spins = 0;
    while (weave_preemptions - preempt_before < 20 &&
           arch_get_ticks() - start_tick < 500 &&
           spins < 20000000ULL) {
        sched_yield();
        spins++;
    }
    uint64_t end_tick = arch_get_ticks();

    uint64_t forced = weave_preemptions - preempt_before;
    uint32_t l0 = weave_pt_lane[0], l1 = weave_pt_lane[1];
    printk("  observer: %u forced switches, ticks %u->%u, %u boot yields\n",
           (unsigned)forced, (unsigned)start_tick, (unsigned)end_tick,
           (unsigned)spins);

    /* Stop the workers and drain them out of the rotation. */
    weave_pt_stop = 1;
    while (__atomic_load_n(&weave_pt_live, __ATOMIC_SEQ_CST) > 0)
        sched_yield();
    sched_leave();

    printk("  burn-0 ran %u iters (last lane %u), burn-1 ran %u iters (last lane %u)\n",
           (unsigned)weave_pt_work[0], (unsigned)l0,
           (unsigned)weave_pt_work[1], (unsigned)l1);

    if (forced > 0 && weave_pt_work[0] > 0 && weave_pt_work[1] > 0) {
        printk(ANSI_BRIGHT_GREEN
               "WEAVE: preemption OK -- the timer forced %u involuntary switches; "
               "both non-yielding workers made progress and were woven down to "
               "the CPU-bound lanes\n" ANSI_RESET, (unsigned)forced);
    } else {
        printk(ANSI_YELLOW
               "WEAVE: preemption self-test inconclusive (%u forced switches)\n"
               ANSI_RESET, (unsigned)forced);
    }

    /* Leave preemption armed: from here on CPU-bound kernel threads are time-
     * sliced. The interactive shell runs on the idle/boot thread (pid 0), which
     * sched_tick never charges, so it keeps the CPU when nothing else is ready. */
#endif
}
