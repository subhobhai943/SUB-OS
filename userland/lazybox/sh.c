#include <userland/sh.h>
#include <userland/lazybox.h>
#include <fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_ENV_VARS 32
#define MAX_LINE_LEN 256

typedef struct {
    char key[32];
    char value[128];
    bool in_use;
} env_var_t;

static env_var_t env_table[MAX_ENV_VARS];

static void init_default_env(void) {
    static bool env_inited = false;
    if (env_inited) return;

    memset(env_table, 0, sizeof(env_table));
    sh_set_env("PATH", "/bin:/usr/bin");
    sh_set_env("USER", "root");
    sh_set_env("HOME", "/root");
    sh_set_env("SHELL", "/bin/sh");
    sh_set_env("TERM", "xterm-256color");
    sh_set_env("OS", "SUB-OS");
    env_inited = true;
}

const char* sh_get_env(const char* key) {
    init_default_env();
    if (!key) return NULL;
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].in_use && strcmp(env_table[i].key, key) == 0) {
            return env_table[i].value;
        }
    }
    return NULL;
}

int sh_set_env(const char* key, const char* value) {
    if (!key || !value) return -1;
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].in_use && strcmp(env_table[i].key, key) == 0) {
            strncpy(env_table[i].value, value, sizeof(env_table[i].value) - 1);
            return 0;
        }
    }
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (!env_table[i].in_use) {
            env_table[i].in_use = true;
            strncpy(env_table[i].key, key, sizeof(env_table[i].key) - 1);
            strncpy(env_table[i].value, value, sizeof(env_table[i].value) - 1);
            return 0;
        }
    }
    return -1;
}

static void expand_variables(const char* in, char* out, size_t max_len) {
    size_t out_idx = 0;
    while (*in && out_idx < max_len - 1) {
        if (*in == '$') {
            in++;
            char var_name[32];
            size_t vi = 0;
            while ((*in >= 'A' && *in <= 'Z') || (*in >= 'a' && *in <= 'z') || (*in >= '0' && *in <= '9') || *in == '_') {
                if (vi < sizeof(var_name) - 1) var_name[vi++] = *in;
                in++;
            }
            var_name[vi] = '\0';
            const char* val = sh_get_env(var_name);
            if (val) {
                while (*val && out_idx < max_len - 1) {
                    out[out_idx++] = *val++;
                }
            }
        } else {
            out[out_idx++] = *in++;
        }
    }
    out[out_idx] = '\0';
}

static int execute_single_line(char* line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#') return 0; // Skip empty or comment lines

    char expanded[MAX_LINE_LEN];
    expand_variables(line, expanded, sizeof(expanded));

    char* argv[16];
    int argc = 0;
    char* token = expanded;

    while (*token) {
        while (*token == ' ' || *token == '\t') *token++ = '\0';
        if (*token == '\0') break;
        if (argc < 16) argv[argc++] = token;
        while (*token && *token != ' ' && *token != '\t') token++;
    }
    if (argc == 0) return 0;

    // Check variable assignment: VAR=VALUE
    char* eq = strchr(argv[0], '=');
    if (eq && argc == 1) {
        *eq = '\0';
        return sh_set_env(argv[0], eq + 1);
    }

    if (lazybox_has_applet(argv[0])) {
        return lazybox_run_applet(argv[0], argc, argv);
    }

    printk(KERN_ERR "sh: %s: command not found\n", argv[0]);
    return 127;
}

int sh_execute_script(const char* filepath) {
    init_default_env();
    int fd = vfs_open(filepath, O_RDONLY);
    if (fd < 0) {
        printk(KERN_ERR "sh: %s: No such file or directory\n", filepath);
        return 1;
    }

    char line[MAX_LINE_LEN];
    size_t line_pos = 0;
    char c;

    while (vfs_read(fd, &c, 1) == 1) {
        if (c == '\r') continue;
        if (c == '\n') {
            line[line_pos] = '\0';
            execute_single_line(line);
            line_pos = 0;
        } else {
            if (line_pos < sizeof(line) - 1) {
                line[line_pos++] = c;
            }
        }
    }
    if (line_pos > 0) {
        line[line_pos] = '\0';
        execute_single_line(line);
    }

    vfs_close(fd);
    return 0;
}

int sh_main(int argc, char** argv) {
    init_default_env();
    if (argc < 2) {
        printk(KERN_INFO "Usage: sh <script_file.sub> | sh -c \"<command>\"\n");
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0 && argc >= 3) {
        char cmd_buf[MAX_LINE_LEN];
        strncpy(cmd_buf, argv[2], sizeof(cmd_buf) - 1);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
        return execute_single_line(cmd_buf);
    }

    return sh_execute_script(argv[1]);
}
