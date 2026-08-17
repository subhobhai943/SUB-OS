#ifndef _DRIVERS_HWMON_H
#define _DRIVERS_HWMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char     chip_name[32];
    int32_t  cpu_temp_celsius;
    int32_t  ambient_temp_celsius;
    uint32_t fan_rpm;
    uint32_t vcore_millivolts;
    uint32_t v12_millivolts;
    uint32_t v5_millivolts;
    bool     initialized;
} hwmon_data_t;

void hwmon_init(void);
void hwmon_refresh(void);
const hwmon_data_t* hwmon_get_data(void);

#endif // _DRIVERS_HWMON_H
