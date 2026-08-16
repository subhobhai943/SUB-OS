#ifndef _DRIVERS_SPEAKER_H
#define _DRIVERS_SPEAKER_H

#include <stdint.h>

void speaker_on(uint32_t frequency);
void speaker_off(void);
void speaker_beep(uint32_t frequency, uint32_t duration_ms);

#endif // _DRIVERS_SPEAKER_H
