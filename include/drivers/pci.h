#ifndef _DRIVERS_PCI_H
#define _DRIVERS_PCI_H

#include <stdint.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_CLASS_NETWORK 0x02
#define PCI_CLASS_DISPLAY 0x03
#define PCI_CLASS_STORAGE 0x01

#define PCI_SUBCLASS_ETHERNET 0x00
#define PCI_SUBCLASS_IDE      0x01
#define PCI_SUBCLASS_SATA     0x06

typedef struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass_id;
    uint8_t  prog_if;
    uint8_t  revision;
    uint8_t  irq;
    uint32_t bar[6];
    uint8_t  bar_type[6];
    struct pci_device* next;
} pci_device_t;

void pci_init(void);
pci_device_t* pci_get_devices(void);
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pci_find_class(uint8_t class_id, uint8_t subclass_id);
void pci_enable_bus_mastering(pci_device_t* dev);

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void     pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

#endif // _DRIVERS_PCI_H
