// SUB-OS Tree Directory Visualization Engine
// Linux 'tree' compliant implementation with UTF-8 / ASCII branch rendering,
// depth filtering (-L), human-readable file sizes (-h/-s), and permission flags (-p).

#include <fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TREE_DEPTH 32
#define MAX_ENTRIES_PER_DIR 128

typedef struct {
    int max_depth;
    bool show_all;
    bool dirs_only;
    bool show_size;
    bool human_readable;
    bool show_perms;
    bool classify;
    bool full_path;
    bool ascii_only;
} tree_opts_t;

typedef struct {
    char name[128];
    uint32_t type;
    uint32_t size;
    uint32_t flags;
} tree_dirent_cache_t;

static void format_human_size(uint32_t bytes, char* buf, size_t max) {
    if (bytes >= 1024 * 1024) {
        snprintf(buf, max, "%3u.%uM", bytes / (1024 * 1024), (bytes % (1024 * 1024)) / 100000);
    } else if (bytes >= 1024) {
        snprintf(buf, max, "%3u.%uK", bytes / 1024, (bytes % 1024) / 100);
    } else {
        snprintf(buf, max, "%4uB", bytes);
    }
}

static void print_branch_prefix(bool* is_last_stack, int depth, bool is_current_last, bool ascii) {
    for (int i = 0; i < depth; i++) {
        if (i == depth - 1) {
            if (ascii) {
                printk("%s ", is_current_last ? "\\--" : "+--");
            } else {
                printk("%s ", is_current_last ? "└──" : "├──");
            }
        } else {
            if (ascii) {
                printk("%s   ", is_last_stack[i] ? " " : "|");
            } else {
                printk("%s   ", is_last_stack[i] ? " " : "│");
            }
        }
    }
}

