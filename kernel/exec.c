#include <kernel/exec.h>
#include <kernel/elf.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <kernel/task.h>
#include <mm/vmspace.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

#if defined(__x86_64__)
#include <arch/x86_64/gdt.h>

/* Implemented in arch/x86_64/cpu/usermode.asm. */
extern int64_t user_enter(uint64_t entry, uint64_t user_rsp);
extern void    user_return(int64_t status) __attribute__((noreturn));

#define USER_KSTACK_SIZE 16384

static uproc_t* current_uproc = NULL;

/*
 * User processes carry their own pid namespace for now. They are not backed by
 * a task_struct yet, so borrowing task_get_pid() would report the idle task's
 * pid 0 to every program that asked.
 */
static pid_t next_user_pid = 1;

uproc_t* uproc_current(void) {
    return current_uproc;
}

bool uaccess_ok(uint64_t uaddr, size_t len) {
    if (!current_uproc) return false;
    if (len == 0) return true;
    return vmspace_range_ok(current_uproc->vm, uaddr, len);
}

/*
 * While a process runs, CR3 is its own, and its pages are mapped and present.
 * The kernel can therefore dereference a user pointer directly -- but only
 * after uaccess_ok() has confirmed the whole range really belongs to the
 * process, otherwise a hostile pointer into the identity-mapped low 4 GB
 * would read or scribble on the kernel itself.
 */
int uaccess_copy_from_user(void* dst, uint64_t usrc, size_t len) {
    if (!uaccess_ok(usrc, len)) return -1;
    memcpy(dst, (const void*)(uintptr_t)usrc, len);
    return 0;
}

int uaccess_copy_to_user(uint64_t udst, const void* src, size_t len) {
    if (!uaccess_ok(udst, len)) return -1;
    memcpy((void*)(uintptr_t)udst, src, len);
    return 0;
}

long uaccess_copy_string(char* dst, uint64_t usrc, size_t max) {
    if (!dst || max == 0) return -1;
    for (size_t i = 0; i < max; i++) {
        if (!uaccess_ok(usrc + i, 1)) return -1;
        char c = *(const char*)(uintptr_t)(usrc + i);
        dst[i] = c;
        if (c == '\0') return (long)i;
    }
    dst[max - 1] = '\0';
    return -1;   /* not terminated within max */
}

void uproc_exit(int status) {
    if (current_uproc) {
        current_uproc->exit_code = status;
        current_uproc->exited = true;
    }
    user_return((int64_t)status);
    __builtin_unreachable();
}

void uproc_fault(const char* reason, uint64_t addr) {
    printk(KERN_ERR "exec: killing '%s' (pid %d): %s at 0x%llx\n",
           current_uproc ? current_uproc->name : "?",
           current_uproc ? current_uproc->pid : -1,
           reason ? reason : "fault",
           (unsigned long long)addr);
    uproc_exit(-1);
}

/*
 * Build the System V entry stack: rsp points at argc, followed by the argv
 * pointers, a NULL, and an empty environment. The program name string itself
 * is copied to the very top of the stack.
 */
static int setup_user_stack(uproc_t* proc, uint64_t* rsp_out) {
    uint64_t stack_bytes = USER_STACK_PAGES * VM_PAGE_SIZE;
    uint64_t stack_low = USER_STACK_TOP - stack_bytes;

    if (vmspace_alloc(proc->vm, stack_low, stack_bytes,
                      VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER) != 0) {
        return -1;
    }

    size_t name_len = strlen(proc->name) + 1;
    uint64_t argv0 = (USER_STACK_TOP - name_len) & ~0xFULL;
    if (vmspace_write(proc->vm, argv0, proc->name, name_len) != 0) return -1;

    /* argc, argv[0], argv[1]=NULL, envp[0]=NULL */
    uint64_t frame[4] = { 1, argv0, 0, 0 };
    uint64_t rsp = (argv0 - sizeof(frame)) & ~0xFULL;
    if (vmspace_write(proc->vm, rsp, frame, sizeof(frame)) != 0) return -1;

    *rsp_out = rsp;
    return 0;
}

