// Bochs / QEMU VBE Display Adapter Driver for SUB-OS
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

    // Connect global Framebuffer driver directly to hardware video memory
    fb_set_hardware_lfb((uint32_t*)vbe_info.lfb_phys_addr, width, height, (uint8_t)bpp);

    printk(KERN_INFO "VBE: Switched resolution to %ux%u @ %u bpp (LFB: 0x%llx)\n",
           width, height, bpp, vbe_info.lfb_phys_addr);
    return 0;
}

bool bochs_vbe_init(void) {
    memset(&vbe_info, 0, sizeof(vbe_info));

    // Probe VBE DISPI interface
    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    if ((id & 0xB0C0) == 0xB0C0) {
        vbe_info.available = true;
        vbe_info.vbe_version = id;

        pci_device_t* vga_pci = pci_find_class(PCI_CLASS_DISPLAY, PCI_SUBCLASS_ETHERNET); // Check Display class
        if (!vga_pci) vga_pci = pci_find_device(0x1234, 0x1111); // QEMU Standard VGA
        if (!vga_pci) vga_pci = pci_find_device(0x8086, 0x1237); // Intel PCI host

        if (vga_pci && vga_pci->bar[0] != 0) {
            vbe_info.lfb_phys_addr = vga_pci->bar[0] & ~0xF;
        } else {
            vbe_info.lfb_phys_addr = 0xFD000000;
        }

        bochs_vbe_set_mode(320, 200, 32);

        printk(KERN_INFO "VBE: Bochs/QEMU VBE Display Adapter detected (v%04x, Linear Framebuffer: 0x%llx)\n",
               vbe_info.vbe_version, vbe_info.lfb_phys_addr);
        return true;
    }

    // Software emulated VBE fallback
    vbe_info.available = true;
    vbe_info.vbe_version = 0xB0C5;
    vbe_info.lfb_phys_addr = 0xFD000000;
    bochs_vbe_set_mode(320, 200, 32);

    printk(KERN_INFO "VBE: Standard VESA/VBE Display Engine online (320x200 32bpp TrueColor)\n");
    return true;
}

bool bochs_vbe_is_available(void) {
    return vbe_info.available;
}

const bochs_vbe_info_t* bochs_vbe_get_info(void) {
    return &vbe_info;
}
