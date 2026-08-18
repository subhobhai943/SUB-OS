#include <arch/aarch64/cpu.h>
#include <lib/string.h>
#include <kernel/printk.h>

static aarch64_cpu_info_t cpu_info;

uint64_t aarch64_get_midr(void) {
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
    return midr;
}

uint64_t aarch64_get_mpidr(void) {
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return mpidr;
}

uint32_t aarch64_get_current_el(void) {
    uint64_t cur_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(cur_el));
    return (uint32_t)(cur_el >> 2);
}

void aarch64_cpu_init(void) {
    memset(&cpu_info, 0, sizeof(cpu_info));

    uint64_t midr = aarch64_get_midr();
    cpu_info.implementer  = (uint32_t)((midr >> 24) & 0xFF);
    cpu_info.variant      = (uint32_t)((midr >> 20) & 0x0F);
    cpu_info.architecture = (uint32_t)((midr >> 16) & 0x0F);
    cpu_info.part_num     = (uint32_t)((midr >> 4)  & 0xFFF);
    cpu_info.revision     = (uint32_t)(midr & 0x0F);
    cpu_info.current_el   = aarch64_get_current_el();

    if (cpu_info.implementer == 0x41) { // ARM Limited
        switch (cpu_info.part_num) {
            case 0xD03: strcpy(cpu_info.model_name, "ARM Cortex-A53 (ARMv8.0-A)"); break;
            case 0xD07: strcpy(cpu_info.model_name, "ARM Cortex-A57 (ARMv8.0-A)"); break;
            case 0xD08: strcpy(cpu_info.model_name, "ARM Cortex-A72 (ARMv8.0-A)"); break;
            case 0xD09: strcpy(cpu_info.model_name, "ARM Cortex-A73 (ARMv8.0-A)"); break;
            case 0xD0B: strcpy(cpu_info.model_name, "ARM Cortex-A76 (ARMv8.2-A)"); break;
            case 0xD40: strcpy(cpu_info.model_name, "ARM Neoverse-V1 (ARMv8.4-A)"); break;
            default:    strcpy(cpu_info.model_name, "ARMv8-A Compatible 64-Bit Processor"); break;
        }
    } else if (cpu_info.implementer == 0x61) { // Apple Silicon
        strcpy(cpu_info.model_name, "Apple Silicon M-Series (ARMv8-A)");
    } else {
        strcpy(cpu_info.model_name, "Generic AArch64 / ARMv8 64-Bit Core");
    }

    printk(KERN_INFO "CPU: %s (Implementer: 0x%02x, Part: 0x%03x, EL%u)\n",
           cpu_info.model_name, cpu_info.implementer, cpu_info.part_num, cpu_info.current_el);
}

const aarch64_cpu_info_t* aarch64_get_cpu_info(void) {
    return &cpu_info;
}
