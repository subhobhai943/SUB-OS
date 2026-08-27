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
// One-shot hash helpers
static inline void md5(const void* data, size_t len, uint8_t digest[16]) {
    md5_ctx_t ctx;
    md5_init(&ctx);
    md5_update(&ctx, data, len);
    md5_final(digest, &ctx);
}

static inline void sha256(const void* data, size_t len, uint8_t hash[32]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(hash, &ctx);
}

// X25519 (Curve25519 ECDH), RFC 7748.
void x25519_base(uint8_t out_pub[32], const uint8_t priv[32]);
void x25519(uint8_t out_shared[32], const uint8_t priv[32], const uint8_t peer_pub[32]);

// ChaCha20-Poly1305 AEAD, RFC 8439.
void chacha20_xor(uint8_t* dst, const uint8_t* src, size_t len,
                  const uint8_t key[32], uint32_t counter, const uint8_t nonce[12]);
void chacha20poly1305_encrypt(uint8_t* ct, uint8_t tag[16],
                              const uint8_t key[32], const uint8_t nonce[12],
                              const uint8_t* aad, size_t aad_len,
                              const uint8_t* pt, size_t pt_len);
bool chacha20poly1305_decrypt(uint8_t* pt, const uint8_t key[32],
                              const uint8_t nonce[12],
                              const uint8_t* aad, size_t aad_len,
                              const uint8_t* ct, size_t ct_len,
                              const uint8_t tag[16]);

// HMAC-SHA256 / HKDF (RFC 2104, 5869) and the TLS 1.3 key-schedule helpers.
void hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* msg, size_t msg_len, uint8_t out[32]);
void hkdf_extract(const uint8_t* salt, size_t salt_len,
                  const uint8_t* ikm, size_t ikm_len, uint8_t prk[32]);
void hkdf_expand(const uint8_t prk[32], const uint8_t* info, size_t info_len,
                 uint8_t* out, size_t out_len);
void tls13_expand_label(const uint8_t secret[32], const char* label,
                        const uint8_t* context, size_t context_len,
                        uint8_t* out, size_t out_len);
void tls13_derive_secret(const uint8_t secret[32], const char* label,
                         const uint8_t transcript_hash[32], uint8_t out[32]);

// PRNG (Cryptographic Pseudo-Random Number Generator)
void prng_seed(uint64_t seed1, uint64_t seed2);
uint64_t prng_rand64(void);
uint32_t prng_rand32(void);
void prng_get_bytes(void* buf, size_t len);

#endif // _CRYPTO_CRYPTO_H
