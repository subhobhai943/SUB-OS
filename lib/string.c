#include <lib/string.h>
#include <mm/kmalloc.h>

void* memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    while (count--) {
        *ptr++ = (uint8_t)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count--) {
            *(--d) = *(--s);
        }
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    while (num--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

int bcmp(const void* ptr1, const void* ptr2, size_t num) {
    return memcmp(ptr1, ptr2, num);
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str && str[len]) {
        len++;
    }
    return len;
}

int strcmp(const char* str1, const char* str2) {
    if (!str1 || !str2) return (str1 == str2) ? 0 : (str1 ? 1 : -1);
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int strncmp(const char* str1, const char* str2, size_t num) {
    if (num == 0) return 0;
    while (num > 0 && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
        num--;
    }
    if (num == 0) return 0;
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;
    while ((*dest++ = *src++)) {
    }
    return orig_dest;
}

char* strncpy(char* dest, const char* src, size_t num) {
    size_t i;
    for (i = 0; i < num && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < num; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* rd = dest;
    while (*rd) rd++;
    while ((*rd++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, size_t num) {
    char* rd = dest;
    while (*rd) rd++;
    while (num-- && (*rd++ = *src++));
    *rd = '\0';
    return dest;
}

char* strchr(const char* str, int c) {
    if (!str) return NULL;
    while (*str) {
        if (*str == (char)c) return (char*)str;
        str++;
    }
    if ((char)c == '\0') return (char*)str;
    return NULL;
}

char* strrchr(const char* str, int c) {
    if (!str) return NULL;
    const char* last = NULL;
    while (*str) {
        if (*str == (char)c) last = str;
        str++;
    }
    if ((char)c == '\0') return (char*)str;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;

    size_t needle_len = strlen(needle);
    while (*haystack) {
        if (*haystack == *needle && strncmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    return NULL;
}

char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)kmalloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static void strreverse(char* begin, char* end) {
    char aux;
    while (end > begin) {
        aux = *end;
        *end-- = *begin;
        *begin++ = aux;
    }
}

char* itoa(int value, char* str, int base) {
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    char* ptr = str, *ptr1 = str;
    int tmp_value;

    if (value < 0 && base == 10) {
        value = -value;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    if (tmp_value < 0 && base == 10) {
        *ptr++ = '-';
    }
    *ptr-- = '\0';

    strreverse(ptr1, ptr);
    return str;
}

char* itoa_hex(uint64_t value, char* str) {
    char* ptr = str;
    char* ptr1 = str;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }

    while (value > 0) {
        uint8_t mod = value % 16;
        *ptr++ = "0123456789ABCDEF"[mod];
        value /= 16;
    }

    *ptr-- = '\0';
    strreverse(ptr1, ptr);
    return str;
}

int atoi(const char* str) {
    int res = 0;
    int sign = 1;
    while (*str == ' ' || *str == '\t') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

long strtol(const char* str, char** endptr, int base) {
    if (!str) return 0;
    while (*str == ' ' || *str == '\t') str++;
    int sign = 1;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }

    if (base == 0) {
        if (*str == '0' && (*(str+1) == 'x' || *(str+1) == 'X')) {
            base = 16;
            str += 2;
        } else if (*str == '0') {
            base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16 && *str == '0' && (*(str+1) == 'x' || *(str+1) == 'X')) {
        str += 2;
    }

    long result = 0;
    while (*str) {
        int val = -1;
        if (*str >= '0' && *str <= '9') val = *str - '0';
        else if (*str >= 'a' && *str <= 'z') val = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'Z') val = *str - 'A' + 10;
        if (val < 0 || val >= base) break;
        result = result * base + val;
        str++;
    }
    if (endptr) *endptr = (char*)str;
    return result * sign;
}
