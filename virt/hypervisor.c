#include <virt/virt.h>
#include <virt/virtio.h>
#include <arch/x86_64/cpuid.h>
#include <lib/string.h>
#include <kernel/printk.h>

static hypervisor_info_t host_hypervisor;

void virt_init(void) {
    memset(&host_hypervisor, 0, sizeof(host_hypervisor));

    // Check CPUID hypervisor feature bit (ECX bit 31 on leaf 1)
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);

    if (ecx & (1U << 31)) {
        // Hypervisor present, query leaf 0x40000000
        cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
        char sig[13];
        memcpy(sig, &ebx, 4);
        memcpy(sig + 4, &ecx, 4);
        memcpy(sig + 8, &edx, 4);
        sig[12] = '\0';

        strncpy(host_hypervisor.signature, sig, sizeof(host_hypervisor.signature) - 1);

        if (strcmp(sig, "KVMKVMKVM\0\0\0") == 0 || strstr(sig, "KVM") != NULL) {
            host_hypervisor.type = HYPERVISOR_KVM;
            strcpy(host_hypervisor.name, "Linux KVM / QEMU");
        } else if (strcmp(sig, "TCGTCGTCGTCG") == 0) {
            host_hypervisor.type = HYPERVISOR_QEMU;
            strcpy(host_hypervisor.name, "QEMU TCG Engine");
        } else if (strcmp(sig, "VMwareVMware") == 0) {
            host_hypervisor.type = HYPERVISOR_VMWARE;
            strcpy(host_hypervisor.name, "VMware ESXi / Workstation");
        } else if (strcmp(sig, "VBoxVBoxVBox") == 0) {
            host_hypervisor.type = HYPERVISOR_VBOX;
            strcpy(host_hypervisor.name, "Oracle VirtualBox");
        } else if (strcmp(sig, "Microsoft Hv") == 0) {
            host_hypervisor.type = HYPERVISOR_HYPERV;
            strcpy(host_hypervisor.name, "Microsoft Hyper-V");
        } else {
            host_hypervisor.type = HYPERVISOR_UNKNOWN;
            strcpy(host_hypervisor.name, "Generic Hypervisor");
        }

        printk(KERN_INFO "VIRT: Detected Hypervisor: %s (Signature: '%s')\n",
               host_hypervisor.name, host_hypervisor.signature);
    } else {
        host_hypervisor.type = HYPERVISOR_NONE;
        strcpy(host_hypervisor.name, "Bare Metal x86_64 Machine");
        printk(KERN_INFO "VIRT: Running on physical Bare Metal x86_64 hardware\n");
    }

    virtio_init();
}

const hypervisor_info_t* virt_get_hypervisor_info(void) {
    return &host_hypervisor;
}

bool virt_is_virtualized(void) {
    return host_hypervisor.type != HYPERVISOR_NONE;
}
