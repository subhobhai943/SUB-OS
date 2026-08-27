/*
 * Baseline JPEG decoder (ITU-T T.81, SOF0 sequential DCT).
 *
 * The web's photo format. Supports grayscale and YCbCr, arbitrary chroma
 * subsampling, restart markers, and the standard integer IDCT -- all integer
 * arithmetic, safe in the freestanding kernel. Progressive JPEG (SOF2) and
 * arithmetic coding are rejected. Output is a flat 0xAARRGGBB buffer, so it
 * plugs into the same image path as PNG and BMP.
 *
 * The Huffman decode uses canonical tables (counts + symbols), the way the
 * inflate decoder does, to avoid a large fast-table. The integer row/column
 * IDCT is the well-known fixed-point transform from the JPEG reference.
 */
#include <lib/jpeg.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

#define JPEG_MAX_PIXELS (4 * 1024 * 1024)

typedef struct {
    uint8_t  counts[17];        // number of codes of each length 1..16
    uint8_t  symbols[256];
    int      first_code[17];    // first canonical code of each length
    int      sym_index[17];     // index into symbols[] for each length
    int      maxlen;
} huff_t;

typedef struct {
    int cid;
    int ssx, ssy;               // sampling factors
    int qtsel;
    int dctab, actab;
    int dcpred;
    int width, height;          // this component's plane size (upsampled to full)
    int bx, by;                 // blocks per MCU (== ssx, ssy)
    uint8_t* pixels;            // decoded, upsampled to image size
    uint8_t* plane;             // native-resolution samples
    int pstride;                // stride of plane
    int pw, ph;                 // plane width/height (block-aligned)
} comp_t;

typedef struct {
    const uint8_t* data;
    int size;
    int pos;

    int width, height;
    int ncomp;
    comp_t comp[3];

    uint16_t qtab[4][64];
    huff_t   huff[8];           // 0..3 DC, 4..7 AC

    int restart_interval;

    // entropy bit reader
    int bitbuf;
    int bitcnt;
    int marker;                 // non-zero: a marker byte was hit in the stream

    int mcu_w, mcu_h;           // MCU size in pixels
    int mcu_cols, mcu_rows;
} jpeg_t;

