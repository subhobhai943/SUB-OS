// SUB-OS LazyBox Text Processing Suite
//
// GNU coreutils-compatible line and byte oriented tools operating over the
// VFS: sort, uniq, cut, tr, rev, tac, nl, seq, diff, xxd, du, factor, sum
// and truncate. Every tool that reads a file streams it into a bounded line
// table so a pathological input cannot exhaust the kernel heap.

#include <userland/coreutils.h>
#include <fs/vfs.h>
#include <kernel/printk.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <stdint.h>
#include <stdbool.h>

#define CU_MAX_LINES     512
#define CU_MAX_LINE_LEN  256
#define CU_MAX_FILE_SIZE (128 * 1024)

typedef struct {
    char** lines;
    int    count;
    char*  raw;
} cu_text_t;

static void cu_free_text(cu_text_t* t) {
    if (!t) return;
    if (t->lines) kfree(t->lines);
    if (t->raw) kfree(t->raw);
    t->lines = NULL;
    t->raw = NULL;
    t->count = 0;
}

// Slurp a file and split it in place on newlines. Returns false (and prints a
// diagnostic) when the path cannot be read.
static bool cu_read_text(const char* path, cu_text_t* out) {
    out->lines = NULL;
    out->raw = NULL;
    out->count = 0;

    vfs_node_t* node = vfs_namei(path);
    if (!node) {
        printk(ANSI_RED "cannot open '%s': No such file or directory\n" ANSI_RESET, path);
        return false;
    }
    if (node->flags & FS_DIRECTORY) {
        printk(ANSI_RED "'%s' is a directory\n" ANSI_RESET, path);
        return false;
    }

    // Synthetic procfs/sysfs nodes report length 0 and materialise their
    // contents only on read, so fall back to a fixed window for those.
    size_t len = node->length ? node->length : 8192;
    if (len > CU_MAX_FILE_SIZE) len = CU_MAX_FILE_SIZE;

    char* buf = (char*)kmalloc(len + 2);
    if (!buf) {
        printk(ANSI_RED "out of memory reading '%s'\n" ANSI_RESET, path);
        return false;
    }

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        kfree(buf);
        printk(ANSI_RED "cannot open '%s'\n" ANSI_RESET, path);
        return false;
    }

    ssize_t got = vfs_read(fd, buf, len);
    vfs_close(fd);
    if (got < 0) got = 0;
    buf[got] = '\0';

    char** lines = (char**)kzalloc(CU_MAX_LINES * sizeof(char*));
    if (!lines) {
        kfree(buf);
        return false;
    }

    int count = 0;
    char* cursor = buf;
    lines[count++] = cursor;

    for (ssize_t i = 0; i < got && count < CU_MAX_LINES; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            if (i + 1 < got) lines[count++] = &buf[i + 1];
        }
    }

    // A trailing newline produces one empty final line; drop it.
    if (count > 0 && lines[count - 1][0] == '\0' && got > 0 && buf[got - 1] == '\0') {
        count--;
    }

    out->raw = buf;
    out->lines = lines;
    out->count = count;
    return true;
}

static int cu_atoi(const char* s, int fallback) {
    if (!s || !*s) return fallback;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    if (*s < '0' || *s > '9') return fallback;

    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v * sign;
}

static bool cu_has_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; i++) {
        if (argv[i] && strcmp(argv[i], flag) == 0) return true;
    }
    return false;
}

// First argument that is not a flag and not the value of a flag.
static const char* cu_first_operand(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-') return argv[i];
    }
    return NULL;
}

