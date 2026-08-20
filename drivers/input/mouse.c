// PS/2 Mouse Controller & Interrupt Driver for SUB-OS
#include <drivers/mouse.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <kernel/printk.h>

#define MOUSE_PORT_DATA   0x60
#define MOUSE_PORT_STATUS 0x64
#define MOUSE_PORT_CMD    0x64

#define MOUSE_CMD_ENABLE_AUX   0xA8
#define MOUSE_CMD_GET_BYTE     0x20
#define MOUSE_CMD_SET_BYTE     0x60
#define MOUSE_CMD_WRITE_AUX    0xD4

#define MOUSE_CMD_SET_DEFAULTS 0xF6
#define MOUSE_CMD_ENABLE_DATA  0xF4
#define MOUSE_CMD_SET_SAMPLE   0xF3

static mouse_state_t mouse_state = {400, 300, false, false, false};
static int mouse_bounds_x = 800;
static int mouse_bounds_y = 600;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static inline void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while ((inb(MOUSE_PORT_STATUS) & 0x02) && timeout--) {
        io_wait();
    }
}

static inline void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (!(inb(MOUSE_PORT_STATUS) & 0x01) && timeout--) {
        io_wait();
    }
}

static void mouse_write_cmd(uint8_t cmd) {
    mouse_wait_write();
    outb(MOUSE_PORT_CMD, cmd);
}

static void mouse_write_data(uint8_t data) {
    mouse_wait_write();
    outb(MOUSE_PORT_CMD, MOUSE_CMD_WRITE_AUX);
    mouse_wait_write();
    outb(MOUSE_PORT_DATA, data);
}

static uint8_t mouse_read_data(void) {
    mouse_wait_read();
    return inb(MOUSE_PORT_DATA);
}

static void mouse_interrupt_handler(registers_t* regs) {
    (void)regs;
    uint8_t status = inb(MOUSE_PORT_STATUS);
    if (!(status & 0x20)) {
        // Not mouse data
        return;
    }

    uint8_t data = inb(MOUSE_PORT_DATA);

    switch (mouse_cycle) {
        case 0:
            // Byte 0 must have bit 3 set to 1 in standard PS/2 packet
            if ((data & 0x08) == 0x08) {
                mouse_packet[0] = data;
                mouse_cycle = 1;
            }
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_cycle = 0;

            // Process Packet
            uint8_t flags = mouse_packet[0];
            int32_t dx = (int32_t)mouse_packet[1];
            int32_t dy = (int32_t)mouse_packet[2];

            // Sign extension
            if (flags & 0x10) dx |= 0xFFFFFF00;
            if (flags & 0x20) dy |= 0xFFFFFF00;

            // Check overflow
            if (flags & 0xC0) {
                // Discard invalid/overflow packet
                break;
            }

            // Update Coordinates (PS/2 Y delta is inverted)
            mouse_state.x += dx;
            mouse_state.y -= dy;

            // Clamp coordinates to screen bounds
            if (mouse_state.x < 0) mouse_state.x = 0;
            if (mouse_state.y < 0) mouse_state.y = 0;
            if (mouse_state.x >= mouse_bounds_x) mouse_state.x = mouse_bounds_x - 1;
            if (mouse_state.y >= mouse_bounds_y) mouse_state.y = mouse_bounds_y - 1;

            // Update button states
            mouse_state.left_btn = (flags & 0x01) != 0;
            mouse_state.right_btn = (flags & 0x02) != 0;
            mouse_state.middle_btn = (flags & 0x04) != 0;
            break;
    }
}

void mouse_init(void) {
#if defined(__x86_64__)
    // 1. Enable Auxiliary Device
    mouse_write_cmd(MOUSE_CMD_ENABLE_AUX);

    // 2. Read Controller Command Byte
    mouse_write_cmd(MOUSE_CMD_GET_BYTE);
    uint8_t status = mouse_read_data();

    // 3. Enable IRQ12 (Bit 1) & Enable Mouse Clock (Clear Bit 5)
    status |= 0x02;
    status &= ~0x20;
    mouse_write_cmd(MOUSE_CMD_SET_BYTE);
    mouse_wait_write();
    outb(MOUSE_PORT_DATA, status);

    // 4. Set Defaults & Enable Streaming
    mouse_write_data(MOUSE_CMD_SET_DEFAULTS);
    mouse_read_data(); // ACK (0xFA)

    mouse_write_data(MOUSE_CMD_ENABLE_DATA);
    mouse_read_data(); // ACK (0xFA)

    // 5. Register IRQ 12 Handler (Vector 44 = 32 + 12)
    isr_register_handler(44, mouse_interrupt_handler);

    // 6. Unmask PIC IRQ 12
    pic_clear_mask(12);
#endif

    mouse_state.x = mouse_bounds_x / 2;
    mouse_state.y = mouse_bounds_y / 2;
    mouse_state.left_btn = false;
    mouse_state.right_btn = false;
    mouse_state.middle_btn = false;

    printk(KERN_INFO "MOUSE: PS/2 Mouse Controller initialized (IRQ 12, Bounds: %dx%d)\n",
           mouse_bounds_x, mouse_bounds_y);
}

const mouse_state_t* mouse_get_state(void) {
    return &mouse_state;
}

void mouse_set_bounds(int max_x, int max_y) {
    mouse_bounds_x = (max_x > 0) ? max_x : 800;
    mouse_bounds_y = (max_y > 0) ? max_y : 600;
    if (mouse_state.x >= mouse_bounds_x) mouse_state.x = mouse_bounds_x / 2;
    if (mouse_state.y >= mouse_bounds_y) mouse_state.y = mouse_bounds_y / 2;
}
