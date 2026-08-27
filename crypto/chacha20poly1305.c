/*
 * ChaCha20-Poly1305 AEAD, RFC 8439.
 *
 * The record-protection cipher for the TLS 1.3 client. ChaCha20 is a pure
 * add-rotate-xor stream cipher (no tables, so no data-dependent timing), and
 * Poly1305 is its one-time authenticator; together they form the AEAD that
 * encrypts and authenticates every TLS record. All integer arithmetic, so it
 * is safe in the freestanding kernel.
 *
 * Poly1305 here is the 32-bit "donna" formulation (public domain), which needs
 * only 64-bit intermediate products.
 */
#include <crypto/crypto.h>
#include <lib/string.h>

// ===========================================================================
// ChaCha20
// ===========================================================================
static inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static inline uint32_t load32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void store32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

#define QR(a, b, c, d)                         \
    a += b; d ^= a; d = rotl32(d, 16);         \
    c += d; b ^= c; b = rotl32(b, 12);         \
    a += b; d ^= a; d = rotl32(d, 8);          \
    c += d; b ^= c; b = rotl32(b, 7)

static void chacha20_block(const uint32_t in[16], uint8_t out[64]) {
    uint32_t x[16];
    for (int i = 0; i < 16; i++) x[i] = in[i];
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }
    for (int i = 0; i < 16; i++) store32_le(out + 4 * i, x[i] + in[i]);
}

static void chacha20_init(uint32_t state[16], const uint8_t key[32],
                          uint32_t counter, const uint8_t nonce[12]) {
    state[0] = 0x61707865; state[1] = 0x3320646e;
    state[2] = 0x79622d32; state[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) state[4 + i] = load32_le(key + 4 * i);
    state[12] = counter;
    state[13] = load32_le(nonce + 0);
    state[14] = load32_le(nonce + 4);
    state[15] = load32_le(nonce + 8);
}

// Encrypt/decrypt src into dst (they may alias) starting at block counter.
void chacha20_xor(uint8_t* dst, const uint8_t* src, size_t len,
                  const uint8_t key[32], uint32_t counter, const uint8_t nonce[12]) {
    uint32_t state[16];
    uint8_t block[64];
    chacha20_init(state, key, counter, nonce);

    size_t off = 0;
    while (off < len) {
        chacha20_block(state, block);
        state[12]++;                               // next block counter
        size_t n = len - off < 64 ? len - off : 64;
        for (size_t i = 0; i < n; i++) dst[off + i] = src[off + i] ^ block[i];
        off += n;
    }
}

// ===========================================================================
// Poly1305 (32-bit donna)
// ===========================================================================
typedef struct {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
    size_t   leftover;
    uint8_t  buffer[16];
    uint8_t  final;
} poly1305_ctx;

static void poly1305_init(poly1305_ctx* st, const uint8_t key[32]) {
    st->r[0] = (load32_le(&key[0])) & 0x3ffffff;
    st->r[1] = (load32_le(&key[3]) >> 2) & 0x3ffff03;
    st->r[2] = (load32_le(&key[6]) >> 4) & 0x3ffc0ff;
    st->r[3] = (load32_le(&key[9]) >> 6) & 0x3f03fff;
    st->r[4] = (load32_le(&key[12]) >> 8) & 0x00fffff;

    st->h[0] = st->h[1] = st->h[2] = st->h[3] = st->h[4] = 0;
    st->pad[0] = load32_le(&key[16]);
    st->pad[1] = load32_le(&key[20]);
    st->pad[2] = load32_le(&key[24]);
    st->pad[3] = load32_le(&key[28]);
    st->leftover = 0;
    st->final = 0;
}

static void poly1305_blocks(poly1305_ctx* st, const uint8_t* m, size_t bytes) {
    const uint32_t hibit = st->final ? 0 : (1UL << 24);
    uint32_t r0 = st->r[0], r1 = st->r[1], r2 = st->r[2], r3 = st->r[3], r4 = st->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3], h4 = st->h[4];

    while (bytes >= 16) {
        uint64_t d0, d1, d2, d3, d4;
        uint32_t c;

        h0 += (load32_le(m + 0)) & 0x3ffffff;
        h1 += (load32_le(m + 3) >> 2) & 0x3ffffff;
        h2 += (load32_le(m + 6) >> 4) & 0x3ffffff;
        h3 += (load32_le(m + 9) >> 6) & 0x3ffffff;
        h4 += (load32_le(m + 12) >> 8) | hibit;

        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5; c = (h0 >> 26); h0 &= 0x3ffffff;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3; st->h[4] = h4;
}

