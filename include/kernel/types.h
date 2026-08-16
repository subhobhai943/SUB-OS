#ifndef _KERNEL_TYPES_H
#define _KERNEL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef int32_t   pid_t;
typedef int32_t   tid_t;
typedef uint32_t  uid_t;
typedef uint32_t  gid_t;
typedef int64_t   off_t;
typedef int64_t   ssize_t;
typedef uint32_t  mode_t;
typedef uint32_t  dev_t;
typedef uint64_t  ino_t;
typedef uint64_t  time_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define ALIGN_UP(addr, align)   (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#endif // _KERNEL_TYPES_H
