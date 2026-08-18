#ifndef _KERNEL_RUST_H
#define _KERNEL_RUST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Thermal Report Struct
typedef struct {
    uint32_t target_fan_rpm;
    uint8_t throttle_level;
    bool is_critical;
} __attribute__((packed)) rust_thermal_report_t;

// Kernel Health Watchdog Report
typedef struct {
    bool is_healthy;
    uint64_t total_heartbeats;
    uint32_t active_modules;
    uint32_t subsystem_mask;
} __attribute__((packed)) rust_health_report_t;

// PCI Info
typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass_id;
} __attribute__((packed)) rust_pci_info_t;

// Kernel Core
int rust_kernel_init(void);
const char* rust_kernel_status(void);

// Cryptography
int rust_chacha20_crypt(const uint8_t* key, const uint8_t* nonce, uint32_t counter, uint8_t* data, size_t len);
int rust_csprng_get_random(uint8_t* buffer, size_t len);
int rust_sha3_256(const uint8_t* data, size_t len, uint8_t* out);
int rust_aes128_ecb_encrypt(const uint8_t* key, uint8_t* block);

// Hardware & Drivers
rust_thermal_report_t rust_sensor_calc_fan_curve(int32_t temp_celsius);
const char* rust_pci_get_vendor_name(uint16_t vendor_id);
const char* rust_pci_get_class_name(uint8_t class_id, uint8_t subclass_id);

// VFS Directory Cache (dcache)
int rust_dcache_lookup(const uint8_t* path, size_t len, uint64_t* out_inode, bool* out_is_dir);
void rust_dcache_insert(const uint8_t* path, size_t len, uint64_t inode, bool is_dir);
void rust_dcache_get_metrics(uint64_t* out_hits, uint64_t* out_misses);

// Kernel Watchdog & JSON
void rust_watchdog_ping(uint8_t subsystem_id);
rust_health_report_t rust_watchdog_get_report(void);
int rust_json_get_string(const uint8_t* json_str, size_t json_len, const uint8_t* key, size_t key_len, uint8_t* out_val, size_t max_out_len);

#endif // _KERNEL_RUST_H
