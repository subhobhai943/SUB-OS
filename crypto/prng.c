#include <crypto/crypto.h>
#include <arch/x86_64/pit.h>

static uint64_t prng_s[2] = { 0x8a5cd789635d2dffULL, 0x121fd2155c472f96ULL };

void prng_seed(uint64_t seed1, uint64_t seed2) {
    if (seed1 == 0 && seed2 == 0) {
        seed1 = 0x8a5cd789635d2dffULL ^ pit_get_ticks();
        seed2 = 0x121fd2155c472f96ULL ^ (pit_get_ticks() << 32);
    }
    prng_s[0] = seed1;
    prng_s[1] = seed2;
}

uint64_t prng_rand64(void) {
    uint64_t s1 = prng_s[0];
    const uint64_t s0 = prng_s[1];
    prng_s[0] = s0;
    s1 ^= s1 << 23;
    prng_s[1] = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    return prng_s[1] + s0;
}

uint32_t prng_rand32(void) {
    return (uint32_t)(prng_rand64() & 0xFFFFFFFF);
}

void prng_get_bytes(void* buf, size_t len) {
    uint8_t* p = (uint8_t*)buf;
    while (len >= 8) {
        uint64_t r = prng_rand64();
        *(uint64_t*)p = r;
        p += 8;
        len -= 8;
    }
    if (len > 0) {
        uint64_t r = prng_rand64();
        uint8_t* r_bytes = (uint8_t*)&r;
        for (size_t i = 0; i < len; i++) {
            p[i] = r_bytes[i];
        }
    }
}
