// SUB Language In-Kernel AST Interpreter & Virtual Machine
// Executes .sb scripts directly inside the SUB-OS kernel environment

#include <kernel/sub_lang.h>
#include <kernel/printk.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

#define MAX_VARS 32
#define MAX_VAR_NAME 32
#define MAX_STR_LEN 128

typedef struct {
    char name[MAX_VAR_NAME];
    long num_val;
    char str_val[MAX_STR_LEN];
    bool is_string;
    bool active;
} sub_var_t;

typedef struct {
    sub_var_t vars[MAX_VARS];
    int var_count;
} sub_env_t;

static sub_env_t global_env;

static void sub_env_init(sub_env_t* env) {
    memset(env, 0, sizeof(sub_env_t));
}

static sub_var_t* sub_env_find(sub_env_t* env, const char* name) {
    for (int i = 0; i < env->var_count; i++) {
        if (env->vars[i].active && strcmp(env->vars[i].name, name) == 0) {
            return &env->vars[i];
        }
    }
    return NULL;
}

static void sub_env_set_num(sub_env_t* env, const char* name, long val) {
    sub_var_t* var = sub_env_find(env, name);
    if (!var) {
        if (env->var_count < MAX_VARS) {
            var = &env->vars[env->var_count++];
            strncpy(var->name, name, MAX_VAR_NAME - 1);
            var->active = true;
        } else {
            return;
        }
    }
    var->num_val = val;
    var->is_string = false;
}

static void sub_env_set_str(sub_env_t* env, const char* name, const char* val) {
    sub_var_t* var = sub_env_find(env, name);
    if (!var) {
        if (env->var_count < MAX_VARS) {
            var = &env->vars[env->var_count++];
            strncpy(var->name, name, MAX_VAR_NAME - 1);
            var->active = true;
        } else {
            return;
        }
    }
    strncpy(var->str_val, val, MAX_STR_LEN - 1);
    var->is_string = true;
}

