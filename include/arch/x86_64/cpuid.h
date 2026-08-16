#ifndef _ARCH_X86_64_CPUID_H
#define _ARCH_X86_64_CPUID_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char vendor[13];
    char model[49];
    uint32_t family;
    uint32_t model_id;
    uint32_t stepping;
    bool has_fpu;
    bool has_tsc;
    bool has_msr;
    bool has_pae;
    bool has_apic;
    bool has_sse;
    bool has_sse2;
    bool has_sse3;
    bool has_sse4_1;
    bool has_sse4_2;
    bool has_avx;
    bool has_rdrand;
} cpu_info_t;

static inline void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(0));
}

void cpuid_detect(cpu_info_t* info);
const cpu_info_t* cpuid_get_info(void);

#endif // _ARCH_X86_64_CPUID_H