// ===========================================================================
// sort
// ===========================================================================
static int cu_strcmp_ci(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int coreutils_sort(int argc, char** argv) {
    const char* path = cu_first_operand(argc, argv);
    if (!path) {
        printk("Usage: sort [-r] [-n] [-f] [-u] <file>\n");
        printk("  -r  reverse order    -n  numeric sort\n");
        printk("  -f  ignore case      -u  unique lines only\n");
        return 1;
    }

    bool reverse = cu_has_flag(argc, argv, "-r");
    bool numeric = cu_has_flag(argc, argv, "-n");
    bool fold    = cu_has_flag(argc, argv, "-f");
    bool unique  = cu_has_flag(argc, argv, "-u");

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    // Insertion sort: input is capped at CU_MAX_LINES, and it is stable,
    // which `sort -u` relies on to keep the first of each duplicate run.
    for (int i = 1; i < t.count; i++) {
        char* key = t.lines[i];
        int j = i - 1;
        while (j >= 0) {
            int cmp;
            if (numeric) {
                int a = cu_atoi(t.lines[j], 0);
                int b = cu_atoi(key, 0);
                cmp = (a > b) - (a < b);
            } else if (fold) {
                cmp = cu_strcmp_ci(t.lines[j], key);
            } else {
                cmp = strcmp(t.lines[j], key);
            }
            if (reverse) cmp = -cmp;
            if (cmp <= 0) break;
            t.lines[j + 1] = t.lines[j];
            j--;
        }
        t.lines[j + 1] = key;
    }

    const char* prev = NULL;
    for (int i = 0; i < t.count; i++) {
        if (unique && prev && strcmp(prev, t.lines[i]) == 0) continue;
        printk("%s\n", t.lines[i]);
        prev = t.lines[i];
    }

    cu_free_text(&t);
    return 0;
}

// ===========================================================================
// uniq
// ===========================================================================
int coreutils_uniq(int argc, char** argv) {
    const char* path = cu_first_operand(argc, argv);
    if (!path) {
        printk("Usage: uniq [-c] [-d] [-u] <file>\n");
        printk("  -c  prefix each line with its repeat count\n");
        printk("  -d  print only duplicated lines\n");
        printk("  -u  print only unique lines\n");
        return 1;
    }

    bool show_count = cu_has_flag(argc, argv, "-c");
    bool dups_only  = cu_has_flag(argc, argv, "-d");
    bool uniq_only  = cu_has_flag(argc, argv, "-u");

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    int i = 0;
    while (i < t.count) {
        int run = 1;
        while (i + run < t.count && strcmp(t.lines[i], t.lines[i + run]) == 0) run++;

        bool emit = true;
        if (dups_only && run < 2) emit = false;
        if (uniq_only && run > 1) emit = false;

        if (emit) {
            if (show_count) printk("%7d %s\n", run, t.lines[i]);
            else            printk("%s\n", t.lines[i]);
        }
        i += run;
    }

    cu_free_text(&t);
    return 0;
}

// ===========================================================================
// cut
// ===========================================================================
int coreutils_cut(int argc, char** argv) {
    const char* path = NULL;
    char delim = '\t';
    int  field = 0;
    int  byte_start = 0, byte_end = 0;

    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delim = argv[++i][0];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            field = cu_atoi(argv[++i], 1);
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            const char* spec = argv[++i];
            byte_start = cu_atoi(spec, 1);
            const char* dash = strchr(spec, '-');
            byte_end = dash ? cu_atoi(dash + 1, 0) : byte_start;
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }

    if (!path || (field == 0 && byte_start == 0)) {
        printk("Usage: cut -f <n> [-d <char>] <file>\n");
        printk("       cut -b <start[-end]> <file>\n");
        return 1;
    }

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    for (int i = 0; i < t.count; i++) {
        const char* line = t.lines[i];

        if (byte_start > 0) {
            int len = (int)strlen(line);
            int from = byte_start - 1;
            int to = (byte_end > 0) ? byte_end : len;
            if (to > len) to = len;
            for (int b = from; b < to; b++) printk("%c", line[b]);
            printk("\n");
            continue;
        }

        // Field mode: walk the delimiter runs and print the requested column.
        int current = 1;
        const char* start = line;
        const char* p = line;
        bool printed = false;

        while (1) {
            if (*p == delim || *p == '\0') {
                if (current == field) {
                    for (const char* q = start; q < p; q++) printk("%c", *q);
                    printed = true;
                    break;
                }
                if (*p == '\0') break;
                current++;
                start = p + 1;
            }
            if (*p == '\0') break;
            p++;
        }
        if (!printed && current < field) {
            // Missing field: GNU cut emits an empty line.
        }
        printk("\n");
    }

    cu_free_text(&t);
    return 0;
}

