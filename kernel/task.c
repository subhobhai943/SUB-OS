#include <kernel/task.h>
#include <kernel/sched.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <arch/arch.h>

static pid_t next_pid = 1;
static task_t* current_task = NULL;
static task_t kernel_idle_task;

static void task_wrapper(task_entry_fn_t entry, void* arg) {
    if (entry) {
        entry(arg);
    }
    task_exit(0);
}

task_t* task_create(const char* name, task_entry_fn_t entry, void* arg, int priority) {
    task_t* task = (task_t*)kzalloc(sizeof(task_t));
    if (!task) return NULL;

    task->pid = next_pid++;
    strncpy(task->name, name ? name : "task", sizeof(task->name) - 1);
    task->state = TASK_STATE_READY;
    task->priority = priority;
    task->time_slice = 10; // 10 ticks = 100ms default

    // Allocate 16 KB stack for thread
    task->stack_size = 16384;
    task->stack_base = (uint64_t)kmalloc(task->stack_size);
    if (!task->stack_base) {
        kfree(task);
        return NULL;
    }

    uint64_t* stack_top = (uint64_t*)(task->stack_base + task->stack_size);

    // Set up initial stack frame for AMD64
    *(--stack_top) = (uint64_t)arg;          // RDI (arg)
    *(--stack_top) = (uint64_t)entry;        // RSI (entry function)
    *(--stack_top) = (uint64_t)task_wrapper; // RIP

    task->rsp = (uint64_t)stack_top;

    INIT_LIST_HEAD(&task->list);
    sched_add_task(task);

    return task;
}

void task_exit(int exit_code) {
    (void)exit_code;
    if (current_task && current_task->pid != 0) {
        current_task->state = TASK_STATE_DEAD;
        sched_remove_task(current_task);
        sched_yield();
    }
    while (1) {
        arch_halt();
    }
}

task_t* task_current(void) {
    return current_task;
}

pid_t task_get_pid(void) {
    return current_task ? current_task->pid : 0;
}

void task_set_current(task_t* task) {
    current_task = task;
}

void task_init_idle(void) {
    memset(&kernel_idle_task, 0, sizeof(task_t));
    kernel_idle_task.pid = 0;
    strcpy(kernel_idle_task.name, "swapper/0");
    kernel_idle_task.state = TASK_STATE_RUNNING;
    INIT_LIST_HEAD(&kernel_idle_task.list);
    current_task = &kernel_idle_task;
}