static const char* skip_whitespace(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

// Simple arithmetic & variable evaluator
static long sub_eval_expr(sub_env_t* env, const char* expr, const char** endptr) {
    expr = skip_whitespace(expr);
    long result = 0;

    // Check if it's a number
    if ((*expr >= '0' && *expr <= '9') || *expr == '-') {
        char* end;
        result = strtol(expr, &end, 10);
        expr = end;
    } else if ((*expr >= 'a' && *expr <= 'z') || (*expr >= 'A' && *expr <= 'Z') || *expr == '_') {
        char var_name[MAX_VAR_NAME];
        int idx = 0;
        while (((*expr >= 'a' && *expr <= 'z') || (*expr >= 'A' && *expr <= 'Z') || (*expr >= '0' && *expr <= '9') || *expr == '_') && idx < MAX_VAR_NAME - 1) {
            var_name[idx++] = *expr++;
        }
        var_name[idx] = '\0';
        sub_var_t* v = sub_env_find(env, var_name);
        if (v && !v->is_string) {
            result = v->num_val;
        }
    }

    expr = skip_whitespace(expr);
    while (*expr == '+' || *expr == '-' || *expr == '*' || *expr == '/' || *expr == '%') {
        char op = *expr++;
        expr = skip_whitespace(expr);
        long next_val = 0;
        if ((*expr >= '0' && *expr <= '9') || *expr == '-') {
            char* end;
            next_val = strtol(expr, &end, 10);
            expr = end;
        } else if ((*expr >= 'a' && *expr <= 'z') || (*expr >= 'A' && *expr <= 'Z') || *expr == '_') {
            char var_name[MAX_VAR_NAME];
            int idx = 0;
            while (((*expr >= 'a' && *expr <= 'z') || (*expr >= 'A' && *expr <= 'Z') || (*expr >= '0' && *expr <= '9') || *expr == '_') && idx < MAX_VAR_NAME - 1) {
                var_name[idx++] = *expr++;
            }
            var_name[idx] = '\0';
            sub_var_t* v = sub_env_find(env, var_name);
            if (v && !v->is_string) {
                next_val = v->num_val;
            }
        }

        switch (op) {
            case '+': result += next_val; break;
            case '-': result -= next_val; break;
            case '*': result *= next_val; break;
            case '/': if (next_val != 0) result /= next_val; break;
            case '%': if (next_val != 0) result %= next_val; break;
        }
        expr = skip_whitespace(expr);
    }

    if (endptr) *endptr = expr;
    return result;
}

// Executes a single line of SUB code
static int sub_exec_line(sub_env_t* env, const char* line) {
    line = skip_whitespace(line);
    if (*line == '\0' || (line[0] == '/' && line[1] == '/')) {
        return 0; // Comment or empty line
    }

    // 1. print(...) statement
    if (strncmp(line, "print(", 6) == 0) {
        const char* p = line + 6;
        p = skip_whitespace(p);
        if (*p == '"') {
            p++;
            char out_str[MAX_STR_LEN];
            int idx = 0;
            while (*p && *p != '"' && idx < MAX_STR_LEN - 1) {
                if (*p == '\\' && *(p + 1) == 'n') {
                    out_str[idx++] = '\n';
                    p += 2;
                } else {
                    out_str[idx++] = *p++;
                }
            }
            out_str[idx] = '\0';
            printk("%s", out_str);
            if (*p == '"') p++;
            p = skip_whitespace(p);
            if (*p == ',') {
                p++;
                long val = sub_eval_expr(env, p, NULL);
                printk(" %ld", val);
            }
            printk("\n");
        } else {
            long val = sub_eval_expr(env, p, NULL);
            printk("%ld\n", val);
        }
        return 0;
    }

    // 2. var declaration: var name = expr / "str"
    if (strncmp(line, "var ", 4) == 0) {
        const char* p = line + 4;
        p = skip_whitespace(p);
        char var_name[MAX_VAR_NAME];
        int idx = 0;
        while (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_') && idx < MAX_VAR_NAME - 1) {
            var_name[idx++] = *p++;
        }
        var_name[idx] = '\0';
        p = skip_whitespace(p);
        if (*p == '=') {
            p++;
            p = skip_whitespace(p);
            if (*p == '"') {
                p++;
                char str_val[MAX_STR_LEN];
                int sidx = 0;
                while (*p && *p != '"' && sidx < MAX_STR_LEN - 1) {
                    str_val[sidx++] = *p++;
                }
                str_val[sidx] = '\0';
                sub_env_set_str(env, var_name, str_val);
            } else {
                long val = sub_eval_expr(env, p, NULL);
                sub_env_set_num(env, var_name, val);
            }
        }
        return 0;
    }

    // 3. Assignment: name = expr
    const char* eq = strchr(line, '=');
    if (eq && eq != line) {
        char var_name[MAX_VAR_NAME];
        const char* p = line;
        int idx = 0;
        while (p < eq && *p != ' ' && idx < MAX_VAR_NAME - 1) {
            var_name[idx++] = *p++;
        }
        var_name[idx] = '\0';
        long val = sub_eval_expr(env, eq + 1, NULL);
        sub_env_set_num(env, var_name, val);
        return 0;
    }

    return 0;
}

int sub_vm_eval_string(const char* code) {
    if (!code) return -1;
    sub_env_init(&global_env);

    char buffer[256];
    const char* p = code;
    while (*p) {
        int idx = 0;
        while (*p && *p != '\n' && *p != ';' && idx < 255) {
            buffer[idx++] = *p++;
        }
        buffer[idx] = '\0';
        if (*p == '\n' || *p == ';') p++;
        sub_exec_line(&global_env, buffer);
    }
    return 0;
}

int sub_vm_eval_file(const char* filepath) {
    if (!filepath) return -1;

    int fd = vfs_open(filepath, 0);
    if (fd < 0) {
        printk(ANSI_RED "Error: cannot open SUB source file '%s'\n" ANSI_RESET, filepath);
        return -1;
    }

    char* buffer = kmalloc(4096);
    if (!buffer) {
        vfs_close(fd);
        return -1;
    }

    int bytes = vfs_read(fd, buffer, 4095);
    vfs_close(fd);

    if (bytes <= 0) {
        kfree(buffer);
        return 0;
    }
    buffer[bytes] = '\0';

    int res = sub_vm_eval_string(buffer);
    kfree(buffer);
    return res;
}
