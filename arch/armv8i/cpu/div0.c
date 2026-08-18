/*
 * SUB-OS ARMv8i Division-by-Zero and POSIX Signal Stub
 */

#include <stdint.h>

int raise(int sig) {
    (void)sig;
    return 0;
}

void __aeabi_idiv0(void) {
}

void __aeabi_ldiv0(void) {
}
