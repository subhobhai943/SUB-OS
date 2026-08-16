#ifndef _SOUND_AC97_H
#define _SOUND_AC97_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AC97_PCI_VENDOR 0x8086
#define AC97_PCI_DEVICE 0x2415 // Intel 82801AA AC'97 Audio

bool ac97_init(void);
void ac97_set_master_volume(uint8_t left, uint8_t right);
int  ac97_play_pcm(const void* samples, size_t length);

#endif // _SOUND_AC97_H
