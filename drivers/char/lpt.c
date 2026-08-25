// IBM PC parallel port (LPT1) printer driver for SUB-OS.
//
// Implements the classic Centronics handshake: place a byte on the data lines,
// wait for the peripheral to be ready (nBUSY high), then pulse the STROBE line.

#include <drivers/lpt.h>
#include <kernel/printk.h>

#if defined(__x86_64__)
#include <arch/x86_64/io.h>

#define LPT1_DATA    0x378
#define LPT1_STATUS  0x379   // bit7 nBUSY (1 = ready), bit6 nACK, bit5 PAPER, bit4 SELECT
#define LPT1_CONTROL 0x37A   // bit0 STROBE, bit1 AUTOLF, bit2 nINIT, bit3 SELECT

#define LPT_STATUS_BUSY  0x80
#define LPT_CTRL_STROBE  0x01
#define LPT_CTRL_INIT    0x04
#define LPT_CTRL_SELECT  0x08

static bool     g_present = false;
static uint64_t g_bytes_written = 0;

void lpt_init(void) {
    uint8_t status = inb(LPT1_STATUS);
    // A floating (unimplemented) port typically reads back all-ones.
    if (status == 0xFF) {
        g_present = false;
        printk(KERN_INFO "LPT: no parallel port at 0x%03X\n", LPT1_DATA);
        return;
    }
    // Bring the port out of reset and select the printer.
    outb(LPT1_CONTROL, LPT_CTRL_INIT | LPT_CTRL_SELECT);
    g_present = true;
    printk(ANSI_BRIGHT_GREEN "LPT: " ANSI_RESET
           "Parallel port LPT1 online at 0x%03X (status 0x%02X)\n", LPT1_DATA, status);
}

bool lpt_present(void) { return g_present; }
uint8_t lpt_status(void) { return g_present ? inb(LPT1_STATUS) : 0xFF; }

int lpt_putc(char c) {
    if (!g_present) return -1;

    // Wait (bounded) for the peripheral to raise nBUSY.
    int spins = 100000;
    while (spins-- > 0 && !(inb(LPT1_STATUS) & LPT_STATUS_BUSY)) {
        /* busy-wait */
    }

    outb(LPT1_DATA, (uint8_t)c);

    // Pulse STROBE low->high to latch the byte.
    uint8_t ctrl = inb(LPT1_CONTROL);
    outb(LPT1_CONTROL, ctrl | LPT_CTRL_STROBE);
    outb(LPT1_CONTROL, ctrl & ~LPT_CTRL_STROBE);

    g_bytes_written++;
    return 0;
}

int lpt_write(const char* buf, size_t len) {
    if (!g_present || !buf) return -1;
    int n = 0;
    for (size_t i = 0; i < len; i++) {
        if (lpt_putc(buf[i]) != 0) break;
        n++;
    }
    return n;
}

#else  // Parallel port is an x86 platform device.

void    lpt_init(void)                          { }
bool    lpt_present(void)                        { return false; }
int     lpt_putc(char c)                         { (void)c; return -1; }
int     lpt_write(const char* buf, size_t len)   { (void)buf; (void)len; return -1; }
uint8_t lpt_status(void)                         { return 0xFF; }

#endif
