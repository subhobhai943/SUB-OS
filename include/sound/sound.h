#ifndef _SOUND_SOUND_H
#define _SOUND_SOUND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct sound_device {
    char name[32];
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    int volume;
    int (*play_sample)(const void* buffer, size_t length);
    int (*set_volume)(int volume);
    bool active;
} sound_device_t;

void sound_init(void);
int sound_register_device(sound_device_t* dev);
sound_device_t* sound_get_default_device(void);

#endif // _SOUND_SOUND_H
