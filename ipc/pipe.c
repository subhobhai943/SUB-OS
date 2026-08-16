#include <ipc/pipe.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

static ssize_t pipe_vfs_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)offset;
    pipe_t* p = (pipe_t*)node->ptr;
    if (!p) return 0;
    return pipe_read(p, buffer, size);
}

static ssize_t pipe_vfs_write(vfs_node_t* node, off_t offset, size_t size, const uint8_t* buffer) {
    (void)offset;
    pipe_t* p = (pipe_t*)node->ptr;
    if (!p) return 0;
    return pipe_write(p, buffer, size);
}

int pipe_create(int pipefd[2]) {
    if (!pipefd) return -1;

    pipe_t* p = (pipe_t*)kzalloc(sizeof(pipe_t));
    if (!p) return -1;

    p->read_open = true;
    p->write_open = true;

    vfs_node_t* rnode = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    vfs_node_t* wnode = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    if (!rnode || !wnode) {
        kfree(p);
        if (rnode) kfree(rnode);
        if (wnode) kfree(wnode);
        return -1;
    }

    strcpy(rnode->name, "pipe:[r]");
    rnode->flags = FS_CHARDEVICE;
    rnode->read = pipe_vfs_read;
    rnode->ptr = (struct vfs_node*)p;

    strcpy(wnode->name, "pipe:[w]");
    wnode->flags = FS_CHARDEVICE;
    wnode->write = pipe_vfs_write;
    wnode->ptr = (struct vfs_node*)p;

    p->read_node = rnode;
    p->write_node = wnode;

    pipefd[0] = vfs_open_node(rnode, O_RDONLY);
    pipefd[1] = vfs_open_node(wnode, O_WRONLY);

    return 0;
}

ssize_t pipe_read(pipe_t* p, void* buf, size_t count) {
    if (!p || !buf || count == 0) return 0;

    size_t read_bytes = 0;
    uint8_t* out = (uint8_t*)buf;

    while (read_bytes < count && p->count > 0) {
        out[read_bytes++] = p->buffer[p->tail];
        p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
        p->count--;
    }

    return (ssize_t)read_bytes;
}

ssize_t pipe_write(pipe_t* p, const void* buf, size_t count) {
    if (!p || !buf || count == 0) return 0;
    if (!p->read_open) return -1; // EPIPE

    size_t written = 0;
    const uint8_t* in = (const uint8_t*)buf;

    while (written < count && p->count < PIPE_BUF_SIZE) {
        p->buffer[p->head] = in[written++];
        p->head = (p->head + 1) % PIPE_BUF_SIZE;
        p->count++;
    }

    return (ssize_t)written;
}

void pipe_close_read(pipe_t* p) {
    if (p) {
        p->read_open = false;
        if (!p->write_open) kfree(p);
    }
}

void pipe_close_write(pipe_t* p) {
    if (p) {
        p->write_open = false;
        if (!p->read_open) kfree(p);
    }
}
