/*
 * HMAC-SHA256 (RFC 2104), HKDF (RFC 5869), and the TLS 1.3 HKDF-Expand-Label
 * / Derive-Secret helpers (RFC 8446 section 7.1).
 *
 * These sit on top of the existing SHA-256 and drive the TLS 1.3 key schedule:
 * every traffic key, IV and Finished key is produced by an Expand-Label over a
 * secret and the running transcript hash.
 */
#include <crypto/crypto.h>
#include <lib/string.h>

#define SHA256_BLOCK 64
#define SHA256_LEN   32

void hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
    uint8_t k[SHA256_BLOCK];
    uint8_t ipad[SHA256_BLOCK], opad[SHA256_BLOCK];

    if (key_len > SHA256_BLOCK) {
        sha256(key, key_len, k);
        memset(k + SHA256_LEN, 0, SHA256_BLOCK - SHA256_LEN);
    } else {
        memcpy(k, key, key_len);
        memset(k + key_len, 0, SHA256_BLOCK - key_len);
    }

    for (int i = 0; i < SHA256_BLOCK; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    uint8_t inner[SHA256_LEN];
    sha256_ctx_t c;
    sha256_init(&c);
    sha256_update(&c, ipad, SHA256_BLOCK);
    sha256_update(&c, msg, msg_len);
    sha256_final(inner, &c);

    sha256_init(&c);
    sha256_update(&c, opad, SHA256_BLOCK);
    sha256_update(&c, inner, SHA256_LEN);
    sha256_final(out, &c);
}

void hkdf_extract(const uint8_t* salt, size_t salt_len,
                  const uint8_t* ikm, size_t ikm_len, uint8_t prk[32]) {
    static const uint8_t zero[SHA256_LEN] = { 0 };
    if (!salt || salt_len == 0) { salt = zero; salt_len = SHA256_LEN; }
    hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
}

void hkdf_expand(const uint8_t prk[32], const uint8_t* info, size_t info_len,
                 uint8_t* out, size_t out_len) {
    uint8_t t[SHA256_LEN];
    size_t t_len = 0;
    size_t done = 0;
    uint8_t counter = 1;

    while (done < out_len) {
        // T(n) = HMAC(PRK, T(n-1) | info | counter)
        sha256_ctx_t hc; (void)hc;
        uint8_t block[SHA256_LEN + 512 + 1];   // t | info | counter
        size_t bl = 0;
        if (t_len) { memcpy(block, t, t_len); bl += t_len; }
        if (info_len > 512) info_len = 512;
        memcpy(block + bl, info, info_len); bl += info_len;
        block[bl++] = counter;

        hmac_sha256(prk, SHA256_LEN, block, bl, t);
        t_len = SHA256_LEN;

        size_t n = out_len - done < SHA256_LEN ? out_len - done : SHA256_LEN;
        memcpy(out + done, t, n);
        done += n;
        counter++;
    }
}

// TLS 1.3 HKDF-Expand-Label:
//   struct { uint16 length; opaque label<7..255>; opaque context<0..255>; }
// label is prefixed with "tls13 ".
void tls13_expand_label(const uint8_t secret[32], const char* label,
                        const uint8_t* context, size_t context_len,
                        uint8_t* out, size_t out_len) {
    uint8_t info[512];
    size_t p = 0;

    info[p++] = (uint8_t)(out_len >> 8);
    info[p++] = (uint8_t)(out_len & 0xff);

    char full_label[64] = "tls13 ";
    size_t ll = strlen(full_label);
    size_t rest = strlen(label);
    if (ll + rest > sizeof(full_label) - 1) rest = sizeof(full_label) - 1 - ll;
    memcpy(full_label + ll, label, rest);
    size_t label_len = ll + rest;

    info[p++] = (uint8_t)label_len;
    memcpy(info + p, full_label, label_len); p += label_len;

    info[p++] = (uint8_t)context_len;
    if (context_len) { memcpy(info + p, context, context_len); p += context_len; }

    hkdf_expand(secret, info, p, out, out_len);
}

// Derive-Secret(secret, label, messages) = Expand-Label(secret, label,
//   Hash(messages), Hash.length). `transcript_hash` is that Hash(messages).
void tls13_derive_secret(const uint8_t secret[32], const char* label,
                         const uint8_t transcript_hash[32], uint8_t out[32]) {
    tls13_expand_label(secret, label, transcript_hash, SHA256_LEN, out, SHA256_LEN);
}
