#ifndef _CRYPTO_CRYPTO_H
#define _CRYPTO_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// CRC32 API
uint32_t crc32(uint32_t crc, const void* buf, size_t size);

// MD5 API
typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t  buffer[64];
} md5_ctx_t;

void md5_init(md5_ctx_t* ctx);
void md5_update(md5_ctx_t* ctx, const void* input, size_t input_len);
void md5_final(uint8_t digest[16], md5_ctx_t* ctx);
void md5_string(const char* str, char output[33]);

// SHA-256 API
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buffer[64];
    size_t   datalen;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t* ctx);
void sha256_update(sha256_ctx_t* ctx, const void* data, size_t len);
void sha256_final(uint8_t hash[32], sha256_ctx_t* ctx);
void sha256_string(const char* str, char output[65]);

// PRNG (Cryptographic Pseudo-Random Number Generator)
void prng_seed(uint64_t seed1, uint64_t seed2);
uint64_t prng_rand64(void);
uint32_t prng_rand32(void);
void prng_get_bytes(void* buf, size_t len);

#endif // _CRYPTO_CRYPTO_H
