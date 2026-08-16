#include <ipc/sem.h>
#include <lib/string.h>

static semid_ds_t sem_sets[MAX_SEM_SETS];

int semget(key_t key, int nsems, int semflg) {
    (void)semflg;
    if (nsems <= 0 || nsems > MAX_SEMS_PER_SET) return -1;

    for (int i = 0; i < MAX_SEM_SETS; i++) {
        if (sem_sets[i].in_use && sem_sets[i].key == key) {
            return i;
        }
    }

    for (int i = 0; i < MAX_SEM_SETS; i++) {
        if (!sem_sets[i].in_use) {
            sem_sets[i].in_use = true;
            sem_sets[i].key = key;
            sem_sets[i].semid = i;
            sem_sets[i].nsems = (uint32_t)nsems;
            for (int s = 0; s < nsems; s++) {
                sem_sets[i].values[s] = 0;
            }
            return i;
        }
    }
    return -1;
}

int semop(int semid, struct sembuf* sops, size_t nsops) {
    if (semid < 0 || semid >= MAX_SEM_SETS || !sem_sets[semid].in_use || !sops) return -1;

    semid_ds_t* set = &sem_sets[semid];
    for (size_t i = 0; i < nsops; i++) {
        if (sops[i].sem_num >= set->nsems) return -1;
        set->values[sops[i].sem_num] += sops[i].sem_op;
    }
    return 0;
}

int semctl(int semid, int semnum, int cmd, ...) {
    if (semid < 0 || semid >= MAX_SEM_SETS || !sem_sets[semid].in_use) return -1;

    if (cmd == IPC_RMID) {
        memset(&sem_sets[semid], 0, sizeof(semid_ds_t));
        return 0;
    }
    if (semnum >= 0 && semnum < (int)sem_sets[semid].nsems) {
        return sem_sets[semid].values[semnum];
    }
    return 0;
}
