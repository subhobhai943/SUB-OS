/*
 * Image decoder: PNG (8-bit grayscale / RGB / RGBA / palette, non-interlaced)
 * and BMP (24/32-bit uncompressed). Output is a flat 0xAARRGGBB buffer ready
 * for gui_gfx_blit, so the Web app can show pictures.
 *
 * PNG = parse chunks, concatenate IDAT, zlib-inflate, undo the per-scanline
 * filters, then expand to ARGB. All integer, heap-allocated (never on the
 * worker's small stack).
 */
#include <lib/image.h>
#include <lib/inflate.h>
#include <lib/jpeg.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

// Cap the pixel count so a hostile or huge image cannot exhaust the heap.
#define IMAGE_MAX_PIXELS (4 * 1024 * 1024)   // 4 Mpx -> 16 MB output max

static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };

bool image_sniff(const uint8_t* d, unsigned len) {
    if (len >= 8 && memcmp(d, png_sig, 8) == 0) return true;
    if (len >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return true;  // JPEG
    if (len >= 2 && d[0] == 'B' && d[1] == 'M') return true;
    return false;
}

void image_free(image_t* img) {
    if (img && img->pixels) { kfree(img->pixels); img->pixels = NULL; }
}

static inline uint32_t rd32be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
static inline uint32_t rd32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ===========================================================================
// PNG
// ===========================================================================
static int iabs(int x) { return x < 0 ? -x : x; }

static int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = iabs(p - a), pb = iabs(p - b), pc = iabs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static int decode_png(const uint8_t* data, unsigned len, image_t* img) {
    if (len < 8 + 25 || memcmp(data, png_sig, 8) != 0) return -1;

    unsigned pos = 8;
    int width = 0, height = 0, depth = 0, color = 0, interlace = 0;
    uint8_t palette[256][3];
    uint8_t trns[256];
    int have_plte = 0, plte_n = 0, have_trns = 0, trns_n = 0;
    memset(trns, 255, sizeof(trns));

    // Compressed IDAT is gathered into one buffer.
    uint8_t* idat = NULL;
    unsigned idat_len = 0, idat_cap = 0;

    for (;;) {
        if (pos + 8 > len) { if (idat) kfree(idat); return -1; }
        uint32_t clen = rd32be(data + pos);
        const uint8_t* ctype = data + pos + 4;
        const uint8_t* cdata = data + pos + 8;
        if (pos + 12 + clen > len) { if (idat) kfree(idat); return -1; }

        if (memcmp(ctype, "IHDR", 4) == 0) {
            if (clen < 13) { if (idat) kfree(idat); return -1; }
            width     = (int)rd32be(cdata);
            height    = (int)rd32be(cdata + 4);
            depth     = cdata[8];
            color     = cdata[9];
            interlace = cdata[12];
            if (width <= 0 || height <= 0) { if (idat) kfree(idat); return -1; }
            if ((long)width * height > IMAGE_MAX_PIXELS) { if (idat) kfree(idat); return -1; }
            if (interlace != 0) { if (idat) kfree(idat); return -1; } // no Adam7
            if (color != 0 && color != 2 && color != 3 && color != 4 && color != 6) {
                if (idat) kfree(idat);
                return -1;
            }
            // Grayscale (0) and palette (3) may use 1/2/4/8-bit samples; the
            // truecolor types are supported at 8-bit only (16-bit rejected).
            bool sub_ok = (color == 0 || color == 3) &&
                          (depth == 1 || depth == 2 || depth == 4 || depth == 8);
            if (depth != 8 && !sub_ok) { if (idat) kfree(idat); return -1; }
        } else if (memcmp(ctype, "PLTE", 4) == 0) {
            plte_n = clen / 3;
            if (plte_n > 256) plte_n = 256;
            for (int i = 0; i < plte_n; i++) {
                palette[i][0] = cdata[i * 3 + 0];
                palette[i][1] = cdata[i * 3 + 1];
                palette[i][2] = cdata[i * 3 + 2];
            }
            have_plte = 1;
        } else if (memcmp(ctype, "tRNS", 4) == 0) {
            trns_n = clen > 256 ? 256 : (int)clen;
            for (int i = 0; i < trns_n; i++) trns[i] = cdata[i];
            have_trns = 1;
        } else if (memcmp(ctype, "IDAT", 4) == 0) {
            if (idat_len + clen > idat_cap) {
                unsigned ncap = (idat_len + clen) * 2 + 1024;
                uint8_t* n = (uint8_t*)kmalloc(ncap);
                if (!n) { if (idat) kfree(idat); return -1; }
                if (idat) { memcpy(n, idat, idat_len); kfree(idat); }
                idat = n; idat_cap = ncap;
            }
            memcpy(idat + idat_len, cdata, clen);
            idat_len += clen;
        } else if (memcmp(ctype, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + clen;   // length + type + data + crc
    }

    if (!idat || width == 0) { if (idat) kfree(idat); return -1; }
    if (color == 3 && !have_plte) { kfree(idat); return -1; }

    int channels = (color == 0) ? 1 : (color == 2) ? 3 : (color == 3) ? 1 :
                   (color == 4) ? 2 : 4;
    int bits_pp = channels * depth;
    int bpp = (bits_pp + 7) / 8;                         // filter unit, >= 1 byte
    if (bpp < 1) bpp = 1;
    unsigned stride = ((unsigned)width * bits_pp + 7) / 8;
    unsigned raw_len = (stride + 1) * (unsigned)height;  // +1 filter byte per row

    uint8_t* raw = (uint8_t*)kmalloc(raw_len);
    if (!raw) { kfree(idat); return -1; }

    unsigned got = raw_len;
    int ir = inflate_zlib(raw, &got, idat, idat_len);
    kfree(idat);
    if (ir != 0 || got != raw_len) { kfree(raw); return -1; }

    // Unfilter each scanline in place, writing reconstructed bytes over the
    // filtered ones (the filter byte at the row start is consumed).
    uint8_t* unf = (uint8_t*)kmalloc(stride * (unsigned)height);
    if (!unf) { kfree(raw); return -1; }

    for (int y = 0; y < height; y++) {
        uint8_t filter = raw[y * (stride + 1)];
        const uint8_t* rowin = raw + y * (stride + 1) + 1;
        uint8_t* row  = unf + (unsigned)y * stride;
        uint8_t* prev = (y > 0) ? unf + (unsigned)(y - 1) * stride : NULL;

        for (unsigned x = 0; x < stride; x++) {
            int a = (x >= (unsigned)bpp) ? row[x - bpp] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= (unsigned)bpp) ? prev[x - bpp] : 0;
            int val = rowin[x];
            switch (filter) {
            case 0: break;                       // None
            case 1: val += a; break;             // Sub
            case 2: val += b; break;             // Up
            case 3: val += (a + b) / 2; break;   // Average
            case 4: val += paeth(a, b, c); break;// Paeth
            default: kfree(raw); kfree(unf); return -1;
            }
            row[x] = (uint8_t)val;
        }
    }
    kfree(raw);

    uint32_t* out = (uint32_t*)kmalloc((unsigned)width * height * 4);
    if (!out) { kfree(unf); return -1; }

    int maxv = (1 << depth) - 1;
    for (int y = 0; y < height; y++) {
        const uint8_t* row = unf + (unsigned)y * stride;
        uint32_t* dst = out + (unsigned)y * width;
        for (int x = 0; x < width; x++) {
            if (depth < 8) {
                // Grayscale or palette with sub-byte samples, MSB-first.
                int bitpos = x * depth;
                int sample = (row[bitpos >> 3] >> (8 - depth - (bitpos & 7))) & maxv;
                if (color == 3) {
                    int idx = sample;
                    if (idx >= plte_n) idx = 0;
                    uint8_t alpha = (have_trns && idx < trns_n) ? trns[idx] : 255;
                    dst[x] = argb(alpha, palette[idx][0], palette[idx][1], palette[idx][2]);
                } else {
                    uint8_t g = (uint8_t)(sample * 255 / maxv);
                    dst[x] = argb(255, g, g, g);
                }
                continue;
            }

            const uint8_t* px = row + x * bpp;
            switch (color) {
            case 0: dst[x] = argb(255, px[0], px[0], px[0]); break;          // gray
            case 4: dst[x] = argb(px[1], px[0], px[0], px[0]); break;        // gray+alpha
            case 2: dst[x] = argb(255, px[0], px[1], px[2]); break;          // RGB
            case 6: dst[x] = argb(px[3], px[0], px[1], px[2]); break;        // RGBA
            case 3: {                                                        // palette
                int idx = px[0];
                if (idx >= plte_n) idx = 0;
                uint8_t alpha = (have_trns && idx < trns_n) ? trns[idx] : 255;
                dst[x] = argb(alpha, palette[idx][0], palette[idx][1], palette[idx][2]);
                break;
            }
            }
        }
    }
    kfree(unf);

    img->width = width;
    img->height = height;
    img->pixels = out;
    return 0;
}

// ===========================================================================
// BMP (24/32-bit uncompressed, BI_RGB)
// ===========================================================================
static int decode_bmp(const uint8_t* data, unsigned len, image_t* img) {
    if (len < 54 || data[0] != 'B' || data[1] != 'M') return -1;
    uint32_t offset = rd32le(data + 10);
    uint32_t dib    = rd32le(data + 14);
    int width  = (int)rd32le(data + 18);
    int height = (int)rd32le(data + 22);
    int bpp    = data[28] | (data[29] << 8);
    uint32_t compression = rd32le(data + 30);
    if (dib < 40 || compression != 0) return -1;
    if (bpp != 24 && bpp != 32) return -1;

    bool top_down = height < 0;
    if (top_down) height = -height;
    if (width <= 0 || height <= 0) return -1;
    if ((long)width * height > IMAGE_MAX_PIXELS) return -1;

    int bytes_pp = bpp / 8;
    unsigned row_size = (((unsigned)width * bytes_pp + 3) & ~3u);
    if (offset + row_size * (unsigned)height > len) return -1;

    uint32_t* out = (uint32_t*)kmalloc((unsigned)width * height * 4);
    if (!out) return -1;

    for (int y = 0; y < height; y++) {
        int srow = top_down ? y : (height - 1 - y);
        const uint8_t* row = data + offset + (unsigned)srow * row_size;
        uint32_t* dst = out + (unsigned)y * width;
        for (int x = 0; x < width; x++) {
            const uint8_t* px = row + x * bytes_pp;
            uint8_t a = (bytes_pp == 4) ? px[3] : 255;
            dst[x] = argb(a, px[2], px[1], px[0]);   // BMP is BGR(A)
        }
    }

    img->width = width;
    img->height = height;
    img->pixels = out;
    return 0;
}

// ===========================================================================
int image_decode(const uint8_t* data, unsigned len, image_t* img) {
    if (!data || !img) return -1;
    img->width = img->height = 0;
    img->pixels = NULL;

    if (len >= 8 && memcmp(data, png_sig, 8) == 0) return decode_png(data, len, img);
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return jpeg_decode(data, len, img);
    if (len >= 2 && data[0] == 'B' && data[1] == 'M') return decode_bmp(data, len, img);
    return -1;
}
