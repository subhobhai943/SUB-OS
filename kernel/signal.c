#include <kernel/signal.h>
#include <kernel/printk.h>
#include <lib/string.h>

static sighandler_t signal_handlers[NSIG];
static sigset_t pending_signals = 0;

void signal_init(void) {
    memset(signal_handlers, 0, sizeof(signal_handlers));
    pending_signals = 0;
    printk(KERN_INFO "SIGNAL: POSIX Signal subsystem initialized (32 signals)\n");
}

sighandler_t signal_set_handler(int signum, sighandler_t handler) {
    if (signum <= 0 || signum >= NSIG) return SIG_ERR;
    sighandler_t old = signal_handlers[signum];
    signal_handlers[signum] = handler;
    return old;
}

int signal_send(uint32_t pid, int signum) {
    (void)pid;
    if (signum <= 0 || signum >= NSIG) return -1;
    pending_signals |= (1U << signum);
    signal_deliver_pending();
    return 0;
}

void signal_deliver_pending(void) {
    for (int i = 1; i < NSIG; i++) {
        if (pending_signals & (1U << i)) {
            pending_signals &= ~(1U << i);
            if (signal_handlers[i] && signal_handlers[i] != SIG_IGN && signal_handlers[i] != SIG_DFL) {
                signal_handlers[i](i);
            }
        }
    }
}
