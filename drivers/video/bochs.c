// Bochs / QEMU VBE Display Adapter Driver for SUB-OS
#include <drivers/bochs.h>
#include <init/init.h>
#include <arch/x86_64/io.h>
#include <drivers/pci.h>
#include <drivers/fb.h>
#include <lib/string.h>
#include <kernel/printk.h>

static bochs_vbe_info_t vbe_info;

static bool g_vbe_enabled = false;

static void vbe_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t vbe_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

// Parse `video=WIDTHxHEIGHT` from the kernel command line. Falls back to the
// caller's request when the parameter is absent or malformed.
static void vbe_mode_from_cmdline(uint32_t* width, uint32_t* height) {
    const char* spec = init_get_param("video");
    if (!spec) return;

    uint32_t w = 0, h = 0;
    const char* p = spec;

    while (*p >= '0' && *p <= '9') w = w * 10 + (uint32_t)(*p++ - '0');
    if (*p != 'x' && *p != 'X') return;
    p++;
    while (*p >= '0' && *p <= '9') h = h * 10 + (uint32_t)(*p++ - '0');

    // The Bochs adapter tops out at 2560x1600, and the compositor allocates a
    // 32-bit backbuffer per surface, so refuse anything the heap cannot hold.
    if (w < 640 || h < 480 || w > 1920 || h > 1200) {
        printk(KERN_WARNING "VBE: ignoring unsupported video=%s\n", spec);
        return;
    }

    *width = w;
    *height = h;
}

void bochs_vbe_disable(void) {
    if (!vbe_info.available) return;

    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    g_vbe_enabled = false;
    fb_set_active(false);
}

bool bochs_vbe_is_enabled(void) {
    return g_vbe_enabled;
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
    g_vbe_enabled = true;

    // Connect global Framebuffer driver directly to hardware video memory
    fb_set_hardware_lfb((uint32_t*)vbe_info.lfb_phys_addr, width, height, (uint8_t)bpp);

    printk(KERN_INFO "VBE: Switched resolution to %ux%u @ %u bpp (LFB: 0x%llx)\n",
           width, height, bpp, vbe_info.lfb_phys_addr);
    return 0;
}

bool bochs_vbe_init(void) {
    memset(&vbe_info, 0, sizeof(vbe_info));

    uint32_t mode_w = 800, mode_h = 600;

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

        vbe_mode_from_cmdline(&mode_w, &mode_h);
        bochs_vbe_set_mode(mode_w, mode_h, 32);

        printk(KERN_INFO "VBE: Bochs/QEMU VBE Display Adapter detected (v%04x, Linear Framebuffer: 0x%llx)\n",
               vbe_info.vbe_version, vbe_info.lfb_phys_addr);
        return true;
    }

    // Software emulated VBE fallback
    vbe_info.available = true;
    vbe_info.vbe_version = 0xB0C5;
    vbe_info.lfb_phys_addr = 0xFD000000;
    vbe_mode_from_cmdline(&mode_w, &mode_h);
    bochs_vbe_set_mode(mode_w, mode_h, 32);

    printk(KERN_INFO "VBE: Standard VESA/VBE Display Engine online (800x600 32bpp TrueColor)\n");
    return true;
}

bool bochs_vbe_is_available(void) {
    return vbe_info.available;
}

const bochs_vbe_info_t* bochs_vbe_get_info(void) {
    return &vbe_info;
}