static void poly1305_update(poly1305_ctx* st, const uint8_t* m, size_t bytes) {
    if (st->leftover) {
        size_t want = 16 - st->leftover;
        if (want > bytes) want = bytes;
        for (size_t i = 0; i < want; i++) st->buffer[st->leftover + i] = m[i];
        bytes -= want;
        m += want;
        st->leftover += want;
        if (st->leftover < 16) return;
        poly1305_blocks(st, st->buffer, 16);
        st->leftover = 0;
    }
    if (bytes >= 16) {
        size_t want = bytes & ~(size_t)15;
        poly1305_blocks(st, m, want);
        m += want;
        bytes -= want;
    }
    for (size_t i = 0; i < bytes; i++) st->buffer[st->leftover + i] = m[i];
    st->leftover += bytes;
}

static void poly1305_finish(poly1305_ctx* st, uint8_t mac[16]) {
    if (st->leftover) {
        size_t i = st->leftover;
        st->buffer[i++] = 1;
        for (; i < 16; i++) st->buffer[i] = 0;
        st->final = 1;
        poly1305_blocks(st, st->buffer, 16);
    }

    uint32_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3], h4 = st->h[4];
    uint32_t c;

    c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1UL << 26);

    uint32_t mask = (g4 >> 31) - 1;   // 0 if g>=p (use g), 0xffffffff if g<p (use h)
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    // Serialise h into 128-bit little-endian, add pad.
    uint64_t f;
    h0 = (h0) | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);

    f = (uint64_t)h0 + st->pad[0];             h0 = (uint32_t)f;
    f = (uint64_t)h1 + st->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + st->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + st->pad[3] + (f >> 32); h3 = (uint32_t)f;

    store32_le(mac + 0,  h0);
    store32_le(mac + 4,  h1);
    store32_le(mac + 8,  h2);
    store32_le(mac + 12, h3);
}

// ===========================================================================
// AEAD construction (RFC 8439 section 2.8)
// ===========================================================================
static void poly1305_key_gen(uint8_t out_key[32], const uint8_t key[32],
                             const uint8_t nonce[12]) {
    uint32_t state[16];
    uint8_t block[64];
    chacha20_init(state, key, 0, nonce);
    chacha20_block(state, block);
    memcpy(out_key, block, 32);
}

static const uint8_t poly_zeros[16] = { 0 };

static void aead_mac(uint8_t tag[16], const uint8_t poly_key[32],
                     const uint8_t* aad, size_t aad_len,
                     const uint8_t* ct, size_t ct_len) {
    poly1305_ctx st;
    poly1305_init(&st, poly_key);

    poly1305_update(&st, aad, aad_len);
    if (aad_len % 16) poly1305_update(&st, poly_zeros, 16 - (aad_len % 16));

    poly1305_update(&st, ct, ct_len);
    if (ct_len % 16) poly1305_update(&st, poly_zeros, 16 - (ct_len % 16));

    uint8_t lengths[16];
    for (int i = 0; i < 8; i++) lengths[i]     = (uint8_t)(aad_len >> (8 * i));
    for (int i = 0; i < 8; i++) lengths[8 + i] = (uint8_t)(ct_len  >> (8 * i));
    poly1305_update(&st, lengths, 16);

    poly1305_finish(&st, tag);
}

// Encrypt plaintext -> ciphertext (same length) and produce a 16-byte tag.
void chacha20poly1305_encrypt(uint8_t* ct, uint8_t tag[16],
                              const uint8_t key[32], const uint8_t nonce[12],
                              const uint8_t* aad, size_t aad_len,
                              const uint8_t* pt, size_t pt_len) {
    uint8_t poly_key[32];
    poly1305_key_gen(poly_key, key, nonce);
    chacha20_xor(ct, pt, pt_len, key, 1, nonce);   // counter starts at 1
    aead_mac(tag, poly_key, aad, aad_len, ct, pt_len);
}

// Verify tag and decrypt ciphertext -> plaintext. Returns true on success.
bool chacha20poly1305_decrypt(uint8_t* pt, const uint8_t key[32],
                              const uint8_t nonce[12],
                              const uint8_t* aad, size_t aad_len,
                              const uint8_t* ct, size_t ct_len,
                              const uint8_t tag[16]) {
    uint8_t poly_key[32];
    poly1305_key_gen(poly_key, key, nonce);

    uint8_t expect[16];
    aead_mac(expect, poly_key, aad, aad_len, ct, ct_len);

    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= expect[i] ^ tag[i];
    if (diff) return false;                        // authentication failed

    chacha20_xor(pt, ct, ct_len, key, 1, nonce);
    return true;
}
