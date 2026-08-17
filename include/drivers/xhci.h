#ifndef _DRIVERS_XHCI_H
#define _DRIVERS_XHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define XHCI_PCI_CLASS    0x0C
#define XHCI_PCI_SUBCLASS 0x03
#define XHCI_PCI_PROGIF   0x30

typedef struct {
    uint8_t  cap_length;
    uint16_t hci_version;
    uint32_t max_slots;
    uint32_t max_ports;
    uint32_t max_interrupters;
    bool     initialized;
} xhci_controller_info_t;

bool xhci_init(void);
bool xhci_is_detected(void);
const xhci_controller_info_t* xhci_get_info(void);

#endif // _DRIVERS_XHCI_H
