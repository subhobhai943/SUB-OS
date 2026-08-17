#ifndef _SOUND_HDA_H
#define _SOUND_HDA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define HDA_PCI_CLASS    0x04
#define HDA_PCI_SUBCLASS 0x03

#define HDA_REG_GCAP   0x00
#define HDA_REG_GCTL   0x08
#define HDA_REG_STATESTS 0x0E
#define HDA_REG_CORBLBASE 0x40
#define HDA_REG_CORBUBASE 0x44
#define HDA_REG_CORBWP   0x48
#define HDA_REG_CORBRP   0x4A
#define HDA_REG_CORBCTL  0x4C
#define HDA_REG_RIRBLBASE 0x50
#define HDA_REG_RIRBUBASE 0x54
#define HDA_REG_RIRBWP   0x58
#define HDA_REG_RINTCNT  0x5A
#define HDA_REG_RIRBCTL  0x5C

typedef struct {
    char     codec_vendor[32];
    char     codec_name[32];
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bits_per_sample;
    bool     initialized;
} hda_device_info_t;

bool hda_init(void);
bool hda_is_detected(void);
const hda_device_info_t* hda_get_info(void);
int  hda_play_pcm(const uint8_t* pcm_data, size_t len);

#endif // _SOUND_HDA_H
