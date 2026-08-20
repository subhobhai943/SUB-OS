#ifndef _SOUND_BEEP_H
#define _SOUND_BEEP_H

#include <stdint.h>
#include <stddef.h>

void sound_play_tone(uint32_t freq_hz, uint32_t duration_ms);
void sound_play_jingle(const char* jingle_name);
int applet_beep(int argc, char** argv);

#endif // _SOUND_BEEP_H
