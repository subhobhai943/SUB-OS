#include <sound/sound.h>
#include <sound/pcm.h>
#include <sound/ac97.h>
#include <sound/tts.h>
#include <lib/string.h>
#include <kernel/printk.h>

static sound_device_t* default_sound_dev = NULL;

void sound_init(void) {
    pcm_init();
    ac97_init();
    tts_init();
    printk(KERN_INFO "SOUND: Core Audio Architecture & Formant TTS Engine online\n");
}

int sound_register_device(sound_device_t* dev) {
    if (!dev) return -1;
    default_sound_dev = dev;
    printk(KERN_INFO "SOUND: Registered soundcard '%s' (%u Hz, %d-bit)\n",
           dev->name, dev->sample_rate, dev->bits_per_sample);
    return 0;
}

sound_device_t* sound_get_default_device(void) {
    return default_sound_dev;
}