// ===========================================================================
// tr
// ===========================================================================
int coreutils_tr(int argc, char** argv) {
    if (argc < 3) {
        printk("Usage: tr <set1> <set2> <file>       translate characters\n");
        printk("       tr -d <set1> <file>           delete characters\n");
        return 1;
    }

    bool del = (strcmp(argv[1], "-d") == 0);
    const char* set1 = del ? argv[2] : argv[1];
    const char* set2 = del ? NULL : argv[2];
    const char* path = del ? (argc > 3 ? argv[3] : NULL)
                           : (argc > 3 ? argv[3] : NULL);

    if (!path) {
        printk(ANSI_RED "tr: missing file operand\n" ANSI_RESET);
        return 1;
    }

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    size_t set1_len = strlen(set1);
    size_t set2_len = set2 ? strlen(set2) : 0;

    for (int i = 0; i < t.count; i++) {
        for (const char* p = t.lines[i]; *p; p++) {
            const char* found = strchr(set1, *p);
            if (!found) {
                printk("%c", *p);
                continue;
            }
            if (del) continue;

            // Short set2 repeats its final character, as GNU tr does.
            size_t idx = (size_t)(found - set1);
            if (idx >= set2_len) idx = set2_len ? set2_len - 1 : 0;
            if (set2_len) printk("%c", set2[idx]);
        }
        printk("\n");
    }

    (void)set1_len;
    cu_free_text(&t);
    return 0;
}

// ===========================================================================
// rev / tac / nl
// ===========================================================================
int coreutils_rev(int argc, char** argv) {
    const char* path = cu_first_operand(argc, argv);
    if (!path) {
        printk("Usage: rev <file>    reverse each line character-wise\n");
        return 1;
    }

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    for (int i = 0; i < t.count; i++) {
        int len = (int)strlen(t.lines[i]);
        for (int c = len - 1; c >= 0; c--) printk("%c", t.lines[i][c]);
        printk("\n");
    }

    cu_free_text(&t);
    return 0;
}

int coreutils_tac(int argc, char** argv) {
    const char* path = cu_first_operand(argc, argv);
    if (!path) {
        printk("Usage: tac <file>    print lines in reverse order\n");
        return 1;
    }

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    for (int i = t.count - 1; i >= 0; i--) printk("%s\n", t.lines[i]);

    cu_free_text(&t);
    return 0;
}

int coreutils_nl(int argc, char** argv) {
    const char* path = cu_first_operand(argc, argv);
    if (!path) {
        printk("Usage: nl [-a] <file>   number lines (-a numbers blank lines too)\n");
        return 1;
    }

    bool number_all = cu_has_flag(argc, argv, "-a");

    cu_text_t t;
    if (!cu_read_text(path, &t)) return 1;

    int n = 1;
    for (int i = 0; i < t.count; i++) {
        bool blank = (t.lines[i][0] == '\0');
        if (blank && !number_all) {
            printk("\n");
            continue;
        }
        printk(ANSI_BRIGHT_BLACK "%6d  " ANSI_RESET "%s\n", n++, t.lines[i]);
    }

    cu_free_text(&t);
    return 0;
}

// ===========================================================================
// seq
// ===========================================================================
int coreutils_seq(int argc, char** argv) {
    if (argc < 2) {
        printk("Usage: seq [first [step]] last\n");
        return 1;
    }

    int first = 1, step = 1, last;
    if (argc == 2) {
        last = cu_atoi(argv[1], 1);
    } else if (argc == 3) {
        first = cu_atoi(argv[1], 1);
        last  = cu_atoi(argv[2], 1);
    } else {
        first = cu_atoi(argv[1], 1);
        step  = cu_atoi(argv[2], 1);
        last  = cu_atoi(argv[3], 1);
    }

    if (step == 0) {
        printk(ANSI_RED "seq: step must not be zero\n" ANSI_RESET);
        return 1;
    }

    int emitted = 0;
    if (step > 0) {
        for (int v = first; v <= last && emitted < 4096; v += step, emitted++) printk("%d\n", v);
    } else {
        for (int v = first; v >= last && emitted < 4096; v += step, emitted++) printk("%d\n", v);
    }
    return 0;
}

