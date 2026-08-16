#include <arch/x86_64/cpuid.h>
#include <lib/string.h>

static cpu_info_t global_cpu_info;
static bool cpu_detected = false;

static inline void cpuid(uint32_t code, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(code));
}

void cpuid_detect(cpu_info_t* info) {
    uint32_t eax, ebx, ecx, edx;

    memset(info, 0, sizeof(cpu_info_t));

    // 1. Get Vendor String
    cpuid(0, &eax, &ebx, &ecx, &edx);
    ((uint32_t*)info->vendor)[0] = ebx;
    ((uint32_t*)info->vendor)[1] = edx;
    ((uint32_t*)info->vendor)[2] = ecx;
    info->vendor[12] = '\0';

    // 2. Get Features & Model Info
    cpuid(1, &eax, &ebx, &ecx, &edx);
    info->stepping = eax & 0x0F;
    info->model_id = (eax >> 4) & 0x0F;
    info->family   = (eax >> 8) & 0x0F;

    info->has_fpu   = (edx & (1 << 0)) != 0;
    info->has_tsc   = (edx & (1 << 4)) != 0;
    info->has_msr   = (edx & (1 << 5)) != 0;
    info->has_pae   = (edx & (1 << 6)) != 0;
    info->has_apic  = (edx & (1 << 9)) != 0;
    info->has_sse   = (edx & (1 << 25)) != 0;
    info->has_sse2  = (edx & (1 << 26)) != 0;
    info->has_sse3  = (ecx & (1 << 0)) != 0;
    info->has_sse4_1= (ecx & (1 << 19)) != 0;
    info->has_sse4_2= (ecx & (1 << 20)) != 0;
    info->has_avx   = (ecx & (1 << 28)) != 0;
    info->has_rdrand= (ecx & (1 << 30)) != 0;

    // 3. Get Brand String (Extended Functions)
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        uint32_t* brand = (uint32_t*)info->model;
        cpuid(0x80000002, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, &brand[8], &brand[9], &brand[10], &brand[11]);
        info->model[48] = '\0';
    } else {
        strcpy(info->model, "x86_64 Compatible Processor");
    }

    global_cpu_info = *info;
    cpu_detected = true;
}

const cpu_info_t* cpuid_get_info(void) {
    if (!cpu_detected) {
        cpuid_detect(&global_cpu_info);
    }
    return &global_cpu_info;
}
