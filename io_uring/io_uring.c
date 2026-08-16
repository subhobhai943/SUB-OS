#include <io_uring/io_uring.h>
#include <fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>

static io_uring_ring_t default_ring;

void io_uring_init(void) {
    memset(&default_ring, 0, sizeof(default_ring));
    printk(KERN_INFO "IO_URING: Async I/O Ring initialized (Depth: %d SQEs/CQEs)\n", IORING_QUEUE_DEPTH);
}

io_uring_ring_t* io_uring_get_default_ring(void) {
    return &default_ring;
}

io_uring_sqe_t* io_uring_get_sqe(io_uring_ring_t* ring) {
    if (!ring) return NULL;
    uint32_t next = (ring->sq_tail + 1) % IORING_QUEUE_DEPTH;
    if (next == ring->sq_head) {
        return NULL; // Queue full
    }

    io_uring_sqe_t* sqe = &ring->sqes[ring->sq_tail];
    memset(sqe, 0, sizeof(io_uring_sqe_t));
    ring->sq_tail = next;
    return sqe;
}

int io_uring_submit(io_uring_ring_t* ring) {
    if (!ring) return -1;
    int processed = 0;

    while (ring->sq_head != ring->sq_tail) {
        io_uring_sqe_t* sqe = &ring->sqes[ring->sq_head];
        int32_t res = 0;

        switch (sqe->opcode) {
            case IORING_OP_NOP:
                res = 0;
                break;

            case IORING_OP_READV:
            case IORING_OP_READ_FIXED: {
                if (sqe->fd >= 0 && sqe->addr && sqe->len > 0) {
                    if (sqe->off > 0) {
                        vfs_lseek(sqe->fd, (off_t)sqe->off, SEEK_SET);
                    }
                    res = (int32_t)vfs_read(sqe->fd, (void*)sqe->addr, sqe->len);
                } else {
                    res = -1;
                }
                break;
            }

            case IORING_OP_WRITEV:
            case IORING_OP_WRITE_FIXED: {
                if (sqe->fd >= 0 && sqe->addr && sqe->len > 0) {
                    if (sqe->off > 0) {
                        vfs_lseek(sqe->fd, (off_t)sqe->off, SEEK_SET);
                    }
                    res = (int32_t)vfs_write(sqe->fd, (const void*)sqe->addr, sqe->len);
                } else {
                    res = -1;
                }
                break;
            }

            case IORING_OP_CLOSE: {
                if (sqe->fd >= 0) {
                    res = vfs_close(sqe->fd);
                }
                break;
            }

            case IORING_OP_FSYNC:
                res = 0;
                break;

            default:
                res = -1;
                break;
        }

        // Post to CQE ring
        uint32_t next_cq = (ring->cq_tail + 1) % IORING_QUEUE_DEPTH;
        if (next_cq != ring->cq_head) {
            io_uring_cqe_t* cqe = &ring->cqes[ring->cq_tail];
            cqe->user_data = sqe->user_data;
            cqe->res       = res;
            cqe->flags     = 0;
            ring->cq_tail  = next_cq;
            ring->completed++;
        }

        ring->sq_head = (ring->sq_head + 1) % IORING_QUEUE_DEPTH;
        ring->submitted++;
        processed++;
    }

    return processed;
}

io_uring_cqe_t* io_uring_peek_cqe(io_uring_ring_t* ring) {
    if (!ring || ring->cq_head == ring->cq_tail) return NULL;
    return &ring->cqes[ring->cq_head];
}

void io_uring_cqe_seen(io_uring_ring_t* ring, io_uring_cqe_t* cqe) {
    (void)cqe;
    if (ring && ring->cq_head != ring->cq_tail) {
        ring->cq_head = (ring->cq_head + 1) % IORING_QUEUE_DEPTH;
    }
}