static const uint8_t zz[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static inline int clip8(int x) { return x < 0 ? 0 : (x > 255 ? 255 : x); }

// ===========================================================================
// Integer IDCT (JPEG reference fixed-point)
// ===========================================================================
#define W1 2841
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

static void row_idct(int* blk) {
    int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    if (!((x1 = blk[4] << 11) | (x2 = blk[6]) | (x3 = blk[2]) |
          (x4 = blk[1]) | (x5 = blk[7]) | (x6 = blk[5]) | (x7 = blk[3]))) {
        blk[0] = blk[1] = blk[2] = blk[3] = blk[4] = blk[5] = blk[6] = blk[7] = blk[0] << 3;
        return;
    }
    x0 = (blk[0] << 11) + 128;
    x8 = W7 * (x4 + x5);
    x4 = x8 + (W1 - W7) * x4;
    x5 = x8 - (W1 + W7) * x5;
    x8 = W3 * (x6 + x7);
    x6 = x8 - (W3 - W5) * x6;
    x7 = x8 - (W3 + W5) * x7;
    x8 = x0 + x1; x0 -= x1;
    x1 = W6 * (x3 + x2);
    x2 = x1 - (W2 + W6) * x2;
    x3 = x1 + (W2 - W6) * x3;
    x1 = x4 + x6; x4 -= x6;
    x6 = x5 + x7; x5 -= x7;
    x7 = x8 + x3; x8 -= x3;
    x3 = x0 + x2; x0 -= x2;
    x2 = (181 * (x4 + x5) + 128) >> 8;
    x4 = (181 * (x4 - x5) + 128) >> 8;
    blk[0] = (x7 + x1) >> 8;
    blk[1] = (x3 + x2) >> 8;
    blk[2] = (x0 + x4) >> 8;
    blk[3] = (x8 + x6) >> 8;
    blk[4] = (x8 - x6) >> 8;
    blk[5] = (x0 - x4) >> 8;
    blk[6] = (x3 - x2) >> 8;
    blk[7] = (x7 - x1) >> 8;
}

static void col_idct(const int* blk, uint8_t* out, int stride) {
    int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    if (!((x1 = blk[8 * 4] << 8) | (x2 = blk[8 * 6]) | (x3 = blk[8 * 2]) |
          (x4 = blk[8 * 1]) | (x5 = blk[8 * 7]) | (x6 = blk[8 * 5]) | (x7 = blk[8 * 3]))) {
        x1 = clip8(((blk[0] + 32) >> 6) + 128);
        for (x0 = 0; x0 < 8; x0++) out[x0 * stride] = (uint8_t)x1;
        return;
    }
    x0 = (blk[0] << 8) + 8192;
    x8 = W7 * (x4 + x5) + 4;
    x4 = (x8 + (W1 - W7) * x4) >> 3;
    x5 = (x8 - (W1 + W7) * x5) >> 3;
    x8 = W3 * (x6 + x7) + 4;
    x6 = (x8 - (W3 - W5) * x6) >> 3;
    x7 = (x8 - (W3 + W5) * x7) >> 3;
    x8 = x0 + x1; x0 -= x1;
    x1 = W6 * (x3 + x2) + 4;
    x2 = (x1 - (W2 + W6) * x2) >> 3;
    x3 = (x1 + (W2 - W6) * x3) >> 3;
    x1 = x4 + x6; x4 -= x6;
    x6 = x5 + x7; x5 -= x7;
    x7 = x8 + x3; x8 -= x3;
    x3 = x0 + x2; x0 -= x2;
    x2 = (181 * (x4 + x5) + 128) >> 8;
    x4 = (181 * (x4 - x5) + 128) >> 8;
    out[0 * stride] = (uint8_t)clip8(((x7 + x1) >> 14) + 128);
    out[1 * stride] = (uint8_t)clip8(((x3 + x2) >> 14) + 128);
    out[2 * stride] = (uint8_t)clip8(((x0 + x4) >> 14) + 128);
    out[3 * stride] = (uint8_t)clip8(((x8 + x6) >> 14) + 128);
    out[4 * stride] = (uint8_t)clip8(((x8 - x6) >> 14) + 128);
    out[5 * stride] = (uint8_t)clip8(((x0 - x4) >> 14) + 128);
    out[6 * stride] = (uint8_t)clip8(((x3 - x2) >> 14) + 128);
    out[7 * stride] = (uint8_t)clip8(((x7 - x1) >> 14) + 128);
}

// ===========================================================================
// Marker / segment parsing
// ===========================================================================
static int rd16(jpeg_t* j) {
    if (j->pos + 2 > j->size) return -1;
    int v = (j->data[j->pos] << 8) | j->data[j->pos + 1];
    j->pos += 2;
    return v;
}

static void build_huff(huff_t* h) {
    int code = 0, k = 0;
    h->maxlen = 0;
    for (int l = 1; l <= 16; l++) {
        h->first_code[l] = code;
        h->sym_index[l]  = k;
        if (h->counts[l]) h->maxlen = l;
        code = (code + h->counts[l]) << 1;
        k += h->counts[l];
    }
}

// ===========================================================================
// Entropy bit reader
// ===========================================================================
static int getbit(jpeg_t* j) {
    if (j->bitcnt == 0) {
        if (j->marker) return 0;               // past a marker: feed zeros
        if (j->pos >= j->size) { j->marker = 0xD9; return 0; }
        int b = j->data[j->pos++];
        if (b == 0xFF) {
            int b2 = (j->pos < j->size) ? j->data[j->pos] : 0xD9;
            if (b2 == 0) {
                j->pos++;                      // stuffed 0xFF00 -> literal 0xFF
            } else {
                j->marker = b2;                // a real marker: stop consuming
                return 0;
            }
        }
        j->bitbuf = b;
        j->bitcnt = 8;
    }
    j->bitcnt--;
    return (j->bitbuf >> j->bitcnt) & 1;
}

static int getbits(jpeg_t* j, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) v = (v << 1) | getbit(j);
    return v;
}

static int receive_extend(jpeg_t* j, int s) {
    if (s == 0) return 0;
    int v = getbits(j, s);
    if (v < (1 << (s - 1))) v -= (1 << s) - 1;   // sign-extend (avoids UB shift)
    return v;
}

static int huff_decode(jpeg_t* j, const huff_t* h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | getbit(j);
        if (h->counts[l] && (code - h->first_code[l]) < h->counts[l]) {
            return h->symbols[h->sym_index[l] + (code - h->first_code[l])];
        }
    }
    return -1;
}

// Decode one 8x8 block, dequantize, IDCT into `out` (native plane).
static int decode_block(jpeg_t* j, comp_t* c, uint8_t* out, int stride) {
    int blk[64];
    memset(blk, 0, sizeof(blk));

    // DC
    int t = huff_decode(j, &j->huff[c->dctab]);
    if (t < 0) return -1;
    int diff = receive_extend(j, t);
    c->dcpred += diff;
    blk[0] = c->dcpred * j->qtab[c->qtsel][0];

    // AC
    int k = 1;
    while (k < 64) {
        int rs = huff_decode(j, &j->huff[4 + c->actab]);
        if (rs < 0) return -1;
        int r = rs >> 4, s = rs & 15;
        if (s == 0) {
            if (r != 15) break;                // EOB
            k += 16;
            continue;
        }
        k += r;
        if (k >= 64) break;
        int coeff = receive_extend(j, s) * j->qtab[c->qtsel][k];
        blk[zz[k]] = coeff;
        k++;
    }

    for (int i = 0; i < 8; i++) row_idct(&blk[i * 8]);
    for (int i = 0; i < 8; i++) col_idct(&blk[i], out + i, stride);
    return 0;
}

