#include <lib/printf.h>
#include <lib/string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

static int format_number(char* buf, size_t size, uint64_t num, int base, bool is_signed, bool upper, int width, char pad, bool left_align) {
    char temp[65];
    int pos = 0;
    bool negative = false;

    if (is_signed && (int64_t)num < 0) {
        negative = true;
        num = (uint64_t)(-(int64_t)num);
    }

    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (num == 0) {
        temp[pos++] = '0';
    } else {
        while (num > 0) {
            temp[pos++] = digits[num % base];
            num /= base;
        }
    }

    if (negative) {
        temp[pos++] = '-';
    }

    int total_len = pos;
    int pad_count = (width > total_len) ? (width - total_len) : 0;
    int written = 0;

    // Right-aligned padding
    if (!left_align) {
        while (pad_count > 0 && (size_t)written < size - 1) {
            buf[written++] = pad;
            pad_count--;
        }
    }

    // Write number in reverse
    for (int i = pos - 1; i >= 0 && (size_t)written < size - 1; i--) {
        buf[written++] = temp[i];
    }

    // Left-aligned padding
    if (left_align) {
        while (pad_count > 0 && (size_t)written < size - 1) {
            buf[written++] = ' ';
            pad_count--;
        }
    }

    return written;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    if (!buf || size == 0) return 0;

    size_t out_idx = 0;

    for (size_t i = 0; fmt[i] != '\0' && out_idx < size - 1; i++) {
        if (fmt[i] != '%') {
            buf[out_idx++] = fmt[i];
            continue;
        }

        i++; // Skip '%'
        if (fmt[i] == '\0') break;

        // Flags
        bool left_align = false;
        if (fmt[i] == '-') {
            left_align = true;
            i++;
        }

        char pad = ' ';
        if (fmt[i] == '0' && !left_align) {
            pad = '0';
            i++;
        }

        // Width
        int width = 0;
        while (fmt[i] >= '0' && fmt[i] <= '9') {
            width = width * 10 + (fmt[i] - '0');
            i++;
        }

        // Length modifiers
        bool is_long = false;
        bool is_long_long = false;
        if (fmt[i] == 'l') {
            is_long = true;
            i++;
            if (fmt[i] == 'l') {
                is_long_long = true;
                i++;
            }
        }

        switch (fmt[i]) {
            case 'c': {
                char c = (char)va_arg(args, int);
                if (out_idx < size - 1) buf[out_idx++] = c;
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                size_t s_len = strlen(s);
                int pad_count = (width > (int)s_len) ? (width - (int)s_len) : 0;

                if (!left_align) {
                    while (pad_count > 0 && out_idx < size - 1) {
                        buf[out_idx++] = ' ';
                        pad_count--;
                    }
                }

                while (*s && out_idx < size - 1) {
                    buf[out_idx++] = *s++;
                }

                if (left_align) {
                    while (pad_count > 0 && out_idx < size - 1) {
                        buf[out_idx++] = ' ';
                        pad_count--;
                    }
                }
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_long_long ? va_arg(args, int64_t) : (is_long ? va_arg(args, long) : va_arg(args, int));
                out_idx += format_number(buf + out_idx, size - out_idx, (uint64_t)val, 10, true, false, width, pad, left_align);
                break;
            }
            case 'u': {
                uint64_t val = is_long_long ? va_arg(args, uint64_t) : (is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
                out_idx += format_number(buf + out_idx, size - out_idx, val, 10, false, false, width, pad, left_align);
                break;
            }
            case 'x': {
                uint64_t val = is_long_long ? va_arg(args, uint64_t) : (is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
                out_idx += format_number(buf + out_idx, size - out_idx, val, 16, false, false, width, pad, left_align);
                break;
            }
            case 'X': {
                uint64_t val = is_long_long ? va_arg(args, uint64_t) : (is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
                out_idx += format_number(buf + out_idx, size - out_idx, val, 16, false, true, width, pad, left_align);
                break;
            }
            case 'p': {
                uint64_t ptr = (uint64_t)va_arg(args, void*);
                if (out_idx < size - 3) {
                    buf[out_idx++] = '0';
                    buf[out_idx++] = 'x';
                }
                out_idx += format_number(buf + out_idx, size - out_idx, ptr, 16, false, false, 16, '0', false);
                break;
            }
            case '%': {
                if (out_idx < size - 1) buf[out_idx++] = '%';
                break;
            }
            default:
                if (out_idx < size - 1) buf[out_idx++] = fmt[i];
                break;
        }
    }

    buf[out_idx] = '\0';
    return (int)out_idx;
}

int sprintf(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    return ret;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

int vsprintf(char* buf, const char* fmt, va_list args) {
    return vsnprintf(buf, 4096, fmt, args);
}
