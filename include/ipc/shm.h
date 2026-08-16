#ifndef _IPC_SHM_H
#define _IPC_SHM_H

#include <ipc/ipc.h>

#define MAX_SHM_SEGS 16
#define SHM_MAX_SIZE 65536

typedef struct shmid_ds {
    int shmid;
    key_t key;
    size_t size;
    void* addr;
    uint32_t nattch;
    bool in_use;
} shmid_ds_t;

int shmget(key_t key, size_t size, int shmflg);
void* shmat(int shmid, const void* shmaddr, int shmflg);
int shmdt(const void* shmaddr);
int shmctl(int shmid, int cmd, struct shmid_ds* buf);

#endif // _IPC_SHM_H
