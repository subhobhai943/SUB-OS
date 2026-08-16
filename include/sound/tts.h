#ifndef _SOUND_TTS_H
#define _SOUND_TTS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char phoneme[4];
    uint32_t freq_f1;
    uint32_t freq_f2;
    uint32_t duration_ms;
} tts_phoneme_t;

void tts_init(void);
void tts_speak(const char* text);
void tts_speak_letter(char c);

#endif // _SOUND_TTS_H
