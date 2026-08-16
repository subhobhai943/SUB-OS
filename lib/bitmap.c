#include <lib/bitmap.h>

int64_t bitmap_find_first_free(const bitmap_word_t* map, size_t total_bits) {
    size_t words = (total_bits + BITS_PER_WORD - 1) / BITS_PER_WORD;

    for (size_t w = 0; w < words; w++) {
        if (map[w] != (bitmap_word_t)~0ULL) {
            for (size_t b = 0; b < BITS_PER_WORD; b++) {
                size_t bit_idx = w * BITS_PER_WORD + b;
                if (bit_idx >= total_bits) return -1;
                if (!bitmap_test(map, bit_idx)) {
                    return (int64_t)bit_idx;
                }
            }
        }
    }
    return -1;
}

int64_t bitmap_find_contiguous_free(const bitmap_word_t* map, size_t total_bits, size_t count) {
    if (count == 0) return -1;
    if (count == 1) return bitmap_find_first_free(map, total_bits);

    size_t run = 0;
    size_t start = 0;

    for (size_t i = 0; i < total_bits; i++) {
        if (!bitmap_test(map, i)) {
            if (run == 0) start = i;
            run++;
            if (run == count) {
                return (int64_t)start;
            }
        } else {
            run = 0;
        }
    }
    return -1;
}
