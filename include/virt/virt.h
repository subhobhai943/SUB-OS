#ifndef _VIRT_VIRT_H
#define _VIRT_VIRT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HYPERVISOR_NONE = 0,
    HYPERVISOR_KVM,
    HYPERVISOR_QEMU,
    HYPERVISOR_VMWARE,
    HYPERVISOR_VBOX,
    HYPERVISOR_XEN,
    HYPERVISOR_HYPERV,
    HYPERVISOR_UNKNOWN
} hypervisor_type_t;

typedef struct {
    hypervisor_type_t type;
    char name[32];
    char signature[16];
    bool nested_virt;
} hypervisor_info_t;

void virt_init(void);
const hypervisor_info_t* virt_get_hypervisor_info(void);
bool virt_is_virtualized(void);

#endif // _VIRT_VIRT_H