// ===========================================================================
// diff
// ===========================================================================
int coreutils_diff(int argc, char** argv) {
    if (argc < 3) {
        printk("Usage: diff <file1> <file2>    line-by-line comparison\n");
        return 1;
    }

    cu_text_t a, b;
    if (!cu_read_text(argv[1], &a)) return 1;
    if (!cu_read_text(argv[2], &b)) {
        cu_free_text(&a);
        return 1;
    }

    int max = (a.count > b.count) ? a.count : b.count;
    int differences = 0;

    for (int i = 0; i < max; i++) {
        const char* la = (i < a.count) ? a.lines[i] : NULL;
        const char* lb = (i < b.count) ? b.lines[i] : NULL;

        if (la && lb && strcmp(la, lb) == 0) continue;

        differences++;
        printk(ANSI_YELLOW "@@ line %d @@\n" ANSI_RESET, i + 1);
        if (la) printk(ANSI_RED   "- %s\n" ANSI_RESET, la);
        if (lb) printk(ANSI_GREEN "+ %s\n" ANSI_RESET, lb);
    }

    if (differences == 0) {
        printk(ANSI_BRIGHT_GREEN "Files '%s' and '%s' are identical (%d lines)\n" ANSI_RESET,
               argv[1], argv[2], a.count);
    } else {
        printk("\n%d differing line(s)\n", differences);
    }

    cu_free_text(&a);
    cu_free_text(&b);
    return differences ? 1 : 0;
}

// ===========================================================================
// xxd
// ===========================================================================
int coreutils_xxd(int argc, char** argv) {
    const char* path = NULL;
    int limit = 256;

    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) limit = cu_atoi(argv[++i], 256);
        else if (argv[i][0] != '-') path = argv[i];
    }

    if (!path) {
        printk("Usage: xxd [-l <bytes>] <file>    canonical hex + ASCII dump\n");
        return 1;
    }

    vfs_node_t* node = vfs_namei(path);
    if (!node) {
        printk(ANSI_RED "xxd: '%s': No such file\n" ANSI_RESET, path);
        return 1;
    }

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printk(ANSI_RED "xxd: cannot open '%s'\n" ANSI_RESET, path);
        return 1;
    }

    uint8_t row[16];
    int offset = 0;

    while (offset < limit) {
        int want = (limit - offset > 16) ? 16 : (limit - offset);
        ssize_t got = vfs_read(fd, row, (size_t)want);
        if (got <= 0) break;

        printk(ANSI_BRIGHT_BLACK "%08x: " ANSI_RESET, offset);

        for (int i = 0; i < 16; i++) {
            if (i < got) printk("%02x", row[i]);
            else         printk("  ");
            if (i % 2) printk(" ");
        }

        printk(" ");
        for (int i = 0; i < got; i++) {
            char c = (char)row[i];
            printk("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printk("\n");

        offset += (int)got;
        if (got < want) break;
    }

    vfs_close(fd);
    printk(ANSI_BRIGHT_BLACK "%d bytes dumped\n" ANSI_RESET, offset);
    return 0;
}

// ===========================================================================
// du
// ===========================================================================
static uint64_t cu_du_walk(vfs_node_t* dir, const char* path, int depth, bool summarize) {
    if (!dir || depth > 8) return 0;

    uint64_t total = 0;
    for (uint32_t i = 0; ; i++) {
        vfs_dirent_t* de = dir->readdir ? dir->readdir(dir, i) : NULL;
        if (!de) break;
        if (strcmp(de->name, ".") == 0 || strcmp(de->name, "..") == 0) continue;

        vfs_node_t* child = dir->finddir ? dir->finddir(dir, de->name) : NULL;
        if (!child) continue;

        char child_path[256];
        if (strcmp(path, "/") == 0) snprintf(child_path, sizeof(child_path), "/%s", de->name);
        else                        snprintf(child_path, sizeof(child_path), "%s/%s", path, de->name);

        if (child->flags & FS_DIRECTORY) {
            uint64_t sub = cu_du_walk(child, child_path, depth + 1, summarize);
            total += sub;
            if (!summarize) printk("%8llu KB  %s/\n", (unsigned long long)((sub + 1023) / 1024), child_path);
        } else {
            total += child->length;
            if (!summarize && child->length > 0) {
                printk("%8llu KB  %s\n",
                       (unsigned long long)((child->length + 1023) / 1024), child_path);
            }
        }
    }
    return total;
}

int coreutils_du(int argc, char** argv) {
    bool summarize = cu_has_flag(argc, argv, "-s");
    const char* path = cu_first_operand(argc, argv);
    if (!path) path = vfs_getcwd();

    vfs_node_t* node = vfs_namei(path);
    if (!node) {
        printk(ANSI_RED "du: '%s': No such file or directory\n" ANSI_RESET, path);
        return 1;
    }

    if (!(node->flags & FS_DIRECTORY)) {
        printk("%8llu KB  %s\n", (unsigned long long)((node->length + 1023) / 1024), path);
        return 0;
    }

    uint64_t total = cu_du_walk(node, path, 0, summarize);
    printk(ANSI_BRIGHT_GREEN "%8llu KB  %s (total)\n" ANSI_RESET,
           (unsigned long long)((total + 1023) / 1024), path);
    return 0;
}

// ===========================================================================
// factor
// ===========================================================================
int coreutils_factor(int argc, char** argv) {
    if (argc < 2) {
        printk("Usage: factor <number...>    print prime factorization\n");
        return 1;
    }

    for (int a = 1; a < argc; a++) {
        long n = cu_atoi(argv[a], 0);
        if (n < 2) {
            printk("%ld:\n", n);
            continue;
        }

        printk("%ld:", n);
        long v = n;
        for (long d = 2; d * d <= v; d++) {
            while (v % d == 0) {
                printk(" %ld", d);
                v /= d;
            }
        }
        if (v > 1) printk(" %ld", v);
        printk("\n");
    }
    return 0;
}

// ===========================================================================
// sum
// ===========================================================================
int coreutils_sum(int argc, char** argv) {
    const char* path = cu_first_operand(argc, argv);
    if (!path) {
        printk("Usage: sum <file>    BSD 16-bit rotating checksum\n");
        return 1;
    }

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printk(ANSI_RED "sum: cannot open '%s'\n" ANSI_RESET, path);
        return 1;
    }

    uint8_t buf[256];
    uint32_t checksum = 0;
    uint64_t bytes = 0;
    ssize_t got;

    while ((got = vfs_read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < got; i++) {
            // BSD sum: rotate right one bit, then add, keeping 16 bits.
            checksum = (checksum >> 1) | ((checksum & 1) << 15);
            checksum = (checksum + buf[i]) & 0xFFFF;
        }
        bytes += (uint64_t)got;
    }
    vfs_close(fd);

    printk("%05u %5llu %s\n", checksum,
           (unsigned long long)((bytes + 1023) / 1024), path);
    return 0;
}

