#ifndef _KERNEL_SUB_LANG_H
#define _KERNEL_SUB_LANG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Core Engine Initialization & Signature
int sub_kernel_init(void);
const char* sub_get_os_signature(void);
const char* sub_get_sub_lang_version(void);
const char* sub_get_author_credits(void);
void sub_kernel_print_signature(void);

// CPU Power Governor & Thermal Scaling Engine
int sub_power_calc_pstate(int32_t temp_celsius, uint32_t cpu_load_pct);
uint32_t sub_power_get_freq_mhz(int pstate);
uint32_t sub_power_get_voltage_mv(int pstate);

// Algorithm & Math Benchmark
uint32_t sub_benchmark_run(uint32_t iterations);

// Easter Eggs & Kernel Quotes
const char* sub_easter_egg_get(int idx);

// In-Kernel Interpreter & Virtual Machine
int sub_vm_eval_string(const char* code);
int sub_vm_eval_file(const char* filepath);

#endif // _KERNEL_SUB_LANG_H
