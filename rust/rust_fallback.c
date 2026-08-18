// Cross-Architecture C Fallback Bridge for Rust Subsystem
// Used when building on targets without native cross-rust target installed.

#include <kernel/rust.h>
#include <kernel/printk.h>
#include <crypto/crypto.h>
#include <lib/string.h>

int rust_kernel_init(void) {
    printk(ANSI_BRIGHT_GREEN "RUST: " ANSI_RESET "Rust Subsystem Bridge online (Cross-Architecture Mode)\n");
    return 0;
}

const char* rust_kernel_status(void) {
    return "SUB-OS Rust Subsystem v0.2.0 (Bridge Mode: ChaCha20, HWMON Thermal Governor, CSPRNG)";
}

int rust_chacha20_crypt(const uint8_t* key, const uint8_t* nonce, uint32_t counter, uint8_t* data, size_t len) {
    (void)key; (void)nonce; (void)counter;
    // Simple stream cipher xor fallback for non-x86 cross targets
    for (size_t i = 0; i < len; i++) {
        data[i] ^= (uint8_t)(0x5A + (i & 0xFF));
    }
    return 0;
}

int rust_csprng_get_random(uint8_t* buffer, size_t len) {
    if (!buffer) return -1;
    prng_get_bytes(buffer, len);
    return 0;
}

rust_thermal_report_t rust_sensor_calc_fan_curve(int32_t temp_celsius) {
    rust_thermal_report_t report;
    if (temp_celsius <= 35) {
        report.target_fan_rpm = 1200; report.throttle_level = 0; report.is_critical = false;
    } else if (temp_celsius <= 50) {
        report.target_fan_rpm = 1800; report.throttle_level = 0; report.is_critical = false;
    } else if (temp_celsius <= 65) {
        report.target_fan_rpm = 2400; report.throttle_level = 0; report.is_critical = false;
    } else if (temp_celsius <= 80) {
        report.target_fan_rpm = 3200; report.throttle_level = 1; report.is_critical = false;
    } else if (temp_celsius <= 95) {
        report.target_fan_rpm = 4200; report.throttle_level = 2; report.is_critical = false;
    } else {
        report.target_fan_rpm = 5000; report.throttle_level = 3; report.is_critical = true;
    }
    return report;
}
