#ifndef _KERNEL_PANIC_H
#define _KERNEL_PANIC_H

#include <arch/x86_64/isr.h>

void panic(const char* message);
void panic_with_regs(const char* message, const registers_t* regs);

#endif // _KERNEL_PANIC_H
