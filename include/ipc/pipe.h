#ifndef _IPC_PIPE_H
#define _IPC_PIPE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <fs/vfs.h>

#define PIPE_BUF_SIZE 4096

typedef struct pipe {
    uint8_t buffer[PIPE_BUF_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    bool read_open;
    bool write_open;
    vfs_node_t* read_node;
    vfs_node_t* write_node;
} pipe_t;

int pipe_create(int pipefd[2]);
ssize_t pipe_read(pipe_t* p, void* buf, size_t count);
ssize_t pipe_write(pipe_t* p, const void* buf, size_t count);
void pipe_close_read(pipe_t* p);
void pipe_close_write(pipe_t* p);

#endif // _IPC_PIPE_H
