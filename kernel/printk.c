#include <kernel/printk.h>
#include <kernel/sched.h>
#include <drivers/tty.h>
#include <drivers/fbcon.h>
#include <drivers/serial.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <kernel/sync.h>

#define DMESG_BUFFER_SIZE 65536 // 64 KB Ring Buffer

static char dmesg_buffer[DMESG_BUFFER_SIZE];
static size_t dmesg_head = 0;
static size_t dmesg_total = 0;
static spinlock_t printk_lock = SPINLOCK_INIT;

// Console redirection state. `sink_active` guards against a sink that calls
// printk itself, which would otherwise recurse until the stack ran out.
static printk_sink_fn_t sink_fn = NULL;
static void*            sink_ctx = NULL;
static bool             sink_active = false;

void printk_set_sink(printk_sink_fn_t fn, void* ctx) {
    sink_fn  = fn;
    sink_ctx = ctx;
}

void printk_clear_sink(void) {
    sink_fn  = NULL;
    sink_ctx = NULL;
}

bool printk_has_sink(void) {
    return sink_fn != NULL;
}

void printk_init(void) {
    memset(dmesg_buffer, 0, sizeof(dmesg_buffer));
    dmesg_head = 0;
    dmesg_total = 0;
}

static void dmesg_append(const char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dmesg_buffer[dmesg_head] = str[i];
        dmesg_head = (dmesg_head + 1) % DMESG_BUFFER_SIZE;
        if (dmesg_total < DMESG_BUFFER_SIZE) {
            dmesg_total++;
        }
    }
}

/*
 * Put raw bytes on every console printk uses.
 *
 * Kept here rather than at the call sites because the routing is not obvious:
 * once the display adapter is in graphics mode the VGA text buffer is no
 * longer scanned out, so the framebuffer console has to take over, and the
 * serial port is a separate sink that has to be fed either way. Userland
 * writes to stdout/stderr land here so they behave exactly like kernel output.
 */
void console_write(const char* data, size_t len) {
    if (!data || len == 0) return;

    if (fbcon_is_active()) {
        fbcon_write(data, len);
    } else {
        tty_write(data, len);
    }

#if defined(__aarch64__) || defined(__arm__) || defined(__armv8i__)
    extern void uart_pl011_putc(char c);
    for (size_t i = 0; i < len; i++) {
        uart_pl011_putc(data[i]);
    }
#else
    for (size_t i = 0; i < len; i++) {
        serial_write_char(data[i]);
    }
#endif
}

int vprintk(const char* fmt, va_list args) {
    char buffer[1024];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    if (len <= 0) return 0;

    sched_preempt_disable();
    spin_lock(&printk_lock);

    // Skip loglevel prefix if present (e.g. "\0016")
    const char* display_ptr = buffer;
    if (buffer[0] == '\001' && buffer[1] >= '0' && buffer[1] <= '7') {
        display_ptr += 2;
        len -= 2;
    }

    // 1. Output to the active TTY console, or to the installed sink instead.
    if (sink_fn && !sink_active) {
        sink_active = true;
        sink_fn(display_ptr, (size_t)len, sink_ctx);
        sink_active = false;
    } else if (!sink_fn) {
        // The VGA text buffer stops being scanned out once the adapter is in
        // graphics mode, so the framebuffer console takes over there.
        if (fbcon_is_active()) {
            fbcon_write(display_ptr, (size_t)len);
        } else {
            tty_write(display_ptr, (size_t)len);
        }
    }

    // 2. Output to serial console (COM1 on x86, PL011 on ARM/AArch64)
#if defined(__aarch64__) || defined(__arm__) || defined(__armv8i__)
    extern void uart_pl011_putc(char c);
    for (int i = 0; i < len; i++) {
        uart_pl011_putc(display_ptr[i]);
    }
#else
    for (int i = 0; i < len; i++) {
        serial_write_char(display_ptr[i]);
    }
#endif

    // 3. Store in kernel dmesg ring buffer
    dmesg_append(display_ptr, (size_t)len);

    spin_unlock(&printk_lock);
    sched_preempt_enable();
    return len;
}

int printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintk(fmt, args);
    va_end(args);
    return ret;
}

int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintk(fmt, args);
    va_end(args);
    return ret;
}

void dmesg_dump(void) {
    sched_preempt_disable();
    spin_lock(&printk_lock);
    if (dmesg_total < DMESG_BUFFER_SIZE) {
        tty_write(dmesg_buffer, dmesg_total);
    } else {
        // Output from head to end, then 0 to head
        tty_write(dmesg_buffer + dmesg_head, DMESG_BUFFER_SIZE - dmesg_head);
        tty_write(dmesg_buffer, dmesg_head);
    }
    spin_unlock(&printk_lock);
    sched_preempt_enable();
}

const char* dmesg_get_buffer(size_t* size_out) {
    if (size_out) *size_out = dmesg_total;
    return dmesg_buffer;
}
