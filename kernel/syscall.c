#include <kernel/syscall.h>
#include <fs/vfs.h>
#include <kernel/task.h>
#include <kernel/exec.h>
#include <kernel/signal.h>
#include <kernel/printk.h>
#include <drivers/tty.h>
#include <mm/vmspace.h>

#if defined(__x86_64__)
#include <arch/x86_64/idt.h>
#include <arch/x86_64/isr.h>

/* Provided by arch/x86_64/cpu/isr_stubs.asm. */
extern void isr_stub_syscall(void);

#define SYSCALL_VECTOR 0x80

/*
 * Bring the trap frame into the SysV syscall shape. The kernel follows the
 * Linux x86_64 convention -- number in RAX, arguments in RDI, RSI, RDX, R10,
 * R8, R9 -- because RCX is clobbered by the CPU on a SYSCALL entry and R10
 * stands in for it. Keeping the same order here means user code does not have
 * to care which gate it came through.
 */
static void syscall_trap(registers_t* regs) {
    regs->rax = (uint64_t)syscall_handler(regs->rax,
                                          regs->rdi, regs->rsi, regs->rdx,
                                          regs->r10, regs->r8, regs->r9);
}

void syscall_init(void) {
    /*
     * DPL 3 so ring 3 may raise it, and an interrupt gate (0xEE) so IF is
     * cleared on entry. The CPU takes the stack from TSS.rsp0, which the exec
     * path points at a kernel stack owned by the running process.
     */
    idt_set_gate(SYSCALL_VECTOR, (void*)isr_stub_syscall, 0xEE);
    isr_register_handler(SYSCALL_VECTOR, syscall_trap);
    printk(KERN_INFO "SYSCALL: INT 0x%02X user gate installed (DPL 3)\n", SYSCALL_VECTOR);
}
#else
void syscall_init(void) {
    printk(KERN_INFO "SYSCALL: dispatcher online (no ring 3 gate on this architecture)\n");
}
#endif

#define EFAULT_  (-14)
#define ENOSYS_  (-38)
#define EINVAL_  (-22)

/*
 * Validate a user-supplied buffer.
 *
 * When a syscall arrives from ring 3 the process page tables are still live,
 * so a user pointer can be dereferenced directly -- which is exactly why the
 * range has to be checked first. Without this, a program could pass 0x100000
 * and have the kernel copy its own text out to the console.
 *
 * Calls made from kernel context (uproc_current() == NULL) are trusted.
 */
static bool user_buffer_ok(uint64_t ptr, size_t len) {
    if (!uproc_current()) return true;
    return uaccess_ok(ptr, len);
}

static int64_t sys_write(int fd, uint64_t buf, size_t count) {
    if (count == 0) return 0;
    if (!user_buffer_ok(buf, count)) return EFAULT_;

    /* stdout and stderr go straight to the console: a process should be able
     * to say something before any filesystem is mounted. console_write() is
     * used rather than tty_write() so the bytes reach the serial port and the
     * framebuffer console too, exactly like kernel output. */
    if (fd == 1 || fd == 2) {
        console_write((const char*)(uintptr_t)buf, count);
        return (int64_t)count;
    }
    return (int64_t)vfs_write(fd, (const void*)(uintptr_t)buf, count);
}

static int64_t sys_read(int fd, uint64_t buf, size_t count) {
    if (count == 0) return 0;
    if (!user_buffer_ok(buf, count)) return EFAULT_;
    return (int64_t)vfs_read(fd, (void*)(uintptr_t)buf, count);
}

/* Grow or query the program break. */
static int64_t sys_brk(uint64_t addr) {
    uproc_t* proc = uproc_current();
    if (!proc) return EINVAL_;

    if (addr == 0 || addr < proc->brk_start) return (int64_t)proc->brk;
    if (addr > USER_STACK_TOP - (USER_STACK_PAGES * VM_PAGE_SIZE)) return (int64_t)proc->brk;

    if (addr > proc->brk) {
        if (vmspace_alloc(proc->vm, proc->brk, (size_t)(addr - proc->brk),
                          VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER) != 0) {
            return (int64_t)proc->brk;
        }
    }
    proc->brk = addr;
    return (int64_t)proc->brk;
}

static int64_t sys_open(uint64_t path, int flags) {
    char kpath[256];
    if (uproc_current()) {
        if (uaccess_copy_string(kpath, path, sizeof(kpath)) < 0) return EFAULT_;
        return (int64_t)vfs_open(kpath, flags);
    }
    return (int64_t)vfs_open((const char*)(uintptr_t)path, flags);
}

static int64_t sys_mkdir(uint64_t path, uint32_t mode) {
    char kpath[256];
    if (uproc_current()) {
        if (uaccess_copy_string(kpath, path, sizeof(kpath)) < 0) return EFAULT_;
        return (int64_t)vfs_mkdir(kpath, mode);
    }
    return (int64_t)vfs_mkdir((const char*)(uintptr_t)path, mode);
}

int64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    switch (num) {
        case SYS_read:
            return sys_read((int)a1, a2, (size_t)a3);
        case SYS_write:
            return sys_write((int)a1, a2, (size_t)a3);
        case SYS_open:
            return sys_open(a1, (int)a2);
        case SYS_close:
            return (int64_t)vfs_close((int)a1);
        case SYS_lseek:
            return (int64_t)vfs_lseek((int)a1, (off_t)a2, (int)a3);
        case SYS_brk:
            return sys_brk(a1);
        case SYS_getpid: {
            uproc_t* proc = uproc_current();
            return (int64_t)(proc ? proc->pid : task_get_pid());
        }
        case SYS_kill:
            return (int64_t)signal_send((uint32_t)a1, (int)a2);
        case SYS_mkdir:
            return sys_mkdir(a1, (uint32_t)a2);
        case SYS_exit:
            /* A ring 3 process unwinds back into exec_user_image(); a kernel
             * thread just tears itself down. */
            if (uproc_current()) uproc_exit((int)a1);
            task_exit((int)a1);
            return 0;
        default:
            return ENOSYS_;
    }
}
