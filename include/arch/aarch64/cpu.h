#ifndef _ARCH_AARCH64_CPU_H
#define _ARCH_AARCH64_CPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t implementer;
    uint32_t variant;
    uint32_t architecture;
    uint32_t part_num;
    uint32_t revision;
    char     model_name[64];
    uint32_t current_el;
    uint64_t core_frequency_hz;
} aarch64_cpu_info_t;

void aarch64_cpu_init(void);
const aarch64_cpu_info_t* aarch64_get_cpu_info(void);
uint32_t aarch64_get_current_el(void);
uint64_t aarch64_get_midr(void);
uint64_t aarch64_get_mpidr(void);

#endif // _ARCH_AARCH64_CPU_H
