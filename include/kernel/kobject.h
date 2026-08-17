#ifndef _KERNEL_KOBJECT_H
#define _KERNEL_KOBJECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KOBJ_NAME_LEN 32
#define MAX_KOBJECTS  64

typedef enum {
    KOBJ_TYPE_GENERIC = 0,
    KOBJ_TYPE_BUS,
    KOBJ_TYPE_DEVICE,
    KOBJ_TYPE_DRIVER,
    KOBJ_TYPE_CLASS,
    KOBJ_TYPE_SUBSYS
} kobj_type_t;

typedef struct kobject {
    char name[KOBJ_NAME_LEN];
    kobj_type_t type;
    struct kobject* parent;
    uint32_t ref_count;
    char path[128];
    bool in_use;
} kobject_t;

void kobject_subsystem_init(void);
kobject_t* kobject_create_and_add(const char* name, kobj_type_t type, kobject_t* parent);
void kobject_get(kobject_t* kobj);
void kobject_put(kobject_t* kobj);

size_t kobject_get_total_count(void);
const kobject_t* kobject_get_at(size_t index);
void kobject_dump_tree(void);

#endif // _KERNEL_KOBJECT_H
