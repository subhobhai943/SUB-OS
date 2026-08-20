#ifndef _IPC_SHM_POSIX_H
#define _IPC_SHM_POSIX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define POSIX_SHM_MAX_NAME 64
#define POSIX_SHM_MAX_OBJECTS 32

typedef struct {
    char name[POSIX_SHM_MAX_NAME];
    void* memory;
    size_t size;
    uint32_t mode;
    uint32_t ref_count;
    bool active;
} posix_shm_object_t;

void posix_shm_init(void);
int posix_shm_open(const char* name, int flags, uint32_t mode);
int posix_shm_truncate(int fd, size_t size);
int posix_shm_unlink(const char* name);
posix_shm_object_t* posix_shm_get(int fd);
void posix_shm_dump(void);

#endif // _IPC_SHM_POSIX_H