// ===========================================================================
// truncate
// ===========================================================================
int coreutils_truncate(int argc, char** argv) {
    if (argc < 3) {
        printk("Usage: truncate -s <size> <file>    shrink or grow a file\n");
        return 1;
    }

    long size = -1;
    const char* path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) size = cu_atoi(argv[++i], -1);
        else if (argv[i][0] != '-') path = argv[i];
    }

    if (!path || size < 0) {
        printk(ANSI_RED "truncate: need -s <size> and a file\n" ANSI_RESET);
        return 1;
    }

    vfs_node_t* node = vfs_namei(path);
    if (!node) {
        printk(ANSI_RED "truncate: '%s': No such file\n" ANSI_RESET, path);
        return 1;
    }

    size_t old_len = node->length;

    if ((size_t)size > old_len && node->write) {
        // Growing: append zero bytes so the tail is well defined.
        size_t pad = (size_t)size - old_len;
        uint8_t zeros[64];
        memset(zeros, 0, sizeof(zeros));

        int fd = vfs_open(path, O_WRONLY);
        if (fd >= 0) {
            vfs_lseek(fd, (off_t)old_len, SEEK_SET);
            while (pad > 0) {
                size_t chunk = (pad > sizeof(zeros)) ? sizeof(zeros) : pad;
                if (vfs_write(fd, zeros, chunk) <= 0) break;
                pad -= chunk;
            }
            vfs_close(fd);
        }
    } else {
        node->length = (size_t)size;
    }

    printk("truncate: '%s' %llu -> %llu bytes\n", path,
           (unsigned long long)old_len, (unsigned long long)node->length);
    return 0;
}
