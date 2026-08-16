#ifndef _KERNEL_SYNC_H
#define _KERNEL_SYNC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT { .lock = 0 }

static inline void spinlock_init(spinlock_t* lock) {
    lock->lock = 0;
}

static inline void spin_lock(spinlock_t* lock) {
    while (__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(spinlock_t* lock) {
    __atomic_clear(&lock->lock, __ATOMIC_RELEASE);
}

typedef struct {
    spinlock_t lock;
    volatile int count;
} semaphore_t;

void sem_init(semaphore_t* sem, int value);
void sem_wait(semaphore_t* sem);
void sem_post(semaphore_t* sem);

#endif // _KERNEL_SYNC_H
