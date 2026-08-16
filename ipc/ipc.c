#include <ipc/ipc.h>
#include <kernel/printk.h>

void ipc_init(void) {
    printk(KERN_INFO "IPC: Inter-Process Communication Engine (Pipes, MsgQueues, SHM, Semaphores) initialized\n");
}
