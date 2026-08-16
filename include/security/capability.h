#ifndef _SECURITY_CAPABILITY_H
#define _SECURITY_CAPABILITY_H

#include <stdint.h>
#include <stdbool.h>

/* Linux standard capability bit definitions */
#define CAP_CHOWN            0
#define CAP_DAC_OVERRIDE     1
#define CAP_DAC_READ_SEARCH  2
#define CAP_FOWNER           3
#define CAP_FSETID           4
#define CAP_KILL             5
#define CAP_SETGID           6
#define CAP_SETUID           7
#define CAP_SETPCAP          8
#define CAP_NET_BIND_SERVICE 10
#define CAP_NET_BROADCAST    11
#define CAP_NET_ADMIN        12
#define CAP_NET_RAW          13
#define CAP_SYS_MODULE       16
#define CAP_SYS_RAWIO        17
#define CAP_SYS_PTRACE       19
#define CAP_SYS_PACCT        20
#define CAP_SYS_ADMIN        21
#define CAP_SYS_BOOT         22
#define CAP_SYS_NICE         23
#define CAP_SYS_RESOURCE     24
#define CAP_SYS_TIME         25

typedef uint64_t kernel_cap_t;

#define CAP_FULL_SET         ((kernel_cap_t)0xFFFFFFFFFFFFFFFFULL)
#define CAP_EMPTY_SET        ((kernel_cap_t)0)

bool cap_capable(kernel_cap_t caps, int cap);
void cap_raise(kernel_cap_t* caps, int cap);
void cap_lower(kernel_cap_t* caps, int cap);

#endif // _SECURITY_CAPABILITY_H
