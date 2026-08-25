#ifndef _KERNEL_NT_OB_H
#define _KERNEL_NT_OB_H

// -----------------------------------------------------------------------------
// SUB-OS NT-Inspired Object Manager
//
// A faithful-in-spirit port of the Windows NT Executive Object Manager (Ob):
// every kernel resource is a typed, reference-counted object living in a
// hierarchical named namespace ("\BaseNamedObjects\MyEvent"), reached through a
// per-process handle table. Also carries NT dispatcher primitives (events,
// semaphores, mutants) and a software IRQL model.
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// NTSTATUS-style result codes (high bit set == error, as in NT).
typedef int32_t NTSTATUS;
#define STATUS_SUCCESS            ((NTSTATUS)0x00000000)
#define STATUS_UNSUCCESSFUL       ((NTSTATUS)0xC0000001)
#define STATUS_INVALID_HANDLE     ((NTSTATUS)0xC0000008)
#define STATUS_INSUFFICIENT_RES   ((NTSTATUS)0xC000009A)
#define STATUS_OBJECT_NAME_EXISTS ((NTSTATUS)0xC0000035)
#define STATUS_OBJECT_NOT_FOUND   ((NTSTATUS)0xC0000225)
#define NT_SUCCESS(s)             (((NTSTATUS)(s)) >= 0)

typedef enum {
    OB_TYPE_DIRECTORY = 0,
    OB_TYPE_EVENT,
    OB_TYPE_SEMAPHORE,
    OB_TYPE_MUTANT,
    OB_TYPE_MAX
} ob_type_t;

#define OB_NAME_MAX   48
#define OB_DIR_MAX    32   // children per directory
#define OB_HANDLE_MAX 256  // system handle table size

typedef struct ob_object ob_object_t;

struct ob_object {
    ob_type_t    type;
    char         name[OB_NAME_MAX];
    int32_t      ref_count;     // outstanding pointer references
    int32_t      handle_count;  // outstanding open handles
    bool         permanent;     // survives handle_count/ref_count reaching 0
    ob_object_t* parent;        // containing directory (NULL for the root)

    union {
        struct { ob_object_t* children[OB_DIR_MAX]; int count; } dir;
        struct { bool signaled; bool manual_reset; }             event;
        struct { int32_t count; int32_t limit; }                 sem;
        struct { bool held; uint32_t owner; int32_t recursion; } mutant;
    } body;
};

// A HANDLE indexes the system handle table; -1 is the invalid handle.
typedef int32_t HANDLE;
#define OB_INVALID_HANDLE ((HANDLE)-1)

// Lifecycle -------------------------------------------------------------------
void         ob_init(void);
const char*  ob_type_name(ob_type_t t);
ob_object_t* ob_root_directory(void);

NTSTATUS     ob_create_object(ob_type_t type, const char* name,
                              ob_object_t* dir, ob_object_t** out);
ob_object_t* ob_lookup_path(const char* path);   // "\BaseNamedObjects\Name"
void         ob_reference_object(ob_object_t* obj);
void         ob_dereference_object(ob_object_t* obj);
void         ob_make_permanent(ob_object_t* obj);

// Handle table ----------------------------------------------------------------
HANDLE       ob_open_handle(ob_object_t* obj);
ob_object_t* ob_reference_by_handle(HANDLE h);
NTSTATUS     ob_close_handle(HANDLE h);
int          ob_open_handle_count(void);

// Dispatcher primitives (non-blocking, poll-style) ----------------------------
NTSTATUS ke_set_event(ob_object_t* evt);
NTSTATUS ke_reset_event(ob_object_t* evt);
bool     ke_test_event(ob_object_t* evt);            // true if signaled (auto-reset consumes)
NTSTATUS ke_release_semaphore(ob_object_t* sem, int32_t count);
bool     ke_wait_semaphore(ob_object_t* sem);        // true if a unit was acquired

// Enumeration (for the objdir tool) -------------------------------------------
int          ob_dir_child_count(ob_object_t* dir);
ob_object_t* ob_dir_child(ob_object_t* dir, int index);
void         ob_get_stats(uint32_t* out_objects, uint32_t* out_handles,
                          uint32_t per_type[OB_TYPE_MAX]);

// Software IRQL model ---------------------------------------------------------
typedef enum {
    PASSIVE_LEVEL  = 0,
    APC_LEVEL      = 1,
    DISPATCH_LEVEL = 2,
    HIGH_LEVEL     = 15
} KIRQL;

KIRQL ke_get_current_irql(void);
KIRQL ke_raise_irql(KIRQL new_irql);   // returns previous IRQL
void  ke_lower_irql(KIRQL old_irql);

#endif // _KERNEL_NT_OB_H
