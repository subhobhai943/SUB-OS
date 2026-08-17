#ifndef _SOUND_MELODY_H
#define _SOUND_MELODY_H

#include <stdint.h>

typedef struct {
    uint32_t freq;
    uint32_t duration_ms;
} note_t;

void melody_init(void);
void melody_play_startup(void);
void melody_play_tetris(void);
void melody_play_mario(void);
void melody_play_custom(const note_t* notes, uint32_t count);

#endif // _SOUND_MELODY_H
