#ifndef _KERNEL_WAIT_H
#define _KERNEL_WAIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/list.h>
#include <kernel/sync.h>
#include <kernel/task.h>

// Wait queues and completions.
//
// SUB-OS runs a cooperative round-robin scheduler, so a sleeper parks itself
// on the queue, marks its task BLOCKED and yields until a waker flips the
// entry's `woken` flag. Timed waits fall back to the PIT tick counter.

typedef struct wait_entry {
    task_t*          task;
    volatile bool    woken;
    struct list_head node;
} wait_entry_t;

typedef struct wait_queue {
    char             name[24];
    struct list_head waiters;
    spinlock_t       lock;
    uint64_t         wakeups;
    uint64_t         sleeps;
    uint32_t         nr_waiting;
} wait_queue_t;

void wait_queue_init(wait_queue_t* wq, const char* name);

// Park the caller until woken. Returns 0 on wake, -1 if the queue is full.
int  wait_event(wait_queue_t* wq);

// Park the caller for at most `timeout_ticks` PIT ticks (100 Hz).
// Returns 0 if woken, 1 if the wait timed out.
int  wait_event_timeout(wait_queue_t* wq, uint64_t timeout_ticks);

// Wake the longest-waiting sleeper, or every sleeper. Returns the count woken.
int  wake_up(wait_queue_t* wq);
int  wake_up_all(wait_queue_t* wq);

uint32_t wait_queue_waiters(const wait_queue_t* wq);

// ---------------------------------------------------------------------------
// Completions: one-shot "this is done" barriers built on a wait queue.
// ---------------------------------------------------------------------------
typedef struct completion {
    wait_queue_t  wq;
    volatile bool done;
} completion_t;

void completion_init(completion_t* c, const char* name);
void wait_for_completion(completion_t* c);
int  wait_for_completion_timeout(completion_t* c, uint64_t timeout_ticks);
void complete(completion_t* c);
void complete_all(completion_t* c);
bool completion_done(const completion_t* c);

void wait_subsystem_init(void);
void wait_dump_stats(void);

#endif // _KERNEL_WAIT_H
