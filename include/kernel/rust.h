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

// Decoded Partition Structure
typedef struct {
    uint32_t partition_number;
    bool is_bootable;
    bool is_gpt;
    uint8_t partition_type_id;
    uint64_t start_lba;
    uint64_t sector_count;
    uint64_t size_mb;
    char name[36];
} rust_decoded_partition_t;

// Crypto Benchmark Result
typedef struct {
    uint32_t chacha20_mbs;
    uint32_t sha3_256_mbs;
    uint32_t aes128_mbs;
    uint32_t score;
} rust_crypto_bench_result_t;

// Network Filter Rule
typedef struct {
    bool enabled;
    uint8_t protocol;
    uint32_t src_ip;
    uint32_t src_mask;
    uint32_t dst_ip;
    uint32_t dst_mask;
    uint16_t src_port_start;
    uint16_t src_port_end;
    uint16_t dst_port_start;
    uint16_t dst_port_end;
    uint8_t action;
    uint64_t packet_count;
    uint64_t byte_count;
} rust_filter_rule_t;

// Network Packet Header
typedef struct {
    uint8_t protocol;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t length;
} rust_packet_header_t;

// Kernel Core
int rust_kernel_init(void);
const char* rust_kernel_status(void);

// Checksums & Non-Cryptographic Hashing (Rust)
uint32_t rust_crc32c(const uint8_t* data, size_t len);
uint32_t rust_adler32(const uint8_t* data, size_t len);
uint64_t rust_fnv1a64(const uint8_t* data, size_t len);

// Cryptography
int rust_chacha20_crypt(const uint8_t* key, const uint8_t* nonce, uint32_t counter, uint8_t* data, size_t len);
int rust_csprng_get_random(uint8_t* buffer, size_t len);
int rust_sha3_256(const uint8_t* data, size_t len, uint8_t* out);
int rust_aes128_ecb_encrypt(const uint8_t* key, uint8_t* block);
int rust_base64_encode(const uint8_t* in_buf, size_t in_len, uint8_t* out_buf, size_t max_out);
int rust_base64_decode(const uint8_t* in_buf, size_t in_len, uint8_t* out_buf, size_t max_out);
int rust_crypto_run_benchmark(rust_crypto_bench_result_t* out_result);

// Hardware & Drivers
rust_thermal_report_t rust_sensor_calc_fan_curve(int32_t temp_celsius);
const char* rust_pci_get_vendor_name(uint16_t vendor_id);
const char* rust_pci_get_class_name(uint8_t class_id, uint8_t subclass_id);

// Storage & Partitions
int rust_storage_decode_mbr(const uint8_t* sector_512, rust_decoded_partition_t* out_partitions, size_t max_count);

// Network Filter
uint8_t rust_filter_evaluate(const rust_packet_header_t* pkt);
int rust_filter_add_rule(const rust_filter_rule_t* rule);
void rust_filter_get_stats(uint64_t* out_processed, uint64_t* out_blocked, size_t* out_rule_count);

// VFS Directory Cache (dcache)
int rust_dcache_lookup(const uint8_t* path, size_t len, uint64_t* out_inode, bool* out_is_dir);
void rust_dcache_insert(const uint8_t* path, size_t len, uint64_t inode, bool is_dir);
void rust_dcache_get_metrics(uint64_t* out_hits, uint64_t* out_misses);

// Kernel Watchdog, JSON & ELF
void rust_watchdog_ping(uint8_t subsystem_id);
rust_health_report_t rust_watchdog_get_report(void);
int rust_json_get_string(const uint8_t* json_str, size_t json_len, const uint8_t* key, size_t key_len, uint8_t* out_val, size_t max_out_len);
int rust_elf_validate(const uint8_t* buf, size_t len);
int rust_elf_get_entry(const uint8_t* buf, size_t len, uint64_t* out_entry);
void rust_elf_dump(const uint8_t* buf, size_t len);

// Memory Management
void rust_buddy_analyze(uint64_t total_pages, uint64_t free_pages);
void rust_buddy_dump_stats(void);

// AEAD Authenticated Encryption (ChaCha20-Poly1305)
int rust_aead_encrypt(const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aad_len, const uint8_t* plain, size_t plain_len, uint8_t* cipher_out, uint8_t* tag_out);
int rust_aead_decrypt(const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aad_len, const uint8_t* cipher, size_t cipher_len, const uint8_t* tag, uint8_t* plain_out);

#endif // _KERNEL_RUST_H
