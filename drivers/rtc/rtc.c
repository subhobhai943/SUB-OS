#include <drivers/rtc.h>
#include <arch/x86_64/io.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, (uint8_t)reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd2bin(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

void rtc_init(void) {
    rtc_time_t t;
    rtc_get_time(&t);
    printk(KERN_INFO "RTC: CMOS Real-Time Clock synchronized (%04u-%02u-%02u %02u:%02u:%02u UTC)\n",
           t.year, t.month, t.day, t.hour, t.minute, t.second);
}

void rtc_get_time(rtc_time_t* time) {
    if (!time) return;

    time->second = bcd2bin(get_rtc_register(0x00));
    time->minute = bcd2bin(get_rtc_register(0x02));
    time->hour   = bcd2bin(get_rtc_register(0x04));
    time->day    = bcd2bin(get_rtc_register(0x07));
    time->month  = bcd2bin(get_rtc_register(0x08));
    time->year   = 2000 + bcd2bin(get_rtc_register(0x09));
}

void rtc_format_string(char* buf, size_t max_len) {
    if (!buf || max_len == 0) return;
    rtc_time_t t;
    rtc_get_time(&t);
    snprintf(buf, max_len, "%04u-%02u-%02u %02u:%02u:%02u UTC",
             t.year, t.month, t.day, t.hour, t.minute, t.second);
}
