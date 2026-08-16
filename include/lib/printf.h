#ifndef _LIB_PRINTF_H
#define _LIB_PRINTF_H

#include <stddef.h>
#include <stdarg.h>

int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list args);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

#endif // _LIB_PRINTF_H
