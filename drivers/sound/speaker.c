#include <drivers/speaker.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/io.h>

void speaker_on(uint32_t frequency) {
    if (frequency == 0) return;
    uint32_t divisor = 1193180 / frequency;

    // Set PIT Channel 2
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    // Enable speaker via Port 0x61
    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void speaker_off(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_on(frequency);
    pit_sleep(duration_ms);
    speaker_off();
}
