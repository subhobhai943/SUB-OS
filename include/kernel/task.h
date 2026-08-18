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
    pid_t ppid;
    pid_t pgid;
    char name[32];
    task_state_t state;
    uint64_t rsp;
    uint64_t stack_base;
    size_t stack_size;
    uint64_t sleep_until_ticks;
    int priority;
    int time_slice;
    int exit_code;
    struct list_head list;
    struct list_head all_list;
} task_t;

typedef void (*task_entry_fn_t)(void* arg);

void task_init_idle(void);
task_t* task_create(const char* name, task_entry_fn_t entry, void* arg, int priority);
void task_exit(int exit_code);
task_t* task_current(void);
pid_t task_get_pid(void);
void task_set_current(task_t* task);
task_t* task_find_by_pid(pid_t pid);
int task_kill(pid_t pid, int sig);
void task_dump_pstree(void);

#endif // _KERNEL_TASK_H
