#ifndef _KERNEL_TASK_H
#define _KERNEL_TASK_H

#include "types.h"
#include <lib/list.h>

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SLEEPING,
    TASK_STATE_ZOMBIE,
    TASK_STATE_DEAD
} task_state_t;

typedef struct task_struct {
    pid_t pid;
    char name[32];
    task_state_t state;
    uint64_t rsp;
    uint64_t stack_base;
    size_t stack_size;
    uint64_t sleep_until_ticks;
    int priority;
    int time_slice;
    struct list_head list;
} task_t;

typedef void (*task_entry_fn_t)(void* arg);

task_t* task_create(const char* name, task_entry_fn_t entry, void* arg, int priority);
void task_exit(int exit_code);
task_t* task_current(void);
pid_t task_get_pid(void);

#endif // _KERNEL_TASK_H
