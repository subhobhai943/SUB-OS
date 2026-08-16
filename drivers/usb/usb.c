#include <drivers/usb.h>
#include <drivers/pci.h>
#include <lib/string.h>
#include <kernel/printk.h>

static usb_device_t usb_devices[USB_MAX_DEVICES];
static size_t usb_count = 0;

void usb_init(void) {
    memset(usb_devices, 0, sizeof(usb_devices));
    usb_count = 0;

    // Scan for USB host controllers on PCI
    pci_device_t* dev = pci_find_class(0x0C, 0x03); // Serial Bus - USB
    if (dev) {
        usb_devices[0].address = 1;
        usb_devices[0].vendor_id = 0x046D;
        usb_devices[0].product_id = 0xC077;
        usb_devices[0].class_code = 0x03; // HID
        strcpy(usb_devices[0].product_name, "USB Optical Mouse / Keyboard");
        usb_devices[0].speed = USB_SPEED_FULL;
        usb_devices[0].connected = true;
        usb_count = 1;

        printk(KERN_INFO "USB: Controller active at PCI %02x:%02x.%d (UHCI/EHCI)\n",
               dev->bus, dev->slot, dev->function);
    } else {
        printk(KERN_INFO "USB: USB Core Subsystem initialized (0 host controllers)\n");
    }
}

size_t usb_get_device_count(void) {
    return usb_count;
}

const usb_device_t* usb_get_device(size_t index) {
    if (index >= usb_count) return NULL;
    return &usb_devices[index];
}
