#ifndef _KERNEL_RCU_H
#define _KERNEL_RCU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Tiny RCU (Read-Copy-Update).
//
// Readers are free: rcu_read_lock() only bumps a per-CPU nesting depth, so a
// lookup never contends with an update. Writers publish a new version with
// rcu_assign_pointer() and defer reclamation of the old one until every reader
// that could still hold a reference has finished — a "grace period".

#define RCU_MAX_CALLBACKS 64

typedef void (*rcu_callback_t)(void* arg);

typedef struct {
    uint64_t grace_periods;
    uint64_t callbacks_queued;
    uint64_t callbacks_invoked;
    uint64_t read_locks;
    uint32_t nesting_depth;
    uint32_t pending;
} rcu_stats_t;

void rcu_init(void);

void rcu_read_lock(void);
void rcu_read_unlock(void);
bool rcu_read_lock_held(void);

// Block until every reader active at call time has left its critical section.
void rcu_synchronize(void);

// Queue `func(arg)` to run once the current grace period elapses.
int  call_rcu(rcu_callback_t func, void* arg);

// Free `ptr` after a grace period (the common case for call_rcu).
int  kfree_rcu(void* ptr);

// Advance the state machine; called from the timer tick and from rcu_synchronize.
void rcu_process_callbacks(void);

uint64_t   rcu_get_grace_period(void);
rcu_stats_t rcu_get_stats(void);
void       rcu_dump(void);

// Publish/consume helpers. The barriers keep the compiler from reordering the
// initialisation of an object past the store that makes it visible.
#define rcu_assign_pointer(p, v) \
    do { __atomic_store_n(&(p), (v), __ATOMIC_RELEASE); } while (0)

#define rcu_dereference(p) \
    __atomic_load_n(&(p), __ATOMIC_ACQUIRE)

#endif // _KERNEL_RCU_H
