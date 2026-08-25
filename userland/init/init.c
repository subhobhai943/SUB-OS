/*
 * SUB-OS /sbin/init -- the first program that runs outside the kernel.
 *
 * Built freestanding and static: no libc, no startup files, nothing shared
 * with the kernel image. Every service it uses arrives through INT 0x80, so
 * running it end to end proves the ELF loader, the user page tables, the ring
 * transition and the syscall gate all agree with each other.
 */

typedef unsigned long size_t;
typedef long ssize_t;

#define SYS_read    0
#define SYS_write   1
#define SYS_getpid  39
#define SYS_exit    60
#define SYS_brk     12

#define STDOUT 1

static long syscall3(long nr, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3)
                     : "memory", "rcx", "r11");
    return ret;
}

static long syscall1(long nr, long a1) {
    return syscall3(nr, a1, 0, 0);
}

static size_t ustrlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void write_str(const char* s) {
    syscall3(SYS_write, STDOUT, (long)s, (long)ustrlen(s));
}

static void write_dec(long value) {
    char buf[24];
    int i = (int)sizeof(buf);
    int negative = 0;

    if (value < 0) { negative = 1; value = -value; }
    buf[--i] = '\0';
    do {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && i > 1);
    if (negative && i > 0) buf[--i] = '-';

    write_str(&buf[i]);
}

static void write_hex(unsigned long value) {
    static const char digits[] = "0123456789abcdef";
    char buf[19];
    int i = (int)sizeof(buf);

    buf[--i] = '\0';
    do {
        buf[--i] = digits[value & 0xF];
        value >>= 4;
    } while (value && i > 2);
    buf[--i] = 'x';
    buf[--i] = '0';

    write_str(&buf[i]);
}

/* Lives in .data, so a writable PT_LOAD segment has to have been mapped. */
static char greeting[] = "SUB-OS userland is alive.\n";

/* Lives in .bss, so the loader has to have zero-filled the tail of a segment.
 * volatile keeps the compiler from folding the check away: without it gcc
 * proves the array is never written and drops it, taking .bss with it. */
static volatile char zero_probe[64];

void _start(void) {
    write_str("\n");
    write_str("=== /sbin/init (ring 3) ===\n");
    write_str(greeting);

    write_str("  pid            : ");
    write_dec(syscall1(SYS_getpid, 0));
    write_str("\n");

    write_str("  code address   : ");
    write_hex((unsigned long)(void*)_start);
    write_str("\n");

    write_str("  stack address  : ");
    unsigned long here;
    __asm__ volatile("mov %%rsp, %0" : "=r"(here));
    write_hex(here);
    write_str("\n");

    /* .bss must have arrived zeroed. */
    int bss_clean = 1;
    for (unsigned i = 0; i < sizeof(zero_probe); i++) {
        if (zero_probe[i] != 0) bss_clean = 0;
    }
    write_str("  .bss zeroed    : ");
    write_str(bss_clean ? "yes\n" : "NO\n");

    /* .data must be writable. */
    greeting[0] = 'S';
    write_str("  .data writable : yes\n");

    /* An unmapped user address must be rejected rather than served. */
    long bad = syscall3(SYS_write, STDOUT, (long)0x7fffffffffffUL, 8);
    write_str("  bad pointer    : ");
    write_str(bad < 0 ? "rejected\n" : "ACCEPTED (bug)\n");

    /* A kernel address must be rejected too, even though it is mapped. */
    long kern = syscall3(SYS_write, STDOUT, (long)0x100000UL, 8);
    write_str("  kernel pointer : ");
    write_str(kern < 0 ? "rejected\n" : "ACCEPTED (bug)\n");

    write_str("=== init exiting with status 0 ===\n\n");
    syscall1(SYS_exit, 0);

    /* SYS_exit does not return; spin defensively if it ever did. */
    for (;;) { }
}
