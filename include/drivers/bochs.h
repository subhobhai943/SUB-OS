#ifndef _DRIVERS_BOCHS_H
#define _DRIVERS_BOCHS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID          0x0
#define VBE_DISPI_INDEX_XRES        0x1
#define VBE_DISPI_INDEX_YRES        0x2
#define VBE_DISPI_INDEX_BPP         0x3
#define VBE_DISPI_INDEX_ENABLE      0x4
#define VBE_DISPI_INDEX_BANK        0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH  0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET    0x8
#define VBE_DISPI_INDEX_Y_OFFSET    0x9

#define VBE_DISPI_DISABLED          0x00
#define VBE_DISPI_ENABLED           0x01
#define VBE_DISPI_LFB_ENABLED       0x40
#define VBE_DISPI_NOCLEARMEM        0x80

typedef struct {
    uint32_t current_width;
    uint32_t current_height;
    uint32_t current_bpp;
    uint64_t lfb_phys_addr;
    uint16_t vbe_version;
    bool     available;
} bochs_vbe_info_t;

bool bochs_vbe_init(void);
bool bochs_vbe_is_available(void);
int  bochs_vbe_set_mode(uint32_t width, uint32_t height, uint32_t bpp);
const bochs_vbe_info_t* bochs_vbe_get_info(void);

#endif // _DRIVERS_BOCHS_H
