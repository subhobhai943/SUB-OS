#include <sound/ac97.h>
#include <sound/sound.h>
#include <drivers/pci.h>
#include <arch/x86_64/io.h>
#include <kernel/printk.h>

static uint32_t ac97_nam_bar = 0;
static uint32_t ac97_nabm_bar = 0;
static bool ac97_present = false;

static int ac97_play(const void* buffer, size_t length) {
    (void)buffer; (void)length;
    return (int)length;
}

static int ac97_set_vol(int volume) {
    if (ac97_nam_bar) {
        uint16_t vol_val = (uint16_t)(((100 - volume) * 31 / 100) | (((100 - volume) * 31 / 100) << 8));
        outw((uint16_t)(ac97_nam_bar + 0x02), vol_val);
    }
    return 0;
}

static sound_device_t ac97_device = {
    .name = "Intel 82801AA AC'97 Audio",
    .sample_rate = 48000,
    .channels = 2,
    .bits_per_sample = 16,
    .volume = 80,
    .play_sample = ac97_play,
    .set_volume = ac97_set_vol,
    .active = true
};

bool ac97_init(void) {
    pci_device_t* dev = pci_find_device(0x8086, 0x2415);
    if (!dev) {
        dev = pci_find_class(0x04, 0x01); // Multimedia Audio
    }

    if (dev) {
        ac97_nam_bar = dev->bar[0] & ~1;
        ac97_nabm_bar = dev->bar[1] & ~1;
        ac97_present = true;
        pci_enable_bus_mastering(dev);
        sound_register_device(&ac97_device);
        printk(KERN_INFO "AC97: Audio Controller detected at NAM:0x%x, NABM:0x%x (IRQ %d)\n",
               ac97_nam_bar, ac97_nabm_bar, dev->irq);
        return true;
    }

    return false;
}

void ac97_set_master_volume(uint8_t left, uint8_t right) {
    if (ac97_nam_bar) {
        outw((uint16_t)(ac97_nam_bar + 0x02), (uint16_t)(left | (right << 8)));
    }
}

int ac97_play_pcm(const void* samples, size_t length) {
    return ac97_play(samples, length);
}
