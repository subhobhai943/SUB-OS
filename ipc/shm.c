#include <ipc/shm.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

static shmid_ds_t shm_segs[MAX_SHM_SEGS];

int shmget(key_t key, size_t size, int shmflg) {
    (void)shmflg;
    if (size > SHM_MAX_SIZE) return -1;

    for (int i = 0; i < MAX_SHM_SEGS; i++) {
        if (shm_segs[i].in_use && shm_segs[i].key == key) {
            return i;
        }
    }

    for (int i = 0; i < MAX_SHM_SEGS; i++) {
        if (!shm_segs[i].in_use) {
            void* mem = kzalloc(size);
            if (!mem) return -1;

            shm_segs[i].in_use = true;
            shm_segs[i].key = key;
            shm_segs[i].shmid = i;
            shm_segs[i].size = size;
            shm_segs[i].addr = mem;
            shm_segs[i].nattch = 0;
            return i;
        }
    }
    return -1;
}

void* shmat(int shmid, const void* shmaddr, int shmflg) {
    (void)shmaddr; (void)shmflg;
    if (shmid < 0 || shmid >= MAX_SHM_SEGS || !shm_segs[shmid].in_use) return (void*)-1;
    shm_segs[shmid].nattch++;
    return shm_segs[shmid].addr;
}

int shmdt(const void* shmaddr) {
    if (!shmaddr) return -1;
    for (int i = 0; i < MAX_SHM_SEGS; i++) {
        if (shm_segs[i].in_use && shm_segs[i].addr == shmaddr) {
            if (shm_segs[i].nattch > 0) shm_segs[i].nattch--;
            return 0;
        }
    }
    return -1;
}

int shmctl(int shmid, int cmd, struct shmid_ds* buf) {
    if (shmid < 0 || shmid >= MAX_SHM_SEGS || !shm_segs[shmid].in_use) return -1;

    if (cmd == IPC_RMID) {
        if (shm_segs[shmid].addr) kfree(shm_segs[shmid].addr);
        memset(&shm_segs[shmid], 0, sizeof(shmid_ds_t));
        return 0;
    } else if (cmd == IPC_STAT && buf) {
        memcpy(buf, &shm_segs[shmid], sizeof(shmid_ds_t));
        return 0;
    }
    return -1;
}
