#ifndef _KERNEL_MODULE_H
#define _KERNEL_MODULE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MODULE_NAME_LEN 32
#define MAX_MODULES     16

typedef struct kernel_module {
    char name[MODULE_NAME_LEN];
    char description[64];
    char author[32];
    char version[16];
    size_t size;
    int (*init)(void);
    void (*cleanup)(void);
    bool loaded;
} kernel_module_t;

void module_init_subsystem(void);
int module_load(const char* name, int (*init_fn)(void), void (*cleanup_fn)(void), size_t size);
int module_unload(const char* name);
size_t module_get_count(void);
const kernel_module_t* module_get(size_t index);

#endif // _KERNEL_MODULE_H
