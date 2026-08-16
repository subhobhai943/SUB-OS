#include <kernel/sync.h>
#include <kernel/sched.h>

void sem_init(semaphore_t* sem, int value) {
    spinlock_init(&sem->lock);
    sem->count = value;
}

void sem_wait(semaphore_t* sem) {
    while (1) {
        spin_lock(&sem->lock);
        if (sem->count > 0) {
            sem->count--;
            spin_unlock(&sem->lock);
            return;
        }
        spin_unlock(&sem->lock);
        sched_yield();
    }
}

void sem_post(semaphore_t* sem) {
    spin_lock(&sem->lock);
    sem->count++;
    spin_unlock(&sem->lock);
}
