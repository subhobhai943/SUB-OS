#ifndef _KERNEL_SIGNAL_H
#define _KERNEL_SIGNAL_H

#include <stdint.h>
#include <stddef.h>

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV   11
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGUSR1   10
#define SIGUSR2   12

#define NSIG      32

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

typedef uint32_t sigset_t;

struct sigaction {
    sighandler_t sa_handler;
    sigset_t     sa_mask;
    int          sa_flags;
};

void signal_init(void);
sighandler_t signal_set_handler(int signum, sighandler_t handler);
int signal_send(uint32_t pid, int signum);
void signal_deliver_pending(void);

#endif // _KERNEL_SIGNAL_H
