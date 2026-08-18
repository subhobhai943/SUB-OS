#include <arch/armv8i/cpu.h>
#include <lib/string.h>
#include <kernel/printk.h>

static armv8i_cpu_info_t global_cpu_info;

uint32_t armv8i_get_midr(void) {
    uint32_t midr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
    return midr;
}

uint32_t armv8i_get_mpidr(void) {
    uint32_t mpidr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
    return mpidr;
}

void armv8i_cpu_init(void) {
    memset(&global_cpu_info, 0, sizeof(global_cpu_info));
    global_cpu_info.midr = armv8i_get_midr();
    global_cpu_info.mpidr = armv8i_get_mpidr();
    global_cpu_info.implementer = (global_cpu_info.midr >> 24) & 0xFF;
    global_cpu_info.part_number = (global_cpu_info.midr >> 4) & 0xFFF;
    global_cpu_info.architecture = (global_cpu_info.midr >> 16) & 0xF;

    const char* part_str = "ARM Cortex-A (ARMv8i 32-Bit)";
    if (global_cpu_info.part_number == 0xC0F) part_str = "ARM Cortex-A15 (ARMv7-A/v8i)";
    else if (global_cpu_info.part_number == 0xC07) part_str = "ARM Cortex-A7 (ARMv7-A/v8i)";
    else if (global_cpu_info.part_number == 0xD03) part_str = "ARM Cortex-A53 (AArch32)";
    else if (global_cpu_info.part_number == 0xD07) part_str = "ARM Cortex-A57 (AArch32)";

    strncpy(global_cpu_info.model_name, part_str, sizeof(global_cpu_info.model_name) - 1);

    printk(KERN_INFO "CPU: %s (Implementer: 0x%02x, Part: 0x%03x, AArch32)\n",
           global_cpu_info.model_name, global_cpu_info.implementer, global_cpu_info.part_number);
}

const armv8i_cpu_info_t* armv8i_get_cpu_info(void) {
    return &global_cpu_info;
}
