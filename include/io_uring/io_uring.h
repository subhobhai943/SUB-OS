#ifndef _IO_URING_IO_URING_H
#define _IO_URING_IO_URING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IORING_QUEUE_DEPTH 32

/* io_uring opcodes */
#define IORING_OP_NOP         0
#define IORING_OP_READV       1
#define IORING_OP_WRITEV      2
#define IORING_OP_FSYNC       3
#define IORING_OP_READ_FIXED  4
#define IORING_OP_WRITE_FIXED 5
#define IORING_OP_TIMEOUT     6
#define IORING_OP_CLOSE       7

/* io_uring Submission Queue Entry (SQE) */
typedef struct io_uring_sqe {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t ioprio;
    int32_t  fd;
    uint64_t off;
    uint64_t addr;
    uint32_t len;
    uint32_t cancel_flags;
    uint64_t user_data;
} io_uring_sqe_t;

/* io_uring Completion Queue Entry (CQE) */
typedef struct io_uring_cqe {
    uint64_t user_data;
    int32_t  res;
    uint32_t flags;
} io_uring_cqe_t;

typedef struct io_uring_ring {
    io_uring_sqe_t sqes[IORING_QUEUE_DEPTH];
    uint32_t sq_head;
    uint32_t sq_tail;

    io_uring_cqe_t cqes[IORING_QUEUE_DEPTH];
    uint32_t cq_head;
    uint32_t cq_tail;

    uint32_t submitted;
    uint32_t completed;
} io_uring_ring_t;

void io_uring_init(void);
io_uring_ring_t* io_uring_get_default_ring(void);
io_uring_sqe_t* io_uring_get_sqe(io_uring_ring_t* ring);
int io_uring_submit(io_uring_ring_t* ring);
io_uring_cqe_t* io_uring_peek_cqe(io_uring_ring_t* ring);
void io_uring_cqe_seen(io_uring_ring_t* ring, io_uring_cqe_t* cqe);

#endif // _IO_URING_IO_URING_H
