#ifndef _KERNEL_NAMESPACE_H
#define _KERNEL_NAMESPACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CLONE_NEWPID  0x20000000
#define CLONE_NEWUTS  0x04000000
#define CLONE_NEWIPC  0x08000000
#define CLONE_NEWNET  0x40000000
#define CLONE_NEWNS   0x00020000

#define MAX_NAMESPACES 16

typedef struct uts_namespace {
    char hostname[64];
    char domainname[64];
} uts_namespace_t;

typedef struct nsproxy {
    uint32_t id;
    char name[32];
    uts_namespace_t uts;
    uint32_t pid_offset;
    bool isolated_mounts;
    bool in_use;
} nsproxy_t;

void namespace_init(void);
nsproxy_t* namespace_create(const char* name, uint32_t flags);
nsproxy_t* namespace_get_current(void);
int namespace_switch(uint32_t id);
size_t namespace_get_count(void);
const nsproxy_t* namespace_get(size_t index);

#endif // _KERNEL_NAMESPACE_H
