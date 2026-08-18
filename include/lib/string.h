#ifndef _LIB_STRING_H
#define _LIB_STRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/printf.h>

// Memory functions
void* memset(void* dest, int val, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memmove(void* dest, const void* src, size_t count);
int   memcmp(const void* ptr1, const void* ptr2, size_t num);

// String functions
size_t strlen(const char* str);
int    strcmp(const char* str1, const char* str2);
int    strncmp(const char* str1, const char* str2, size_t num);
char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t num);
char*  strcat(char* dest, const char* src);
char*  strncat(char* dest, const char* src, size_t num);
char*  strchr(const char* str, int c);
char*  strrchr(const char* str, int c);
char*  strstr(const char* haystack, const char* needle);
char*  strdup(const char* s);

// Conversion functions
char*  itoa(int value, char* str, int base);
char*  itoa_hex(uint64_t value, char* str);
int    atoi(const char* str);
long   strtol(const char* str, char** endptr, int base);

#endif // _LIB_STRING_H
