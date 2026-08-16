#include <sound/pcm.h>
#include <sound/sound.h>
#include <lib/string.h>

static pcm_stream_t default_pcm;

void pcm_init(void) {
    memset(&default_pcm, 0, sizeof(default_pcm));
    default_pcm.sample_rate = 44100;
    default_pcm.channels = 2;
    default_pcm.bits = 16;
}

int pcm_write(const void* data, size_t len) {
    sound_device_t* dev = sound_get_default_device();
    if (dev && dev->play_sample) {
        return dev->play_sample(data, len);
    }
    return 0;
}
