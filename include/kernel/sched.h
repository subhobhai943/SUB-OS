#ifndef _KERNEL_SCHED_H
#define _KERNEL_SCHED_H

#include "task.h"

void sched_init(void);
void sched_yield(void);
void sched_schedule(void);
void sched_tick(void);
void sched_add_task(task_t* task);
void sched_remove_task(task_t* task);

#endif // _KERNEL_SCHED_H