int exec_user_image(const char* name, const void* image, size_t len) {
    if (!image || len == 0) return -1;
    if (current_uproc) {
        printk(KERN_ERR "exec: nested exec is not supported yet\n");
        return -1;
    }

    uproc_t* proc = (uproc_t*)kzalloc(sizeof(uproc_t));
    if (!proc) return -1;

    strncpy(proc->name, name ? name : "user", sizeof(proc->name) - 1);
    proc->pid = next_user_pid++;

    proc->vm = vmspace_create();
    if (!proc->vm) {
        printk(KERN_ERR "exec: out of memory building an address space\n");
        kfree(proc);
        return -1;
    }

    int err = elf_load(proc->vm, image, len, &proc->entry, &proc->brk_start);
    if (err != ELF_OK) {
        printk(KERN_ERR "exec: %s: %s\n", proc->name, elf_strerror(err));
        vmspace_destroy(proc->vm);
        kfree(proc);
        return -1;
    }
    proc->brk = proc->brk_start;

    uint64_t user_rsp = 0;
    if (setup_user_stack(proc, &user_rsp) != 0) {
        printk(KERN_ERR "exec: %s: cannot map a user stack\n", proc->name);
        vmspace_destroy(proc->vm);
        kfree(proc);
        return -1;
    }

    /*
     * Traps taken in ring 3 switch to TSS.rsp0, which must not be the stack
     * user_enter() is standing on -- the CPU would push the trap frame over
     * the frame we need to unwind to. Give the process its own kernel stack.
     */
    proc->kstack_size = USER_KSTACK_SIZE;
    proc->kstack = (uint64_t)(uintptr_t)kmalloc(proc->kstack_size);
    if (!proc->kstack) {
        printk(KERN_ERR "exec: %s: cannot allocate a kernel stack\n", proc->name);
        vmspace_destroy(proc->vm);
        kfree(proc);
        return -1;
    }

    printk(KERN_INFO "exec: %s entry=0x%llx brk=0x%llx rsp=0x%llx (%llu pages)\n",
           proc->name, (unsigned long long)proc->entry,
           (unsigned long long)proc->brk, (unsigned long long)user_rsp,
           (unsigned long long)vmspace_resident_pages(proc->vm));

    current_uproc = proc;
    gdt_set_kernel_stack((proc->kstack + proc->kstack_size) & ~0xFULL);
    vmspace_switch(proc->vm);

    int status = (int)user_enter(proc->entry, user_rsp);

    /* Back in ring 0 on the kernel's own stack: undo everything. */
    vmspace_switch(NULL);
    current_uproc = NULL;

    printk(KERN_INFO "exec: %s exited with status %d\n", proc->name, status);

    kfree((void*)(uintptr_t)proc->kstack);
    vmspace_destroy(proc->vm);
    kfree(proc);
    return status;
}

/*
 * The statically linked /sbin/init image, embedded in the kernel by
 * scripts/mkblob.py so there is something to exec before any filesystem is
 * mounted.
 */
extern const unsigned char subos_init_elf[];
extern const size_t subos_init_elf_len;

int exec_run_builtin_init(void) {
    return exec_user_image("init", subos_init_elf, subos_init_elf_len);
}

#else  /* !__x86_64__ */

uproc_t* uproc_current(void) { return NULL; }
bool uaccess_ok(uint64_t uaddr, size_t len) { (void)uaddr; (void)len; return false; }
int uaccess_copy_from_user(void* dst, uint64_t usrc, size_t len) {
    (void)dst; (void)usrc; (void)len; return -1;
}
int uaccess_copy_to_user(uint64_t udst, const void* src, size_t len) {
    (void)udst; (void)src; (void)len; return -1;
}
long uaccess_copy_string(char* dst, uint64_t usrc, size_t max) {
    (void)dst; (void)usrc; (void)max; return -1;
}

void uproc_exit(int status) {
    (void)status;
    panic("uproc_exit() with no ring 3 support on this architecture");
    __builtin_unreachable();
}

void uproc_fault(const char* reason, uint64_t addr) {
    (void)reason; (void)addr;
    panic("uproc_fault() with no ring 3 support on this architecture");
    __builtin_unreachable();
}

int exec_user_image(const char* name, const void* image, size_t len) {
    (void)image; (void)len;
    printk(KERN_WARNING "exec: %s: ring 3 execution is only implemented on x86_64\n",
           name ? name : "user");
    return -1;
}

int exec_run_builtin_init(void) {
    printk(KERN_WARNING "exec: no userland init on this architecture yet\n");
    return -1;
}

#endif /* __x86_64__ */
