#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* Linux x86_64 Standard Syscall Numbers */
#define SYS_read          0
#define SYS_write         1
#define SYS_open          2
#define SYS_close         3
#define SYS_stat          4
#define SYS_fstat         5
#define SYS_lseek         8
#define SYS_mmap          9
#define SYS_munmap        11
#define SYS_brk           12
#define SYS_rt_sigaction  13
#define SYS_pipe          22
#define SYS_getpid        39
#define SYS_fork          57
#define SYS_execve        59
#define SYS_exit          60
#define SYS_kill          62
#define SYS_mkdir         83
#define SYS_rmdir         84
#define SYS_unlink        87
#define SYS_time          201
#define SYS_reboot        169

typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void syscall_init(void);
int64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

#endif // _KERNEL_SYSCALL_H
