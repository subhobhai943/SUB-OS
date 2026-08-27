/*
 * DEFLATE / zlib decompressor (RFC 1950/1951).
 *
 * A compact, self-contained inflate -- the algorithm every PNG needs. No
 * allocation, no floating point: it decodes straight into a caller-provided
 * output buffer, so it is safe in the freestanding kernel. Structure follows
 * the well-known "tinf" design (canonical Huffman tables, three block types).
 */
#include <lib/inflate.h>
#include <lib/string.h>

#define TINF_OK          0
#define TINF_DATA_ERROR (-3)
#define TINF_BUF_ERROR  (-5)

typedef struct {
    uint16_t counts[16];    // number of codes of each length
    uint16_t symbols[288];  // symbols sorted by code
} tinf_tree;

typedef struct {
    const uint8_t* src;
    const uint8_t* src_end;
    uint32_t tag;
    int      bitcount;

    uint8_t* dst;
    uint8_t* dst_start;
    uint8_t* dst_end;

    tinf_tree ltree;        // literal/length codes
    tinf_tree dtree;        // distance codes
} tinf;

// Special ordering of code-length codes (RFC 1951 3.2.7).
static const uint8_t clcidx[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static const uint16_t length_base[30] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0
};
static const uint8_t length_bits[30] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0
};
static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t dist_bits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static int getbit(tinf* d) {
    if (d->bitcount == 0) {
        if (d->src >= d->src_end) return -1;   // underflow flagged as a set bit? no: caller checks
        d->tag = *d->src++;
        d->bitcount = 8;
    }
    int bit = d->tag & 1;
    d->tag >>= 1;
    d->bitcount--;
    return bit;
}

// Read `num` bits LSB-first and add `base`.
static int read_bits(tinf* d, int num, int base) {
    int val = 0;
    for (int i = 0; i < num; i++) {
        int b = getbit(d);
        if (b < 0) return -1;
        val |= b << i;
    }
    return base + val;
}

// Build a canonical Huffman tree from a list of code lengths.
static void build_tree(tinf_tree* t, const uint8_t* lengths, unsigned num) {
    unsigned offs[16];
    for (int i = 0; i < 16; i++) t->counts[i] = 0;
    for (unsigned i = 0; i < num; i++) t->counts[lengths[i]]++;
    t->counts[0] = 0;

    offs[0] = 0;
    for (int i = 1; i < 16; i++) offs[i] = offs[i - 1] + t->counts[i - 1];
    for (unsigned i = 0; i < num; i++)
        if (lengths[i]) t->symbols[offs[lengths[i]]++] = (uint16_t)i;
}

static void build_fixed_trees(tinf_tree* lt, tinf_tree* dt) {
    uint8_t lengths[288];
    int i;
    for (i = 0;   i < 144; i++) lengths[i] = 8;
    for (;        i < 256; i++) lengths[i] = 9;
    for (;        i < 280; i++) lengths[i] = 7;
    for (;        i < 288; i++) lengths[i] = 8;
    build_tree(lt, lengths, 288);

    for (i = 0; i < 30; i++) lengths[i] = 5;
    build_tree(dt, lengths, 30);
}

static int decode_symbol(tinf* d, const tinf_tree* t) {
    int sum = 0, cur = 0, len = 0;
    do {
        int b = getbit(d);
        if (b < 0) return -1;
        cur = 2 * cur + b;
        len++;
        if (len >= 16) return -1;
        sum += t->counts[len];
        cur -= t->counts[len];
    } while (cur >= 0);
    int idx = sum + cur;
    if (idx < 0 || idx >= 288) return -1;
    return t->symbols[idx];
}

// Decode the dynamic Huffman trees at the start of a type-2 block.
static int decode_trees(tinf* d, tinf_tree* lt, tinf_tree* dt) {
    uint8_t lengths[288 + 32];

    int hlit  = read_bits(d, 5, 257);
    int hdist = read_bits(d, 5, 1);
    int hclen = read_bits(d, 4, 4);
    if (hlit < 0 || hdist < 0 || hclen < 0) return TINF_DATA_ERROR;
    if (hlit > 286 || hdist > 30) return TINF_DATA_ERROR;

    for (int i = 0; i < 19; i++) lengths[i] = 0;
    for (int i = 0; i < hclen; i++) {
        int clen = read_bits(d, 3, 0);
        if (clen < 0) return TINF_DATA_ERROR;
        lengths[clcidx[i]] = (uint8_t)clen;
    }

    tinf_tree code_tree;
    build_tree(&code_tree, lengths, 19);

    int num = 0;
    while (num < hlit + hdist) {
        int sym = decode_symbol(d, &code_tree);
        if (sym < 0) return TINF_DATA_ERROR;
        switch (sym) {
        case 16: {                              // copy previous length 3..6 times
            if (num == 0) return TINF_DATA_ERROR;
            int prev = lengths[num - 1];
            int n = read_bits(d, 2, 3);
            if (n < 0) return TINF_DATA_ERROR;
            while (n-- && num < hlit + hdist) lengths[num++] = (uint8_t)prev;
            break;
        }
        case 17: {                              // repeat 0 for 3..10 times
            int n = read_bits(d, 3, 3);
            if (n < 0) return TINF_DATA_ERROR;
            while (n-- && num < hlit + hdist) lengths[num++] = 0;
            break;
        }
        case 18: {                              // repeat 0 for 11..138 times
            int n = read_bits(d, 7, 11);
            if (n < 0) return TINF_DATA_ERROR;
            while (n-- && num < hlit + hdist) lengths[num++] = 0;
            break;
        }
        default:
            if (sym > 15) return TINF_DATA_ERROR;
            lengths[num++] = (uint8_t)sym;
            break;
        }
    }

    build_tree(lt, lengths, hlit);
    build_tree(dt, lengths + hlit, hdist);
    return TINF_OK;
}

