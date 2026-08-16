#include <crypto/crypto.h>

static uint32_t crc32_table[256];
static bool table_computed = false;

static void make_crc32_table(void) {
    uint32_t c;
    for (uint32_t n = 0; n < 256; n++) {
        c = n;
        for (int k = 0; k < 8; k++) {
            if (c & 1) {
                c = 0xEDB88320L ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        crc32_table[n] = c;
    }
    table_computed = true;
}

uint32_t crc32(uint32_t crc, const void* buf, size_t size) {
    if (!table_computed) {
        make_crc32_table();
    }

    const uint8_t* p = (const uint8_t*)buf;
    crc = crc ^ 0xFFFFFFFF;

    while (size--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}
