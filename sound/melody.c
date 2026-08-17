#include <sound/melody.h>
#include <drivers/speaker.h>
#include <arch/x86_64/pit.h>

void melody_init(void) {
    // Initialized
}

void melody_play_custom(const note_t* notes, uint32_t count) {
    if (!notes) return;
    for (uint32_t i = 0; i < count; i++) {
        if (notes[i].freq > 0) {
            speaker_beep(notes[i].freq, notes[i].duration_ms);
        } else {
            pit_sleep(notes[i].duration_ms);
        }
        pit_sleep(20); // Small pause between notes
    }
}

void melody_play_startup(void) {
    note_t fanfare[] = {
        {523, 100}, // C5
        {659, 100}, // E5
        {784, 120}, // G5
        {1046, 250} // C6
    };
    melody_play_custom(fanfare, sizeof(fanfare) / sizeof(note_t));
}

void melody_play_tetris(void) {
    note_t tetris[] = {
        {659, 200}, {494, 100}, {523, 100}, {587, 200}, {523, 100}, {494, 100},
        {440, 200}, {440, 100}, {523, 100}, {659, 200}, {587, 100}, {523, 100},
        {494, 300}, {523, 100}, {587, 200}, {659, 200}, {523, 200}, {440, 200},
        {440, 300}
    };
    melody_play_custom(tetris, sizeof(tetris) / sizeof(note_t));
}

void melody_play_mario(void) {
    note_t mario[] = {
        {659, 100}, {659, 100}, {0, 100}, {659, 100}, {0, 100},
        {523, 100}, {659, 150}, {784, 250}, {0, 150}, {392, 250}
    };
    melody_play_custom(mario, sizeof(mario) / sizeof(note_t));
}
