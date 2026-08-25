// -----------------------------------------------------------------------------
// SUB-OS NT-Inspired Configuration Registry (Cm)
//
// A key tree rooted at "\Registry" with typed values. Paths are backslash
// separated and begin at the root key named "Registry". Intermediate keys are
// created on demand by reg_create_key(); reg_open_key() only navigates.
// -----------------------------------------------------------------------------

#include <kernel/nt/reg.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>
#include <kernel/sync.h>
#include <init/version.h>
#include <arch/arch.h>

static reg_key_t*  g_reg_root = NULL;
static uint32_t    g_key_count = 0;
static uint32_t    g_value_count = 0;
static spinlock_t  g_reg_lock = SPINLOCK_INIT;

const char* reg_type_name(reg_type_t t) {
    switch (t) {
        case REG_SZ:       return "REG_SZ";
        case REG_BINARY:   return "REG_BINARY";
        case REG_DWORD:    return "REG_DWORD";
        case REG_MULTI_SZ: return "REG_MULTI_SZ";
        case REG_QWORD:    return "REG_QWORD";
        default:           return "REG_NONE";
    }
}

reg_key_t* reg_root(void) { return g_reg_root; }

// Caller holds g_reg_lock.
static reg_key_t* key_alloc(const char* name, reg_key_t* parent) {
    reg_key_t* k = (reg_key_t*)kzalloc(sizeof(reg_key_t));
    if (!k) return NULL;
    strncpy(k->name, name ? name : "", REG_NAME_MAX - 1);
    k->parent = parent;
    g_key_count++;
    return k;
}

// Caller holds g_reg_lock.
static reg_key_t* key_find_child(reg_key_t* key, const char* name) {
    if (!key) return NULL;
    for (int i = 0; i < key->child_count; i++) {
        if (strcmp(key->children[i]->name, name) == 0) return key->children[i];
    }
    return NULL;
}

// Split a path and walk it, optionally creating missing keys. The first path
// component must be "Registry" (the root key's name).
static reg_key_t* walk_path(const char* path, bool create) {
    if (!path || !g_reg_root) return NULL;

    char buf[192];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    reg_key_t* cur = g_reg_root;
    char* p = buf;
    if (*p == '\\') p++;

    // First component names the root itself.
    char* first = p;
    while (*p && *p != '\\') p++;
    if (*p == '\\') { *p = '\0'; p++; }
    if (strcmp(first, g_reg_root->name) != 0) {
        return NULL; // path is not anchored at "\Registry"
    }

    while (*p) {
        char* seg = p;
        while (*p && *p != '\\') p++;
        if (*p == '\\') { *p = '\0'; p++; }
        if (!*seg) continue;

        reg_key_t* child = key_find_child(cur, seg);
        if (!child) {
            if (!create) return NULL;
            if (cur->child_count >= REG_MAX_CHILDREN) return NULL;
            child = key_alloc(seg, cur);
            if (!child) return NULL;
            cur->children[cur->child_count++] = child;
        }
        cur = child;
    }
    return cur;
}

reg_key_t* reg_create_key(const char* path) {
    spin_lock(&g_reg_lock);
    reg_key_t* k = walk_path(path, true);
    spin_unlock(&g_reg_lock);
    return k;
}

reg_key_t* reg_open_key(const char* path) {
    spin_lock(&g_reg_lock);
    reg_key_t* k = walk_path(path, false);
    spin_unlock(&g_reg_lock);
    return k;
}

// Caller holds g_reg_lock.
static reg_value_t* value_get_or_add(reg_key_t* key, const char* name) {
    for (int i = 0; i < key->value_count; i++) {
        if (strcmp(key->values[i]->name, name) == 0) return key->values[i];
    }
    if (key->value_count >= REG_MAX_VALUES) return NULL;
    reg_value_t* v = (reg_value_t*)kzalloc(sizeof(reg_value_t));
    if (!v) return NULL;
    strncpy(v->name, name, REG_NAME_MAX - 1);
    key->values[key->value_count++] = v;
    g_value_count++;
    return v;
}

reg_value_t* reg_set_dword(reg_key_t* key, const char* name, uint32_t value) {
    if (!key || !name) return NULL;
    spin_lock(&g_reg_lock);
    reg_value_t* v = value_get_or_add(key, name);
    if (v) {
        v->type = REG_DWORD;
        v->data.dword = value;
        v->data_len = sizeof(uint32_t);
    }
    spin_unlock(&g_reg_lock);
    return v;
}

reg_value_t* reg_set_qword(reg_key_t* key, const char* name, uint64_t value) {
    if (!key || !name) return NULL;
    spin_lock(&g_reg_lock);
    reg_value_t* v = value_get_or_add(key, name);
    if (v) {
        v->type = REG_QWORD;
        v->data.qword = value;
        v->data_len = sizeof(uint64_t);
    }
    spin_unlock(&g_reg_lock);
    return v;
}

