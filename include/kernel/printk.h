#ifndef _KERNEL_PRINTK_H
#define _KERNEL_PRINTK_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <lib/printf.h>

// Linux-compatible log level prefixes
#define KERN_EMERG   "\0010"  /* system is unusable */
#define KERN_ALERT   "\0011"  /* action must be taken immediately */
#define KERN_CRIT    "\0012"  /* critical conditions */
#define KERN_ERR     "\0013"  /* error conditions */
#define KERN_WARNING "\0014"  /* warning conditions */
#define KERN_NOTICE  "\0015"  /* normal but significant condition */
#define KERN_INFO    "\0016"  /* informational */
#define KERN_DEBUG   "\0017"  /* debug-level messages */

// ANSI Color Escape Sequences
#define ANSI_RESET         "\033[0m"
#define ANSI_BOLD          "\033[1m"
#define ANSI_BLACK         "\033[30m"
#define ANSI_RED           "\033[31m"
#define ANSI_GREEN         "\033[32m"
#define ANSI_YELLOW        "\033[33m"
#define ANSI_BLUE          "\033[34m"
#define ANSI_MAGENTA       "\033[35m"
#define ANSI_CYAN          "\033[36m"
#define ANSI_WHITE         "\033[37m"
#define ANSI_BRIGHT_BLACK  "\033[90m"
#define ANSI_BRIGHT_RED    "\033[91m"
#define ANSI_BRIGHT_GREEN  "\033[92m"
#define ANSI_BRIGHT_YELLOW "\033[93m"
#define ANSI_BRIGHT_BLUE   "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN   "\033[96m"
#define ANSI_BRIGHT_WHITE  "\033[97m"
#define ANSI_INVERT        "\033[7m"
#define ANSI_BG_BLACK      "\033[40m"
#define ANSI_BG_BLUE       "\033[44m"
#define ANSI_BG_WHITE      "\033[47m"

// Printk API
void printk_init(void);
int  printk(const char* fmt, ...);
int  vprintk(const char* fmt, va_list args);
int  kprintf(const char* fmt, ...); // Alias for compatibility

// Kernel Dmesg Ring Buffer API
void dmesg_dump(void);
const char* dmesg_get_buffer(size_t* size_out);

// ---------------------------------------------------------------------------
// Console redirection
//
// While a sink is installed, printk output is handed to the callback instead
// of the text console, so a caller can run a subsystem that reports via printk
// and capture what it printed. The serial console and the dmesg ring still
// receive everything, which keeps a captured run debuggable.
//
// The sink must not itself call printk; re-entrant output is dropped.
// ---------------------------------------------------------------------------
typedef void (*printk_sink_fn_t)(const char* text, size_t len, void* ctx);

void printk_set_sink(printk_sink_fn_t fn, void* ctx);
void printk_clear_sink(void);
bool printk_has_sink(void);

#endif // _KERNEL_PRINTK_H