static void tree_recurse(vfs_node_t* node, const char* current_path, int depth,
                         bool* is_last_stack, const tree_opts_t* opts,
                         int* total_dirs, int* total_files) {
    if (!node || !node->readdir) return;
    if (opts->max_depth > 0 && depth >= opts->max_depth) return;
    if (depth >= MAX_TREE_DEPTH) return;

    // Cache valid directory entries to identify the last child
    tree_dirent_cache_t entries[MAX_ENTRIES_PER_DIR];
    int count = 0;

    for (uint32_t idx = 0; idx < 1024 && count < MAX_ENTRIES_PER_DIR; idx++) {
        vfs_dirent_t* ent = node->readdir(node, idx);
        if (!ent) break;

        // Skip '.' and '..'
        if (strcmp(ent->name, ".") == 0 || strcmp(ent->name, "..") == 0) continue;

        // Skip hidden dotfiles unless -a is specified
        if (!opts->show_all && ent->name[0] == '.') continue;

        bool is_dir = (ent->type & FS_DIRECTORY) != 0;
        if (opts->dirs_only && !is_dir) continue;

        strncpy(entries[count].name, ent->name, sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        entries[count].type = ent->type;

        // Query child node for metadata (size/permissions)
        vfs_node_t* child = node->finddir ? node->finddir(node, ent->name) : NULL;
        if (child) {
            entries[count].size = (uint32_t)child->length;
            entries[count].flags = child->flags;
        } else {
            entries[count].size = 0;
            entries[count].flags = 0;
        }
        count++;
    }

    // Traverse cached entries
    for (int i = 0; i < count; i++) {
        bool is_last = (i == count - 1);
        bool is_dir = (entries[i].type & FS_DIRECTORY) != 0;

        char subpath[256];
        if (strcmp(current_path, "/") == 0) {
            snprintf(subpath, sizeof(subpath), "/%s", entries[i].name);
        } else {
            snprintf(subpath, sizeof(subpath), "%s/%s", current_path, entries[i].name);
        }

        // Print tree prefix branches
        print_branch_prefix(is_last_stack, depth + 1, is_last, opts->ascii_only);

        // Optional: Permissions [-p]
        if (opts->show_perms) {
            if (is_dir) {
                printk("[drwxr-xr-x] ");
            } else {
                printk("[-rw-r--r--] ");
            }
        }

        // Optional: Size [-s or -h]
        if (opts->show_size) {
            if (opts->human_readable) {
                char size_buf[16];
                format_human_size(entries[i].size, size_buf, sizeof(size_buf));
                printk("[%6s]  ", size_buf);
            } else {
                printk("[%8u]  ", entries[i].size);
            }
        }

        // Print filename with colors and type markers
        const char* display_name = opts->full_path ? subpath : entries[i].name;

        if (is_dir) {
            (*total_dirs)++;
            printk(ANSI_BRIGHT_BLUE ANSI_BOLD "%s" ANSI_RESET, display_name);
            if (opts->classify) printk("/");
            printk("\n");

            // Recurse into subdirectories
            vfs_node_t* child_dir = node->finddir ? node->finddir(node, entries[i].name) : NULL;
            if (child_dir) {
                is_last_stack[depth] = is_last;
                tree_recurse(child_dir, subpath, depth + 1, is_last_stack, opts, total_dirs, total_files);
            }
        } else {
            (*total_files)++;
            if (strstr(entries[i].name, ".sh") || strstr(entries[i].name, ".elf") || strstr(entries[i].name, ".bin")) {
                printk(ANSI_BRIGHT_GREEN "%s" ANSI_RESET, display_name);
                if (opts->classify) printk("*");
            } else if (strstr(entries[i].name, ".txt") || strstr(entries[i].name, ".log") || strstr(entries[i].name, ".conf")) {
                printk(ANSI_BRIGHT_YELLOW "%s" ANSI_RESET, display_name);
            } else {
                printk(ANSI_WHITE "%s" ANSI_RESET, display_name);
            }
            printk("\n");
        }
    }
}

int cmd_tree_main(int argc, char** argv) {
    tree_opts_t opts = {
        .max_depth = 0,
        .show_all = false,
        .dirs_only = false,
        .show_size = false,
        .human_readable = false,
        .show_perms = false,
        .classify = false,
        .full_path = false,
        .ascii_only = false,
    };

    const char* target_path = "/";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            opts.show_all = true;
        } else if (strcmp(argv[i], "-d") == 0) {
            opts.dirs_only = true;
        } else if (strcmp(argv[i], "-s") == 0) {
            opts.show_size = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            opts.show_size = true;
            opts.human_readable = true;
        } else if (strcmp(argv[i], "-p") == 0) {
            opts.show_perms = true;
        } else if (strcmp(argv[i], "-F") == 0) {
            opts.classify = true;
        } else if (strcmp(argv[i], "-f") == 0) {
            opts.full_path = true;
        } else if (strcmp(argv[i], "--ascii") == 0) {
            opts.ascii_only = true;
        } else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            opts.max_depth = (int)strtol(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--help") == 0) {
            printk("Usage: tree [options] [directory]\n\n"
                   "Options:\n"
                   "  -a          All files (include hidden dotfiles)\n"
                   "  -d          List directories only\n"
                   "  -L <level>  Descend only level directories deep\n"
                   "  -f          Print the full path prefix for each file\n"
                   "  -s          Print the size in bytes of each file\n"
                   "  -h          Print sizes in human readable format\n"
                   "  -p          Print file type and permissions\n"
                   "  -F          Append '/' for directories, '*' for executables\n"
                   "  --ascii     Use ASCII lines instead of UTF-8 characters\n");
            return 0;
        } else if (argv[i][0] != '-') {
            target_path = argv[i];
        }
    }

    vfs_node_t* root_node = vfs_namei(target_path);
    if (!root_node) {
        printk(ANSI_RED "tree: '%s': No such file or directory\n" ANSI_RESET, target_path);
        return 1;
    }

    printk(ANSI_BRIGHT_CYAN ANSI_BOLD "%s" ANSI_RESET "\n", target_path);

    bool is_last_stack[MAX_TREE_DEPTH];
    memset(is_last_stack, 0, sizeof(is_last_stack));

    int total_dirs = 0;
    int total_files = 0;

    tree_recurse(root_node, target_path, 0, is_last_stack, &opts, &total_dirs, &total_files);

    printk("\n" ANSI_BRIGHT_YELLOW "%d directories" ANSI_RESET, total_dirs);
    if (!opts.dirs_only) {
        printk(ANSI_BRIGHT_YELLOW ", %d files" ANSI_RESET, total_files);
    }
    printk("\n");

    return 0;
}
