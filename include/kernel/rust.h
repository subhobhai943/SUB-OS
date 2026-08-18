#ifndef _KERNEL_RUST_H
#define _KERNEL_RUST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t target_fan_rpm;
    uint8_t throttle_level;
    bool is_critical;
} __attribute__((packed)) rust_thermal_report_t;

int rust_kernel_init(void);
const char* rust_kernel_status(void);
int rust_chacha20_crypt(const uint8_t* key, const uint8_t* nonce, uint32_t counter, uint8_t* data, size_t len);
int rust_csprng_get_random(uint8_t* buffer, size_t len);
rust_thermal_report_t rust_sensor_calc_fan_curve(int32_t temp_celsius);

#endif // _KERNEL_RUST_H
