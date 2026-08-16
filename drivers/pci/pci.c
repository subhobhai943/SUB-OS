#include <drivers/pci.h>
#include <arch/x86_64/io.h>
#include <mm/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>

static pci_device_t* pci_device_list = NULL;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)slot << 11) |
                                  ((uint32_t)func << 8) |
                                  (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_config_read32(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_config_read32(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((1U << 31) |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)slot << 11) |
                                  ((uint32_t)func << 8) |
                                  (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t old = pci_config_read32(bus, slot, func, offset);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t new_val = (old & ~mask) | ((uint32_t)val << shift);
    pci_config_write32(bus, slot, func, offset, new_val);
}

void pci_enable_bus_mastering(pci_device_t* dev) {
    if (!dev) return;
    uint16_t cmd = pci_config_read16(dev->bus, dev->slot, dev->function, 0x04);
    cmd |= (1 << 2) | (1 << 0) | (1 << 1); // Bus Master, I/O Space, Memory Space
    pci_config_write16(dev->bus, dev->slot, dev->function, 0x04, cmd);
}

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_config_read16(bus, slot, func, 0x00);
    if (vendor == 0xFFFF || vendor == 0x0000) return;

    uint16_t device = pci_config_read16(bus, slot, func, 0x02);
    uint8_t class_id = pci_config_read8(bus, slot, func, 0x0B);
    uint8_t subclass_id = pci_config_read8(bus, slot, func, 0x0A);
    uint8_t prog_if = pci_config_read8(bus, slot, func, 0x09);
    uint8_t revision = pci_config_read8(bus, slot, func, 0x08);
    uint8_t irq = pci_config_read8(bus, slot, func, 0x3C);

    pci_device_t* dev = (pci_device_t*)kmalloc(sizeof(pci_device_t));
    if (!dev) return;

    dev->bus = bus;
    dev->slot = slot;
    dev->function = func;
    dev->vendor_id = vendor;
    dev->device_id = device;
    dev->class_id = class_id;
    dev->subclass_id = subclass_id;
    dev->prog_if = prog_if;
    dev->revision = revision;
    dev->irq = irq;

    // Read BARs 0-5
    for (int b = 0; b < 6; b++) {
        uint32_t bar = pci_config_read32(bus, slot, func, 0x10 + b * 4);
        dev->bar[b] = bar;
        dev->bar_type[b] = (bar & 1); // 0 = Memory, 1 = I/O
    }

    dev->next = pci_device_list;
    pci_device_list = dev;

    printk(KERN_INFO "PCI: %02x:%02x.%d [%04x:%04x] Class %02x.%02x (IRQ %d)\n",
           bus, slot, func, vendor, device, class_id, subclass_id, irq);
}

void pci_init(void) {
    pci_device_list = NULL;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_config_read16((uint8_t)bus, slot, 0, 0x00);
            if (vendor == 0xFFFF) continue;

            pci_check_function((uint8_t)bus, slot, 0);

            uint8_t header_type = pci_config_read8((uint8_t)bus, slot, 0, 0x0E);
            if (header_type & 0x80) {
                // Multi-function device
                for (uint8_t func = 1; func < 8; func++) {
                    pci_check_function((uint8_t)bus, slot, func);
                }
            }
        }
    }
}

pci_device_t* pci_get_devices(void) {
    return pci_device_list;
}

pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    pci_device_t* curr = pci_device_list;
    while (curr) {
        if (curr->vendor_id == vendor_id && curr->device_id == device_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

pci_device_t* pci_find_class(uint8_t class_id, uint8_t subclass_id) {
    pci_device_t* curr = pci_device_list;
    while (curr) {
        if (curr->class_id == class_id && curr->subclass_id == subclass_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}
