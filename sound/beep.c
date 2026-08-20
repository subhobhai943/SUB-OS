// Audio Beep & Tone Synthesizer for SUB-OS
#include <sound/beep.h>
#include <drivers/speaker.h>
#include <arch/x86_64/pit.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

void sound_play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0 || duration_ms == 0) return;
    #if defined(__x86_64__)
    speaker_beep(freq_hz, duration_ms);
    #else
    printk(ANSI_BRIGHT_MAGENTA "♪ [Tone %u Hz for %u ms]\n" ANSI_RESET, freq_hz, duration_ms);
    #endif
}

void sound_play_jingle(const char* jingle_name) {
    if (!jingle_name) return;

    if (strcmp(jingle_name, "startup") == 0) {
        sound_play_tone(523, 60); // C5
        sound_play_tone(659, 60); // E5
        sound_play_tone(784, 60); // G5
        sound_play_tone(1046, 120);// C6
    } else if (strcmp(jingle_name, "coin") == 0) {
        sound_play_tone(988, 40); // B5
        sound_play_tone(1318, 100);// E6
    } else if (strcmp(jingle_name, "alert") == 0) {
        sound_play_tone(880, 80);
        sound_play_tone(440, 80);
    } else {
        sound_play_tone(440, 100);
    }
}

int applet_beep(int argc, char** argv) {
    uint32_t freq = 440;
    uint32_t dur = 100;

    if (argc >= 2) {
        if (strcmp(argv[1], "startup") == 0 || strcmp(argv[1], "coin") == 0 || strcmp(argv[1], "alert") == 0) {
            printk(ANSI_BRIGHT_CYAN "Playing jingle '%s'...\n" ANSI_RESET, argv[1]);
            sound_play_jingle(argv[1]);
            return 0;
        }
        freq = (uint32_t)strtol(argv[1], NULL, 10);
    }
    if (argc >= 3) {
        dur = (uint32_t)strtol(argv[2], NULL, 10);
    }

    if (freq < 20) freq = 20;
    if (freq > 20000) freq = 20000;
    if (dur > 5000) dur = 5000;

    printk(ANSI_BRIGHT_CYAN "Beeping at %u Hz for %u ms...\n" ANSI_RESET, freq, dur);
    sound_play_tone(freq, dur);
    return 0;
}
