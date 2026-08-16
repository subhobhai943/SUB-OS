#ifndef _IPC_IPC_H
#define _IPC_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000
#define IPC_RMID   0
#define IPC_SET    1
#define IPC_STAT   2

typedef int32_t key_t;

void ipc_init(void);

#endif // _IPC_IPC_H
