#ifndef _KERNEL_CPP_KERNEL_H
#define _KERNEL_CPP_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize C++ Runtime and Object-Oriented Subsystems
int cpp_kernel_init(void);

// Print C++ Subsystem Status and Telemetry
void cpp_kernel_print_status(void);

// Run C++ OOP Virtual Dispatch and Template Test
int cpp_test_oop_subsystem(void);

#ifdef __cplusplus
}
#endif

#endif // _KERNEL_CPP_KERNEL_H
