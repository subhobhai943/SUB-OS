#ifndef _SOUND_PCM_H
#define _SOUND_PCM_H

#include <stdint.h>
#include <stddef.h>

#define PCM_BUFFER_SIZE 8192

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits;
    uint8_t buffer[PCM_BUFFER_SIZE];
    size_t length;
} pcm_stream_t;

void pcm_init(void);
int pcm_write(const void* data, size_t len);

#endif // _SOUND_PCM_H
