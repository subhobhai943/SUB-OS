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

/* Per-arch context switch primitives (see arch/<arch>/cpu/switch.*). */
extern void sub_ctx_switch(void** save_sp, void* load_sp);
extern void sub_thread_trampoline(void);

/* Retire the running thread and switch to the next runnable one (sched.c). */
extern void sched_exit_current(void);

/*
 * Forge the initial saved-context frame for a brand-new thread so that the
 * first switch into it lands on sub_thread_trampoline with the entry function
 * and its argument sitting in the callee-saved registers the trampoline reads.
 * The exact word layout mirrors the pop/ldp/pop sequence in each arch's
 * sub_ctx_switch, so the two must be kept in lockstep.
 */
static void* task_forge_frame(uint64_t stack_base, size_t stack_size,
                              task_entry_fn_t entry, void* arg) {
#if defined(__x86_64__)
    uint64_t top = (stack_base + stack_size) & ~0xFULL;   /* 16-byte aligned */
    uint64_t* sp = (uint64_t*)top;
    *(--sp) = (uint64_t)&sub_thread_trampoline;  /* ret target             */
    *(--sp) = 0;                                 /* rbp                    */
    *(--sp) = 0;                                 /* rbx                    */
    *(--sp) = (uint64_t)entry;                   /* r12 -> entry           */
    *(--sp) = (uint64_t)arg;                     /* r13 -> arg             */
    *(--sp) = 0;                                 /* r14                    */
    *(--sp) = 0;                                 /* r15                    */
    *(--sp) = 0x2;                               /* rflags, IF clear       */
    return (void*)sp;
#elif defined(__aarch64__) || defined(__arm64__)
    uint64_t top = (stack_base + stack_size) & ~0xFULL;
    uint64_t* sp = (uint64_t*)top;
    sp -= 12;                                    /* x19..x30 */
    sp[0]  = (uint64_t)entry;                    /* x19 -> entry           */
    sp[1]  = (uint64_t)arg;                      /* x20 -> arg             */
    for (int i = 2; i <= 10; i++) sp[i] = 0;     /* x21..x28, x29(fp)      */
    sp[11] = (uint64_t)&sub_thread_trampoline;   /* x30(lr) -> trampoline  */
    return (void*)sp;
#elif defined(__arm__)
    uint32_t top = (uint32_t)((stack_base + stack_size) & ~0x7ULL);
    uint32_t* sp = (uint32_t*)top;
    sp -= 9;                                     /* r4..r11, lr */
    sp[0] = (uint32_t)(uintptr_t)entry;          /* r4 -> entry            */
    sp[1] = (uint32_t)(uintptr_t)arg;            /* r5 -> arg              */
    for (int i = 2; i <= 7; i++) sp[i] = 0;      /* r6..r11                */
    sp[8] = (uint32_t)(uintptr_t)&sub_thread_trampoline; /* lr             */
    return (void*)sp;
#else
    (void)stack_base; (void)stack_size; (void)entry; (void)arg;
    return NULL;
#endif
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

    // Forge the initial saved-register frame so the first context switch into
    // this thread lands on the trampoline with entry/arg in place.
    task->ctx_sp = task_forge_frame(task->stack_base, task->stack_size, entry, arg);
    task->rsp = (uint64_t)(uintptr_t)task->ctx_sp;

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
        current_task->weave_on = 0;
        sched_exit_current();  // switches to the next runnable thread; only
                               // falls through here if nothing else is runnable
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
