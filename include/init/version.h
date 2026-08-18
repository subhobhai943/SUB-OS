#ifndef _INIT_VERSION_H
#define _INIT_VERSION_H

#define SUBOS_VERSION_MAJOR 0
#define SUBOS_VERSION_MINOR 2
#define SUBOS_VERSION_PATCH 0
#define SUBOS_VERSION_CODENAME "Titan"

#if defined(__x86_64__)
#define SUBOS_VERSION_STRING "0.2.0-lts (x86_64)"
#define SUBOS_BUILD_TARGET "x86_64-elf-baremetal"
#define SUBOS_COMPILER "x86_64-elf-gcc"
#elif defined(__aarch64__)
#define SUBOS_VERSION_STRING "0.2.0-lts (aarch64)"
#define SUBOS_BUILD_TARGET "aarch64-linux-gnu-baremetal"
#define SUBOS_COMPILER "aarch64-linux-gnu-gcc"
#elif defined(__arm__) || defined(__armv8i__)
#define SUBOS_VERSION_STRING "0.2.0-lts (armv8i)"
#define SUBOS_BUILD_TARGET "arm-linux-gnueabihf-baremetal"
#define SUBOS_COMPILER "arm-linux-gnueabihf-gcc"
#else
#define SUBOS_VERSION_STRING "0.2.0-lts (generic)"
#define SUBOS_BUILD_TARGET "generic-elf-baremetal"
#define SUBOS_COMPILER "gcc"
#endif

const char* kernel_get_version(void);
const char* kernel_get_build_banner(void);

#endif // _INIT_VERSION_H
