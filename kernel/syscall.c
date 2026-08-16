#include <kernel/syscall.h>
#include <fs/vfs.h>
#include <kernel/task.h>
#include <kernel/signal.h>
#include <kernel/printk.h>

void syscall_init(void) {
    printk(KERN_INFO "SYSCALL: x86_64 Fast SYSCALL/SYSRET Dispatcher online\n");
}

int64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    switch (num) {
        case SYS_read:
            return (int64_t)vfs_read((int)a1, (void*)a2, (size_t)a3);
        case SYS_write:
            return (int64_t)vfs_write((int)a1, (const void*)a2, (size_t)a3);
        case SYS_open:
            return (int64_t)vfs_open((const char*)a1, (int)a2);
        case SYS_close:
            return (int64_t)vfs_close((int)a1);
        case SYS_lseek:
            return (int64_t)vfs_lseek((int)a1, (off_t)a2, (int)a3);
        case SYS_getpid:
            return (int64_t)task_get_pid();
        case SYS_kill:
            return (int64_t)signal_send((uint32_t)a1, (int)a2);
        case SYS_mkdir:
            return (int64_t)vfs_mkdir((const char*)a1, (uint32_t)a2);
        case SYS_exit:
            task_exit((int)a1);
            return 0;
        default:
            return -1; // ENOSYS
    }
}
