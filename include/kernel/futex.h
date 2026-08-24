#ifndef _KERNEL_FUTEX_H
#define _KERNEL_FUTEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/wait.h>

// Fast Userspace muTEX. A futex is an ordinary 32-bit word that stays entirely
// in user memory while uncontended; the kernel is only consulted when a thread
// actually has to block, at which point the word's address is hashed onto one
// of FUTEX_BUCKETS wait queues.

#define FUTEX_BUCKETS 32

typedef enum {
    FUTEX_OP_WAIT = 0,
    FUTEX_OP_WAKE = 1,
    FUTEX_OP_REQUEUE = 2,
    FUTEX_OP_WAKE_ALL = 3
} futex_op_t;

typedef struct {
    uint64_t waits;
    uint64_t wakes;
    uint64_t requeues;
    uint64_t spurious;
    uint64_t timeouts;
    uint32_t active_waiters;
} futex_stats_t;

void futex_init(void);

// Sleep on `uaddr` only while *uaddr still equals `expected`. Returns 0 when
// woken, 1 on timeout, -1 if the value changed before parking (the caller
// should retry its fast path).
int futex_wait(volatile uint32_t* uaddr, uint32_t expected, uint64_t timeout_ticks);

// Wake up to `count` waiters parked on `uaddr`. Returns how many were woken.
int futex_wake(volatile uint32_t* uaddr, int count);

// Move waiters from `uaddr1` onto `uaddr2` without waking them (used to avoid
// the thundering herd when a condvar hands off to a mutex).
int futex_requeue(volatile uint32_t* uaddr1, volatile uint32_t* uaddr2, int count);

futex_stats_t futex_get_stats(void);
void          futex_dump(void);

// ---------------------------------------------------------------------------
// A futex-backed mutex, for kernel code that wants sleeping mutual exclusion.
// ---------------------------------------------------------------------------
typedef struct {
    volatile uint32_t state; // 0 = unlocked, 1 = locked, 2 = locked + waiters
    char              name[24];
} fmutex_t;

void fmutex_init(fmutex_t* m, const char* name);
void fmutex_lock(fmutex_t* m);
bool fmutex_trylock(fmutex_t* m);
void fmutex_unlock(fmutex_t* m);

#endif // _KERNEL_FUTEX_H
