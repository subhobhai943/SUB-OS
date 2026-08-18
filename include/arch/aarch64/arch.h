#ifndef _ARCH_AARCH64_ARCH_H
#define _ARCH_AARCH64_ARCH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t r[31]; // X0 - X30
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
} __attribute__((packed)) aarch64_context_t;

void aarch64_init(void);

#endif // _ARCH_AARCH64_ARCH_H
