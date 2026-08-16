#ifndef _IPC_SEM_H
#define _IPC_SEM_H

#include <ipc/ipc.h>

#define MAX_SEM_SETS 16
#define MAX_SEMS_PER_SET 16

struct sembuf {
    unsigned short sem_num;
    short          sem_op;
    short          sem_flg;
};

typedef struct semid_ds {
    int semid;
    key_t key;
    uint32_t nsems;
    int values[MAX_SEMS_PER_SET];
    bool in_use;
} semid_ds_t;

int semget(key_t key, int nsems, int semflg);
int semop(int semid, struct sembuf* sops, size_t nsops);
int semctl(int semid, int semnum, int cmd, ...);

#endif // _IPC_SEM_H
