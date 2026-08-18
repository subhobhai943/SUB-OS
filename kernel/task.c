#include <kernel/task.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <arch/arch.h>

static pid_t next_pid = 1;
static task_t* current_task = NULL;
static task_t kernel_idle_task;
static LIST_HEAD(global_tasks);

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
    task->ppid = current_task ? current_task->pid : 0;
    task->pgid = task->pid;
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
    INIT_LIST_HEAD(&task->all_list);
    list_add_tail(&task->all_list, &global_tasks);

    sched_add_task(task);

    return task;
}

void task_exit(int exit_code) {
    if (current_task && current_task->pid != 0) {
        current_task->exit_code = exit_code;
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

task_t* task_find_by_pid(pid_t pid) {
    if (pid == 0) return &kernel_idle_task;
    task_t* pos;
    list_for_each_entry(pos, &global_tasks, all_list) {
        if (pos->pid == pid) {
            return pos;
        }
    }
    return NULL;
}

int task_kill(pid_t pid, int sig) {
    if (pid == 0) {
        printk(ANSI_RED "Error: cannot signal swapper/0 (PID 0)\n" ANSI_RESET);
        return -1;
    }
    task_t* t = task_find_by_pid(pid);
    if (!t) {
        printk(ANSI_RED "kill: (%d) - No such process\n" ANSI_RESET, pid);
        return -1;
    }

    printk(ANSI_YELLOW "Signal %d sent to process '%s' (PID %d)\n" ANSI_RESET, sig, t->name, pid);
    if (sig == 9 || sig == 15) { // SIGKILL / SIGTERM
        t->state = TASK_STATE_DEAD;
        sched_remove_task(t);
    }
    return 0;
}

static void print_process_tree_node(task_t* parent, int depth) {
    for (int i = 0; i < depth; i++) {
        printk("  │ ");
    }
    const char* state_col = (parent->state == TASK_STATE_RUNNING) ? ANSI_BRIGHT_GREEN :
                            (parent->state == TASK_STATE_READY) ? ANSI_BRIGHT_CYAN : ANSI_YELLOW;

    printk("  ├─ " ANSI_BOLD "%s" ANSI_RESET " (pid: " ANSI_BRIGHT_YELLOW "%d" ANSI_RESET ", %s%s" ANSI_RESET ")\n",
           parent->name, parent->pid, state_col,
           parent->state == TASK_STATE_RUNNING ? "RUNNING" :
           parent->state == TASK_STATE_READY ? "READY" :
           parent->state == TASK_STATE_SLEEPING ? "SLEEP" : "BLOCKED");

    task_t* pos;
    list_for_each_entry(pos, &global_tasks, all_list) {
        if (pos->ppid == parent->pid && pos != parent && pos->state != TASK_STATE_DEAD) {
            print_process_tree_node(pos, depth + 1);
        }
    }
}

void task_dump_pstree(void) {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Process Tree Hierarchy (pstree) ===\n" ANSI_RESET);
    printk(ANSI_BOLD "systemd/init (PID 1)\n" ANSI_RESET);
    task_t* pos;
    list_for_each_entry(pos, &global_tasks, all_list) {
        if (pos->ppid <= 1 && pos->state != TASK_STATE_DEAD) {
            print_process_tree_node(pos, 0);
        }
    }
}

void task_init_idle(void) {
    memset(&kernel_idle_task, 0, sizeof(task_t));
    kernel_idle_task.pid = 0;
    kernel_idle_task.ppid = 0;
    kernel_idle_task.pgid = 0;
    strcpy(kernel_idle_task.name, "swapper/0");
    kernel_idle_task.state = TASK_STATE_RUNNING;
    INIT_LIST_HEAD(&kernel_idle_task.list);
    INIT_LIST_HEAD(&kernel_idle_task.all_list);
    INIT_LIST_HEAD(&global_tasks);
    current_task = &kernel_idle_task;
}
