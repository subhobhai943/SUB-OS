#ifndef _DRIVERS_USB_H
#define _DRIVERS_USB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define USB_MAX_DEVICES 8

typedef enum {
    USB_SPEED_LOW = 0,
    USB_SPEED_FULL = 1,
    USB_SPEED_HIGH = 2
} usb_speed_t;

typedef struct usb_device {
    uint8_t address;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t class_code;
    char product_name[32];
    usb_speed_t speed;
    bool connected;
} usb_device_t;

void usb_init(void);
size_t usb_get_device_count(void);
const usb_device_t* usb_get_device(size_t index);

#endif // _DRIVERS_USB_H
