#ifndef _KERNEL_MAIN_H
#define _KERNEL_MAIN_H

#include "types.h"
#include "printk.h"
#include <arch/arch.h>

#define KERNEL_NAME        "SUB-OS"
#define KERNEL_VERSION     "0.2.0-lts"
#define KERNEL_ARCH        "x86_64"
#define KERNEL_AUTHOR      "SUB-OS Development Team"

void kernel_main(void* memory_map, uint64_t memory_map_count);

#endif // _KERNEL_MAIN_H