static int inflate_block_data(tinf* d, const tinf_tree* lt, const tinf_tree* dt) {
    for (;;) {
        int sym = decode_symbol(d, lt);
        if (sym < 0) return TINF_DATA_ERROR;

        if (sym == 256) return TINF_OK;         // end of block

        if (sym < 256) {                        // literal byte
            if (d->dst >= d->dst_end) return TINF_BUF_ERROR;
            *d->dst++ = (uint8_t)sym;
            continue;
        }

        // length/distance pair
        sym -= 257;
        if (sym >= 29) return TINF_DATA_ERROR;
        int length = read_bits(d, length_bits[sym], length_base[sym]);
        if (length < 0) return TINF_DATA_ERROR;

        int dsym = decode_symbol(d, dt);
        if (dsym < 0 || dsym >= 30) return TINF_DATA_ERROR;
        int dist = read_bits(d, dist_bits[dsym], dist_base[dsym]);
        if (dist < 0) return TINF_DATA_ERROR;

        uint8_t* from = d->dst - dist;
        if (from < d->dst_start) return TINF_DATA_ERROR;
        if (d->dst + length > d->dst_end) return TINF_BUF_ERROR;
        for (int i = 0; i < length; i++) *d->dst++ = *from++;
    }
}

static int inflate_uncompressed_block(tinf* d) {
    d->bitcount = 0;                            // byte-align
    if (d->src + 4 > d->src_end) return TINF_DATA_ERROR;
    unsigned len  = d->src[0] | (d->src[1] << 8);
    unsigned nlen = d->src[2] | (d->src[3] << 8);
    d->src += 4;
    if ((len ^ 0xffff) != nlen) return TINF_DATA_ERROR;
    if (d->src + len > d->src_end) return TINF_DATA_ERROR;
    if (d->dst + len > d->dst_end) return TINF_BUF_ERROR;
    for (unsigned i = 0; i < len; i++) *d->dst++ = *d->src++;
    return TINF_OK;
}

// Decompress a raw DEFLATE stream. On success *dest_len is the byte count.
int inflate_raw(uint8_t* dest, unsigned* dest_len,
                const uint8_t* src, unsigned src_len) {
    tinf d;
    d.src = src; d.src_end = src + src_len;
    d.tag = 0; d.bitcount = 0;
    d.dst = dest; d.dst_start = dest; d.dst_end = dest + *dest_len;

    int bfinal;
    do {
        bfinal = getbit(&d);
        if (bfinal < 0) return TINF_DATA_ERROR;
        int btype = read_bits(&d, 2, 0);
        int res;
        switch (btype) {
        case 0:
            res = inflate_uncompressed_block(&d);
            break;
        case 1:
            build_fixed_trees(&d.ltree, &d.dtree);
            res = inflate_block_data(&d, &d.ltree, &d.dtree);
            break;
        case 2:
            res = decode_trees(&d, &d.ltree, &d.dtree);
            if (res == TINF_OK) res = inflate_block_data(&d, &d.ltree, &d.dtree);
            break;
        default:
            return TINF_DATA_ERROR;
        }
        if (res != TINF_OK) return res;
    } while (!bfinal);

    *dest_len = (unsigned)(d.dst - d.dst_start);
    return TINF_OK;
}

// Decompress a zlib stream (2-byte header, deflate body, 4-byte adler trailer).
int inflate_zlib(uint8_t* dest, unsigned* dest_len,
                 const uint8_t* src, unsigned src_len) {
    if (src_len < 2) return TINF_DATA_ERROR;
    int cmf = src[0], flg = src[1];
    if ((cmf & 0x0f) != 8) return TINF_DATA_ERROR;     // not deflate
    if (((cmf << 8) | flg) % 31 != 0) return TINF_DATA_ERROR;
    if (flg & 0x20) return TINF_DATA_ERROR;            // preset dictionary unsupported
    return inflate_raw(dest, dest_len, src + 2, src_len - 2);
}
