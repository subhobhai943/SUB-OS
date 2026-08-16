#ifndef _LIB_BITMAP_H
#define _LIB_BITMAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint64_t bitmap_word_t;
#define BITS_PER_WORD (sizeof(bitmap_word_t) * 8)

static inline void bitmap_set(bitmap_word_t* map, size_t bit) {
    map[bit / BITS_PER_WORD] |= ((bitmap_word_t)1 << (bit % BITS_PER_WORD));
}

static inline void bitmap_clear(bitmap_word_t* map, size_t bit) {
    map[bit / BITS_PER_WORD] &= ~((bitmap_word_t)1 << (bit % BITS_PER_WORD));
}

static inline bool bitmap_test(const bitmap_word_t* map, size_t bit) {
    return (map[bit / BITS_PER_WORD] & ((bitmap_word_t)1 << (bit % BITS_PER_WORD))) != 0;
}

int64_t bitmap_find_first_free(const bitmap_word_t* map, size_t total_bits);
int64_t bitmap_find_contiguous_free(const bitmap_word_t* map, size_t total_bits, size_t count);

#endif // _LIB_BITMAP_H
