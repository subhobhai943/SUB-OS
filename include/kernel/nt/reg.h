#ifndef _KERNEL_NT_REG_H
#define _KERNEL_NT_REG_H

// -----------------------------------------------------------------------------
// SUB-OS NT-Inspired Configuration Registry
//
// A hive-and-key configuration store modelled on the Windows NT Configuration
// Manager (Cm). Keys form a tree rooted at "\Registry"; each key holds typed
// values (REG_SZ / REG_DWORD / REG_QWORD / REG_BINARY / REG_MULTI_SZ) whose
// type codes match the Win32 constants. Backed by kmalloc, bounded fan-out.
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    REG_NONE      = 0,
    REG_SZ        = 1,   // null-terminated string
    REG_BINARY    = 3,   // raw bytes
    REG_DWORD     = 4,   // 32-bit
    REG_MULTI_SZ  = 7,   // list of strings, shown joined by '|'
    REG_QWORD     = 11   // 64-bit
} reg_type_t;

#define REG_NAME_MAX     48
#define REG_STR_MAX      128
#define REG_MAX_CHILDREN 24
#define REG_MAX_VALUES   24

typedef struct reg_value {
    char       name[REG_NAME_MAX];
    reg_type_t type;
    uint32_t   data_len;
    union {
        uint32_t dword;
        uint64_t qword;
        char     str[REG_STR_MAX];
        uint8_t  bin[REG_STR_MAX];
    } data;
} reg_value_t;

typedef struct reg_key {
    char             name[REG_NAME_MAX];
    struct reg_key*  parent;
    struct reg_key*  children[REG_MAX_CHILDREN];
    int              child_count;
    reg_value_t*     values[REG_MAX_VALUES];
    int              value_count;
} reg_key_t;

void        reg_init(void);
reg_key_t*  reg_root(void);
const char* reg_type_name(reg_type_t t);

// Path form: "\Registry\Machine\Software\SUB-OS". create makes intermediates.
reg_key_t*  reg_create_key(const char* path);
reg_key_t*  reg_open_key(const char* path);

// Value setters/getters (operate on an already-open key).
reg_value_t* reg_set_dword(reg_key_t* key, const char* name, uint32_t value);
reg_value_t* reg_set_qword(reg_key_t* key, const char* name, uint64_t value);
reg_value_t* reg_set_sz(reg_key_t* key, const char* name, const char* str);
reg_value_t* reg_query_value(reg_key_t* key, const char* name);

// Enumeration.
int          reg_enum_key_count(reg_key_t* key);
reg_key_t*   reg_enum_key(reg_key_t* key, int index);
int          reg_enum_value_count(reg_key_t* key);
reg_value_t* reg_enum_value(reg_key_t* key, int index);

// Render a value's data into a human-readable string (for reg query / GUI).
void reg_value_to_string(const reg_value_t* v, char* out, size_t out_len);

void reg_get_stats(uint32_t* out_keys, uint32_t* out_values);

#endif // _KERNEL_NT_REG_H
