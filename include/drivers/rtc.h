#ifndef _DRIVERS_RTC_H
#define _DRIVERS_RTC_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint32_t year;
} rtc_time_t;

void rtc_init(void);
void rtc_get_time(rtc_time_t* time);
void rtc_format_string(char* buf, size_t max_len);

#endif // _DRIVERS_RTC_H
