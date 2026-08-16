#ifndef _INIT_VERSION_H
#define _INIT_VERSION_H

#define SUBOS_VERSION_MAJOR 0
#define SUBOS_VERSION_MINOR 2
#define SUBOS_VERSION_PATCH 0
#define SUBOS_VERSION_CODENAME "Titan"
#define SUBOS_VERSION_STRING "0.2.0-lts (x86_64)"
#define SUBOS_BUILD_TARGET "x86_64-elf-baremetal"
#define SUBOS_COMPILER "x86_64-elf-gcc (Modular Monolithic)"

const char* kernel_get_version(void);
const char* kernel_get_build_banner(void);

#endif // _INIT_VERSION_H
