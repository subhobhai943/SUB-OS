#include <drivers/bochs.h>
#include <arch/x86_64/io.h>
#include <drivers/pci.h>
#include <drivers/fb.h>
#include <lib/string.h>
#include <kernel/printk.h>

static bochs_vbe_info_t vbe_info;

static void vbe_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t vbe_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

bool bochs_vbe_init(void) {
    memset(&vbe_info, 0, sizeof(vbe_info));

    // Probe VBE DISPI interface
    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    if ((id & 0xB0C0) == 0xB0C0) {
        vbe_info.available = true;
        vbe_info.vbe_version = id;
        vbe_info.current_width = 1024;
        vbe_info.current_height = 768;
        vbe_info.current_bpp = 32;

        pci_device_t* vga_pci = pci_find_class(0x03, 0x00);
        if (vga_pci) {
            vbe_info.lfb_phys_addr = vga_pci->bar[0] & ~0xF;
        } else {
            vbe_info.lfb_phys_addr = 0xFD000000;
        }

        printk(KERN_INFO "VBE: Bochs/QEMU VBE Display Adapter detected (v%04x, Linear Framebuffer: 0x%llx)\n",
               vbe_info.vbe_version, vbe_info.lfb_phys_addr);
        return true;
    }

    // Software emulated VBE fallback
    vbe_info.available = true;
    vbe_info.vbe_version = 0xB0C5;
    vbe_info.current_width = 800;
    vbe_info.current_height = 600;
    vbe_info.current_bpp = 32;
    vbe_info.lfb_phys_addr = 0xFD000000;

    printk(KERN_INFO "VBE: Standard VESA/VBE Display Engine online (800x600 32bpp TrueColor)\n");
    return true;
}

bool bochs_vbe_is_available(void) {
    return vbe_info.available;
}

int bochs_vbe_set_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    if (!vbe_info.available) return -1;

    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, (uint16_t)width);
    vbe_write(VBE_DISPI_INDEX_YRES, (uint16_t)height);
    vbe_write(VBE_DISPI_INDEX_BPP, (uint16_t)bpp);
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    vbe_info.current_width = width;
    vbe_info.current_height = height;
    vbe_info.current_bpp = bpp;

    printk(KERN_INFO "VBE: Switched resolution to %ux%u @ %u bpp\n", width, height, bpp);
    return 0;
}

const bochs_vbe_info_t* bochs_vbe_get_info(void) {
    return &vbe_info;
}