reg_value_t* reg_set_sz(reg_key_t* key, const char* name, const char* str) {
    if (!key || !name) return NULL;
    spin_lock(&g_reg_lock);
    reg_value_t* v = value_get_or_add(key, name);
    if (v) {
        v->type = REG_SZ;
        strncpy(v->data.str, str ? str : "", REG_STR_MAX - 1);
        v->data.str[REG_STR_MAX - 1] = '\0';
        v->data_len = (uint32_t)strlen(v->data.str) + 1;
    }
    spin_unlock(&g_reg_lock);
    return v;
}

reg_value_t* reg_query_value(reg_key_t* key, const char* name) {
    if (!key || !name) return NULL;
    spin_lock(&g_reg_lock);
    reg_value_t* found = NULL;
    for (int i = 0; i < key->value_count; i++) {
        if (strcmp(key->values[i]->name, name) == 0) { found = key->values[i]; break; }
    }
    spin_unlock(&g_reg_lock);
    return found;
}

int reg_enum_key_count(reg_key_t* key)   { return key ? key->child_count : 0; }
reg_key_t* reg_enum_key(reg_key_t* key, int index) {
    if (!key || index < 0 || index >= key->child_count) return NULL;
    return key->children[index];
}
int reg_enum_value_count(reg_key_t* key) { return key ? key->value_count : 0; }
reg_value_t* reg_enum_value(reg_key_t* key, int index) {
    if (!key || index < 0 || index >= key->value_count) return NULL;
    return key->values[index];
}

void reg_value_to_string(const reg_value_t* v, char* out, size_t out_len) {
    if (!v || !out || out_len == 0) return;
    switch (v->type) {
        case REG_SZ:
        case REG_MULTI_SZ:
            snprintf(out, out_len, "%s", v->data.str);
            break;
        case REG_DWORD:
            snprintf(out, out_len, "0x%08X (%u)", v->data.dword, v->data.dword);
            break;
        case REG_QWORD:
            snprintf(out, out_len, "0x%016llX", (unsigned long long)v->data.qword);
            break;
        case REG_BINARY:
            snprintf(out, out_len, "<%u bytes>", v->data_len);
            break;
        default:
            snprintf(out, out_len, "(none)");
            break;
    }
}

void reg_get_stats(uint32_t* out_keys, uint32_t* out_values) {
    if (out_keys)   *out_keys = g_key_count;
    if (out_values) *out_values = g_value_count;
}

void reg_init(void) {
    g_key_count = 0;
    g_value_count = 0;

    g_reg_root = key_alloc("Registry", NULL);
    if (!g_reg_root) {
        printk(KERN_ERR "REG: failed to allocate the registry root\n");
        return;
    }

    // Standard hives.
    reg_create_key("\\Registry\\Machine");
    reg_create_key("\\Registry\\User");

    // Hardware identification.
    reg_key_t* sys = reg_create_key("\\Registry\\Machine\\Hardware\\Description\\System");
    if (sys) {
        reg_set_sz(sys, "Identifier", "SUB-OS Modular Monolithic Kernel");
        reg_set_sz(sys, "SystemBootDevice", "\\Device\\Harddisk0\\Partition1");
    }

    // Control set.
    reg_key_t* ctl = reg_create_key("\\Registry\\Machine\\System\\CurrentControlSet\\Control");
    if (ctl) {
        reg_set_sz(ctl, "SystemStartOptions", "NOEXECUTE=OPTIN QUIET");
        reg_set_dword(ctl, "PreemptiveScheduler", 1);
    }

    // SUB-OS software hive.
    reg_key_t* sw = reg_create_key("\\Registry\\Machine\\Software\\SUB-OS");
    if (sw) {
        reg_set_sz(sw, "Version", kernel_get_version());
        reg_set_sz(sw, "BuildArch", arch_get_name());
        reg_set_dword(sw, "GrowableHeap", 1);
        reg_set_dword(sw, "ObjectManager", 1);
        reg_set_qword(sw, "InstallCookie", 0x5355424F53000001ULL);
    }

    // Default user profile.
    reg_key_t* du = reg_create_key("\\Registry\\User\\.DEFAULT\\Environment");
    if (du) {
        reg_set_sz(du, "PATH", "/bin:/sbin:/usr/bin");
        reg_set_sz(du, "SHELL", "/bin/sh");
    }

    printk(ANSI_BRIGHT_GREEN "REG: " ANSI_RESET
           "NT Configuration Registry online (%u keys, %u values under \\Registry)\n",
           g_key_count, g_value_count);
}