// ===========================================================================
// Scan
// ===========================================================================
static int decode_scan(jpeg_t* j) {
    j->bitcnt = 0; j->marker = 0;
    int rst_left = j->restart_interval;

    for (int my = 0; my < j->mcu_rows; my++) {
        for (int mx = 0; mx < j->mcu_cols; mx++) {
            for (int ci = 0; ci < j->ncomp; ci++) {
                comp_t* c = &j->comp[ci];
                for (int sy = 0; sy < c->ssy; sy++) {
                    for (int sx = 0; sx < c->ssx; sx++) {
                        int px = (mx * c->ssx + sx) * 8;
                        int py = (my * c->ssy + sy) * 8;
                        uint8_t* out = c->plane + py * c->pstride + px;
                        if (decode_block(j, c, out, c->pstride) < 0) return -1;
                    }
                }
            }

            if (j->restart_interval && --rst_left == 0 &&
                !(my == j->mcu_rows - 1 && mx == j->mcu_cols - 1)) {
                // Align to the next byte and consume the RSTn marker.
                j->bitcnt = 0;
                if (!j->marker) {
                    // find the marker in the stream
                    while (j->pos + 1 < j->size &&
                           !(j->data[j->pos] == 0xFF && j->data[j->pos + 1] >= 0xD0 &&
                             j->data[j->pos + 1] <= 0xD7)) {
                        j->pos++;
                    }
                    if (j->pos + 1 < j->size) j->marker = j->data[j->pos + 1];
                }
                if (j->marker >= 0xD0 && j->marker <= 0xD7) {
                    j->pos += 2;               // skip FF Dn
                    j->marker = 0;
                }
                for (int ci = 0; ci < j->ncomp; ci++) j->comp[ci].dcpred = 0;
                rst_left = j->restart_interval;
            }
        }
    }
    return 0;
}

// ===========================================================================
// Upsample + colour convert
// ===========================================================================
static int finish_image(jpeg_t* j, image_t* img) {
    int W = j->width, H = j->height;
    uint32_t* out = (uint32_t*)kmalloc((unsigned)W * H * 4);
    if (!out) return -1;

    // Max sampling, to map component planes to full resolution.
    int hmax = 1, vmax = 1;
    for (int ci = 0; ci < j->ncomp; ci++) {
        if (j->comp[ci].ssx > hmax) hmax = j->comp[ci].ssx;
        if (j->comp[ci].ssy > vmax) vmax = j->comp[ci].ssy;
    }

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int Y, Cb = 128, Cr = 128;
            {
                comp_t* c = &j->comp[0];
                int cx = x * c->ssx / hmax;
                int cy = y * c->ssy / vmax;
                Y = c->plane[cy * c->pstride + cx];
            }
            if (j->ncomp == 3) {
                comp_t* cb = &j->comp[1];
                comp_t* cr = &j->comp[2];
                int bx = x * cb->ssx / hmax, by = y * cb->ssy / vmax;
                int rx = x * cr->ssx / hmax, ry = y * cr->ssy / vmax;
                Cb = cb->plane[by * cb->pstride + bx];
                Cr = cr->plane[ry * cr->pstride + rx];
            }

            int r, g, b;
            if (j->ncomp == 1) {
                r = g = b = Y;
            } else {
                int cb = Cb - 128, cr = Cr - 128;
                r = clip8(Y + ((91881 * cr) >> 16));
                g = clip8(Y - ((22554 * cb + 46802 * cr) >> 16));
                b = clip8(Y + ((116130 * cb) >> 16));
            }
            out[y * W + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }

    img->width = W;
    img->height = H;
    img->pixels = out;
    return 0;
}

