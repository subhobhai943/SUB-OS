#include <sound/tts.h>
#include <drivers/speaker.h>
#include <arch/x86_64/pit.h>
#include <kernel/printk.h>

static const tts_phoneme_t phoneme_table[] = {
    {{"a"}, 730,  1090, 120},
    {{"b"}, 400,  800,  80},
    {{"c"}, 600,  1400, 90},
    {{"d"}, 450,  1600, 80},
    {{"e"}, 530,  1840, 110},
    {{"f"}, 300,  1200, 70},
    {{"g"}, 420,  1300, 80},
    {{"h"}, 350,  900,  60},
    {{"i"}, 270,  2290, 100},
    {{"j"}, 500,  1700, 90},
    {{"k"}, 650,  1500, 80},
    {{"l"}, 400,  1100, 90},
    {{"m"}, 280,  900,  90},
    {{"n"}, 300,  1400, 90},
    {{"o"}, 570,  840,  120},
    {{"p"}, 350,  700,  80},
    {{"q"}, 600,  1300, 90},
    {{"r"}, 450,  1200, 90},
    {{"s"}, 700,  2000, 80},
    {{"t"}, 500,  1800, 70},
    {{"u"}, 440,  1020, 110},
    {{"v"}, 350,  1100, 80},
    {{"w"}, 320,  800,  90},
    {{"x"}, 650,  1900, 80},
    {{"y"}, 300,  2100, 100},
    {{"z"}, 500,  1600, 80},
    {{" "}, 0,    0,    60},
    {{0},   0,    0,    0}
};

void tts_init(void) {
    // Initialized
}

void tts_speak_letter(char c) {
    if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));

    if (c == ' ' || c == ',' || c == '.' || c == '!') {
        pit_sleep(80);
        return;
    }

    for (int i = 0; phoneme_table[i].phoneme[0] != 0; i++) {
        if (phoneme_table[i].phoneme[0] == c) {
            uint32_t freq = phoneme_table[i].freq_f1;
            uint32_t dur = phoneme_table[i].duration_ms;
            if (freq > 0) {
                speaker_beep(freq, dur);
            }
            return;
        }
    }
}

void tts_speak(const char* text) {
    if (!text) return;
    while (*text) {
        tts_speak_letter(*text);
        text++;
    }
}
