#ifndef _ARCH_ARMV8I_CPU_H
#define _ARCH_ARMV8I_CPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t midr;
    uint32_t mpidr;
    uint32_t implementer;
    uint32_t part_number;
    uint32_t architecture;
    char model_name[64];
} armv8i_cpu_info_t;

void armv8i_cpu_init(void);
const armv8i_cpu_info_t* armv8i_get_cpu_info(void);
uint32_t armv8i_get_midr(void);
uint32_t armv8i_get_mpidr(void);

#endif // _ARCH_ARMV8I_CPU_H