// ===========================================================================
// Top level
// ===========================================================================
int jpeg_decode(const uint8_t* data, unsigned len, image_t* img) {
    if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) return -1;   // SOI

    jpeg_t* j = (jpeg_t*)kzalloc(sizeof(jpeg_t));
    if (!j) return -1;
    j->data = data; j->size = (int)len; j->pos = 2;

    int rc = -1;
    for (;;) {
        if (j->pos + 2 > j->size) goto done;
        if (j->data[j->pos] != 0xFF) { j->pos++; continue; }
        int marker = j->data[j->pos + 1];
        j->pos += 2;
        if (marker == 0xD9) break;                          // EOI
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;

        int seg_len = rd16(j);
        if (seg_len < 2) goto done;
        int seg_start = j->pos;
        int seg_end = seg_start + seg_len - 2;
        if (seg_end > j->size) goto done;

        if (marker == 0xC0) {                               // SOF0 baseline
            int prec = j->data[j->pos];
            j->height = (j->data[j->pos + 1] << 8) | j->data[j->pos + 2];
            j->width  = (j->data[j->pos + 3] << 8) | j->data[j->pos + 4];
            j->ncomp  = j->data[j->pos + 5];
            if (prec != 8 || (j->ncomp != 1 && j->ncomp != 3)) goto done;
            if (j->width <= 0 || j->height <= 0) goto done;
            if ((long)j->width * j->height > JPEG_MAX_PIXELS) goto done;
            const uint8_t* p = j->data + j->pos + 6;
            int hmax = 1, vmax = 1;
            for (int i = 0; i < j->ncomp; i++) {
                j->comp[i].cid  = p[0];
                j->comp[i].ssx  = p[1] >> 4;
                j->comp[i].ssy  = p[1] & 15;
                j->comp[i].qtsel = p[2];
                if (j->comp[i].ssx < 1 || j->comp[i].ssy < 1) goto done;
                if (j->comp[i].ssx > hmax) hmax = j->comp[i].ssx;
                if (j->comp[i].ssy > vmax) vmax = j->comp[i].ssy;
                p += 3;
            }
            j->mcu_w = hmax * 8;
            j->mcu_h = vmax * 8;
            j->mcu_cols = (j->width  + j->mcu_w - 1) / j->mcu_w;
            j->mcu_rows = (j->height + j->mcu_h - 1) / j->mcu_h;
            for (int i = 0; i < j->ncomp; i++) {
                comp_t* c = &j->comp[i];
                c->pw = j->mcu_cols * c->ssx * 8;
                c->ph = j->mcu_rows * c->ssy * 8;
                c->pstride = c->pw;
                c->plane = (uint8_t*)kmalloc((unsigned)c->pw * c->ph);
                if (!c->plane) goto done;
            }
        } else if (marker == 0xC2) {                        // SOF2 progressive
            goto done;                                       // unsupported
        } else if (marker == 0xC4) {                        // DHT
            const uint8_t* p = j->data + j->pos;
            const uint8_t* pe = j->data + seg_end;
            while (p < pe) {
                int tc = p[0] >> 4, th = p[0] & 15;
                if (th > 3) goto done;
                int idx = (tc ? 4 : 0) + th;
                huff_t* h = &j->huff[idx];
                p++;
                int total = 0;
                for (int l = 1; l <= 16; l++) { h->counts[l] = p[l - 1]; total += h->counts[l]; }
                p += 16;
                if (total > 256 || p + total > pe) goto done;
                for (int i = 0; i < total; i++) h->symbols[i] = p[i];
                p += total;
                build_huff(h);
            }
        } else if (marker == 0xDB) {                        // DQT
            const uint8_t* p = j->data + j->pos;
            const uint8_t* pe = j->data + seg_end;
            while (p < pe) {
                int pq = p[0] >> 4, tq = p[0] & 15;
                if (tq > 3) goto done;
                p++;
                for (int i = 0; i < 64; i++) {
                    if (pq) { j->qtab[tq][i] = (p[0] << 8) | p[1]; p += 2; }
                    else    { j->qtab[tq][i] = p[0]; p += 1; }
                }
            }
        } else if (marker == 0xDD) {                        // DRI
            j->restart_interval = (j->data[j->pos] << 8) | j->data[j->pos + 1];
        } else if (marker == 0xDA) {                        // SOS
            const uint8_t* p = j->data + j->pos;
            int ns = p[0]; p++;
            if (ns != j->ncomp) goto done;
            for (int i = 0; i < ns; i++) {
                int cid = p[0], td = p[1] >> 4, ta = p[1] & 15;
                p += 2;
                for (int k = 0; k < j->ncomp; k++) {
                    if (j->comp[k].cid == cid) { j->comp[k].dctab = td; j->comp[k].actab = ta; }
                }
            }
            // p now at Ss, Se, Ah/Al (3 bytes) -- ignored for baseline.
            j->pos = seg_end;                                // entropy data follows
            if (decode_scan(j) < 0) goto done;
            rc = finish_image(j, img);
            goto done;
        }
        j->pos = seg_end;
    }

done:
    for (int i = 0; i < 3; i++) if (j->comp[i].plane) kfree(j->comp[i].plane);
    kfree(j);
    return rc;
}
