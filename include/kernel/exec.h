#ifndef _KERNEL_EXEC_H
#define _KERNEL_EXEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/types.h>
#include <mm/vmspace.h>

/* A process running in ring 3. */
typedef struct uproc {
    pid_t      pid;
    char       name[32];
    vmspace_t* vm;
    uint64_t   entry;
    uint64_t   brk;        /* program break, grown by SYS_brk */
    uint64_t   brk_start;  /* end of the loaded image */
    uint64_t   kstack;     /* kernel stack used for traps out of ring 3 */
    size_t     kstack_size;
    int        exit_code;
    bool       exited;
} uproc_t;

/* The process currently executing in ring 3, or NULL in plain kernel context. */
uproc_t* uproc_current(void);

/*
 * Load a static ELF64 image and run it in ring 3. Blocks until the process
 * exits and returns its exit status, or a negative value if it could not be
 * started. `image` must stay valid for the lifetime of the call.
 */
int exec_user_image(const char* name, const void* image, size_t len);

/* Terminate the running process and unwind back into exec_user_image(). */
void uproc_exit(int status) __attribute__((noreturn));

/*
 * Abort the running process because it faulted. Safe to call from an
 * exception handler; does not return.
 */
void uproc_fault(const char* reason, uint64_t addr) __attribute__((noreturn));

/* Bounds-checked transfers between the kernel and the running process. */
bool uaccess_ok(uint64_t uaddr, size_t len);
int  uaccess_copy_from_user(void* dst, uint64_t usrc, size_t len);
int  uaccess_copy_to_user(uint64_t udst, const void* src, size_t len);
/* Copy a NUL-terminated user string; returns its length or -1. */
long uaccess_copy_string(char* dst, uint64_t usrc, size_t max);

/* Run the built-in /sbin/init image. Returns its exit status. */
int exec_run_builtin_init(void);

#endif /* _KERNEL_EXEC_H */
