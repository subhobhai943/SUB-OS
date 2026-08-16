#include <kernel/sched.h>
#include <kernel/sync.h>
#include <arch/x86_64/pit.h>

static struct list_head ready_queue;
static spinlock_t sched_lock = SPINLOCK_INIT;

extern void task_init_idle(void);

void sched_init(void) {
    INIT_LIST_HEAD(&ready_queue);
    task_init_idle();
}

void sched_add_task(task_t* task) {
    if (!task) return;
    spin_lock(&sched_lock);
    list_add_tail(&task->list, &ready_queue);
    spin_unlock(&sched_lock);
}

void sched_remove_task(task_t* task) {
    if (!task) return;
    spin_lock(&sched_lock);
    list_del(&task->list);
    spin_unlock(&sched_lock);
}

void sched_yield(void) {
    // Cooperative yield
    sched_schedule();
}

void sched_tick(void) {
    // Timer tick scheduler hook
    task_t* curr = task_current();
    if (curr && curr->pid != 0) {
        if (--curr->time_slice <= 0) {
            curr->time_slice = 10;
            sched_schedule();
        }
    }
}

void sched_schedule(void) {
    spin_lock(&sched_lock);

    if (list_empty(&ready_queue)) {
        spin_unlock(&sched_lock);
        return;
    }

    task_t* next = list_entry(ready_queue.next, task_t, list);
    list_del(&next->list);
    list_add_tail(&next->list, &ready_queue); // Move to back of round-robin queue

    task_t* curr = task_current();
    if (curr != next) {
        // Context switch logic
        next->state = TASK_STATE_RUNNING;
        if (curr->state == TASK_STATE_RUNNING) {
            curr->state = TASK_STATE_READY;
        }
    }

    spin_unlock(&sched_lock);
}
