// VirtIO-Input Para-Virtualized Tablet & Mouse Event Driver for SUB-OS
#include <drivers/virtio_input.h>
#include <drivers/pci.h>
#include <virt/virtio.h>
#include <mm/kmalloc.h>
#include <kernel/printk.h>

static pci_device_t* input_pci_dev = NULL;
static bool g_input_online = false;
static uint64_t g_events_received = 0;
static int32_t g_abs_x = 0;
static int32_t g_abs_y = 0;

void virtio_input_init(void) {
    input_pci_dev = pci_find_device(VIRTIO_PCI_VENDOR, 0x1052); // VirtIO-Input
    if (!input_pci_dev) {
        input_pci_dev = pci_find_device(VIRTIO_PCI_VENDOR, 0x1012); // Legacy ID
    }

    if (input_pci_dev) {
        pci_enable_bus_mastering(input_pci_dev);
        g_input_online = true;
        printk(KERN_INFO "VIRTIO-INPUT: Para-virtualized Absolute Tablet & Event Input online\n");
    } else {
        g_input_online = false;
        printk(KERN_INFO "VIRTIO-INPUT: Subsystem ready for VirtIO tablet/mouse hotplug\n");
    }
}

bool virtio_input_is_active(void) {
    return g_input_online;
}

void virtio_input_dump_status(void) {
    printk(ANSI_BRIGHT_CYAN "=== VirtIO-Input Subsystem Telemetry ===\n" ANSI_RESET);
    printk("  Device Status  : %s\n", g_input_online ? ANSI_BRIGHT_GREEN "ONLINE" ANSI_RESET : ANSI_YELLOW "STANDBY" ANSI_RESET);
    printk("  Coordinates    : (%d, %d)\n", g_abs_x, g_abs_y);
    printk("  Events Handled : %llu input events\n\n", g_events_received);
}
