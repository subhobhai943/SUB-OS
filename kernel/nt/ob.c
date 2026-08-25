// -----------------------------------------------------------------------------
// SUB-OS NT-Inspired Object Manager (Ob) + Dispatcher (Ke) + IRQL
//
// Objects are typed, reference-counted, and named in a directory hierarchy
// rooted at "\". A global handle table stands in for the NT System process's
// handle table. Deletion follows NT rules: an object is torn down once its
// handle_count and ref_count both reach zero and it is not permanent.
// -----------------------------------------------------------------------------

#include <kernel/nt/ob.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/sync.h>

static ob_object_t* g_root = NULL;
static ob_object_t* g_handle_table[OB_HANDLE_MAX];
static uint32_t     g_object_count = 0;
static KIRQL        g_current_irql = PASSIVE_LEVEL;
static spinlock_t   g_ob_lock = SPINLOCK_INIT;

static const char* const OB_TYPE_NAMES[OB_TYPE_MAX] = {
    "Directory", "Event", "Semaphore", "Mutant"
};

const char* ob_type_name(ob_type_t t) {
    return (t < OB_TYPE_MAX) ? OB_TYPE_NAMES[t] : "Unknown";
}

ob_object_t* ob_root_directory(void) { return g_root; }

// --- Directory child list (caller holds g_ob_lock) ---------------------------
static bool dir_add_child(ob_object_t* dir, ob_object_t* child) {
    if (!dir || dir->type != OB_TYPE_DIRECTORY) return false;
    if (dir->body.dir.count >= OB_DIR_MAX) return false;
    dir->body.dir.children[dir->body.dir.count++] = child;
    child->parent = dir;
    return true;
}

static void dir_remove_child(ob_object_t* dir, ob_object_t* child) {
    if (!dir || dir->type != OB_TYPE_DIRECTORY) return;
    int n = dir->body.dir.count;
    for (int i = 0; i < n; i++) {
        if (dir->body.dir.children[i] == child) {
            for (int j = i; j < n - 1; j++) {
                dir->body.dir.children[j] = dir->body.dir.children[j + 1];
            }
            dir->body.dir.count--;
            return;
        }
    }
}

static ob_object_t* dir_find_child(ob_object_t* dir, const char* name) {
    if (!dir || dir->type != OB_TYPE_DIRECTORY) return NULL;
    for (int i = 0; i < dir->body.dir.count; i++) {
        if (strcmp(dir->body.dir.children[i]->name, name) == 0) {
            return dir->body.dir.children[i];
        }
    }
    return NULL;
}

// --- Allocation (caller holds g_ob_lock) -------------------------------------
static ob_object_t* ob_alloc(ob_type_t type, const char* name) {
    ob_object_t* obj = (ob_object_t*)kzalloc(sizeof(ob_object_t));
    if (!obj) return NULL;
    obj->type = type;
    strncpy(obj->name, name ? name : "", OB_NAME_MAX - 1);
    obj->ref_count = 1;      // the reference handed back to the creator
    obj->handle_count = 0;
    obj->permanent = false;
    obj->parent = NULL;
    if (type == OB_TYPE_SEMAPHORE) {
        obj->body.sem.count = 0;
        obj->body.sem.limit = 1;
    }
    g_object_count++;
    return obj;
}

// Tear an object down once nothing references it. Caller holds g_ob_lock.
static void ob_maybe_delete(ob_object_t* obj) {
    if (!obj || obj->permanent) return;
    if (obj->ref_count > 0 || obj->handle_count > 0) return;
    if (obj->type == OB_TYPE_DIRECTORY && obj->body.dir.count > 0) return;
    if (obj->parent) {
        dir_remove_child(obj->parent, obj);
    }
    if (g_object_count > 0) g_object_count--;
    kfree(obj);
}

NTSTATUS ob_create_object(ob_type_t type, const char* name,
                          ob_object_t* dir, ob_object_t** out) {
    if (type >= OB_TYPE_MAX) return STATUS_UNSUCCESSFUL;

    spin_lock(&g_ob_lock);

    ob_object_t* parent = dir ? dir : g_root;
    if (name && name[0] && parent && dir_find_child(parent, name)) {
        spin_unlock(&g_ob_lock);
        return STATUS_OBJECT_NAME_EXISTS;
    }

    ob_object_t* obj = ob_alloc(type, name);
    if (!obj) {
        spin_unlock(&g_ob_lock);
        return STATUS_INSUFFICIENT_RES;
    }

    if (name && name[0] && parent) {
        if (!dir_add_child(parent, obj)) {
            g_object_count--;
            kfree(obj);
            spin_unlock(&g_ob_lock);
            return STATUS_INSUFFICIENT_RES;
        }
    }

    spin_unlock(&g_ob_lock);
    if (out) *out = obj;
    return STATUS_SUCCESS;
}

void ob_reference_object(ob_object_t* obj) {
    if (!obj) return;
    spin_lock(&g_ob_lock);
    obj->ref_count++;
    spin_unlock(&g_ob_lock);
}

void ob_dereference_object(ob_object_t* obj) {
    if (!obj) return;
    spin_lock(&g_ob_lock);
    if (obj->ref_count > 0) obj->ref_count--;
    ob_maybe_delete(obj);
    spin_unlock(&g_ob_lock);
}

void ob_make_permanent(ob_object_t* obj) {
    if (!obj) return;
    spin_lock(&g_ob_lock);
    obj->permanent = true;
    spin_unlock(&g_ob_lock);
}

ob_object_t* ob_lookup_path(const char* path) {
    if (!path) return NULL;
    // A bare "\" (or empty) resolves to the root directory.
    if (path[0] == '\0' || (path[0] == '\\' && path[1] == '\0')) {
        return g_root;
    }

    char buf[192];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    spin_lock(&g_ob_lock);
    ob_object_t* cur = g_root;
    char* p = buf;
    if (*p == '\\') p++;               // skip leading backslash

    while (*p && cur) {
        char* seg = p;
        while (*p && *p != '\\') p++;
        if (*p == '\\') { *p = '\0'; p++; }
        if (*seg) {
            cur = dir_find_child(cur, seg);
        }
    }
    spin_unlock(&g_ob_lock);
    return cur;
}

// --- Handle table ------------------------------------------------------------
HANDLE ob_open_handle(ob_object_t* obj) {
    if (!obj) return OB_INVALID_HANDLE;
    spin_lock(&g_ob_lock);
    for (int i = 0; i < OB_HANDLE_MAX; i++) {
        if (g_handle_table[i] == NULL) {
            g_handle_table[i] = obj;
            obj->handle_count++;
            obj->ref_count++;          // the handle holds a reference
            spin_unlock(&g_ob_lock);
            // NT hands out non-zero, 4-aligned handle values; mirror that shape.
            return (HANDLE)((i + 1) * 4);
        }
    }
    spin_unlock(&g_ob_lock);
    return OB_INVALID_HANDLE;
}

static int handle_to_index(HANDLE h) {
    if (h <= 0 || (h & 3)) return -1;
    int idx = (h / 4) - 1;
    return (idx >= 0 && idx < OB_HANDLE_MAX) ? idx : -1;
}

ob_object_t* ob_reference_by_handle(HANDLE h) {
    int idx = handle_to_index(h);
    if (idx < 0) return NULL;
    spin_lock(&g_ob_lock);
    ob_object_t* obj = g_handle_table[idx];
    if (obj) obj->ref_count++;
    spin_unlock(&g_ob_lock);
    return obj;
}

NTSTATUS ob_close_handle(HANDLE h) {
    int idx = handle_to_index(h);
    if (idx < 0) return STATUS_INVALID_HANDLE;
    spin_lock(&g_ob_lock);
    ob_object_t* obj = g_handle_table[idx];
    if (!obj) {
        spin_unlock(&g_ob_lock);
        return STATUS_INVALID_HANDLE;
    }
    g_handle_table[idx] = NULL;
    if (obj->handle_count > 0) obj->handle_count--;
    if (obj->ref_count > 0) obj->ref_count--; // drop the handle's reference
    ob_maybe_delete(obj);
    spin_unlock(&g_ob_lock);
    return STATUS_SUCCESS;
}

int ob_open_handle_count(void) {
    int n = 0;
    spin_lock(&g_ob_lock);
    for (int i = 0; i < OB_HANDLE_MAX; i++) {
        if (g_handle_table[i]) n++;
    }
    spin_unlock(&g_ob_lock);
    return n;
}

// --- Dispatcher primitives ---------------------------------------------------
NTSTATUS ke_set_event(ob_object_t* evt) {
    if (!evt || evt->type != OB_TYPE_EVENT) return STATUS_UNSUCCESSFUL;
    spin_lock(&g_ob_lock);
    evt->body.event.signaled = true;
    spin_unlock(&g_ob_lock);
    return STATUS_SUCCESS;
}

NTSTATUS ke_reset_event(ob_object_t* evt) {
    if (!evt || evt->type != OB_TYPE_EVENT) return STATUS_UNSUCCESSFUL;
    spin_lock(&g_ob_lock);
    evt->body.event.signaled = false;
    spin_unlock(&g_ob_lock);
    return STATUS_SUCCESS;
}

bool ke_test_event(ob_object_t* evt) {
    if (!evt || evt->type != OB_TYPE_EVENT) return false;
    spin_lock(&g_ob_lock);
    bool sig = evt->body.event.signaled;
    if (sig && !evt->body.event.manual_reset) {
        evt->body.event.signaled = false;   // auto-reset consumes the signal
    }
    spin_unlock(&g_ob_lock);
    return sig;
}

NTSTATUS ke_release_semaphore(ob_object_t* sem, int32_t count) {
    if (!sem || sem->type != OB_TYPE_SEMAPHORE || count <= 0) return STATUS_UNSUCCESSFUL;
    spin_lock(&g_ob_lock);
    sem->body.sem.count += count;
    if (sem->body.sem.count > sem->body.sem.limit) {
        sem->body.sem.count = sem->body.sem.limit;
    }
    spin_unlock(&g_ob_lock);
    return STATUS_SUCCESS;
}

bool ke_wait_semaphore(ob_object_t* sem) {
    if (!sem || sem->type != OB_TYPE_SEMAPHORE) return false;
    spin_lock(&g_ob_lock);
    bool ok = false;
    if (sem->body.sem.count > 0) {
        sem->body.sem.count--;
        ok = true;
    }
    spin_unlock(&g_ob_lock);
    return ok;
}

// --- Enumeration / stats -----------------------------------------------------
int ob_dir_child_count(ob_object_t* dir) {
    if (!dir || dir->type != OB_TYPE_DIRECTORY) return 0;
    return dir->body.dir.count;
}

ob_object_t* ob_dir_child(ob_object_t* dir, int index) {
    if (!dir || dir->type != OB_TYPE_DIRECTORY) return NULL;
    if (index < 0 || index >= dir->body.dir.count) return NULL;
    return dir->body.dir.children[index];
}

static void count_recursive(ob_object_t* obj, uint32_t per_type[OB_TYPE_MAX]) {
    if (!obj) return;
    if (obj->type < OB_TYPE_MAX) per_type[obj->type]++;
    if (obj->type == OB_TYPE_DIRECTORY) {
        for (int i = 0; i < obj->body.dir.count; i++) {
            count_recursive(obj->body.dir.children[i], per_type);
        }
    }
}

void ob_get_stats(uint32_t* out_objects, uint32_t* out_handles,
                  uint32_t per_type[OB_TYPE_MAX]) {
    for (int i = 0; i < OB_TYPE_MAX; i++) per_type[i] = 0;
    count_recursive(g_root, per_type);
    if (out_objects) *out_objects = g_object_count;
    if (out_handles) *out_handles = (uint32_t)ob_open_handle_count();
}

// --- Software IRQL -----------------------------------------------------------
KIRQL ke_get_current_irql(void) { return g_current_irql; }

KIRQL ke_raise_irql(KIRQL new_irql) {
    KIRQL old = g_current_irql;
    if (new_irql >= g_current_irql) {   // raising only; NT bugchecks otherwise
        g_current_irql = new_irql;
    }
    return old;
}

void ke_lower_irql(KIRQL old_irql) {
    if (old_irql <= g_current_irql) {
        g_current_irql = old_irql;
    }
}

// --- Initialization ----------------------------------------------------------
void ob_init(void) {
    for (int i = 0; i < OB_HANDLE_MAX; i++) g_handle_table[i] = NULL;
    g_object_count = 0;
    g_current_irql = PASSIVE_LEVEL;

    // Root directory "\" is permanent and self-parented.
    g_root = (ob_object_t*)kzalloc(sizeof(ob_object_t));
    if (!g_root) {
        printk(KERN_ERR "OB: failed to allocate the root directory\n");
        return;
    }
    g_root->type = OB_TYPE_DIRECTORY;
    strncpy(g_root->name, "\\", OB_NAME_MAX - 1);
    g_root->permanent = true;
    g_root->ref_count = 1;
    g_object_count = 1;

    // Standard NT namespace directories.
    const char* std_dirs[] = { "BaseNamedObjects", "Device", "ObjectTypes", "KernelObjects" };
    for (size_t i = 0; i < sizeof(std_dirs) / sizeof(std_dirs[0]); i++) {
        ob_object_t* d = NULL;
        if (NT_SUCCESS(ob_create_object(OB_TYPE_DIRECTORY, std_dirs[i], g_root, &d)) && d) {
            ob_make_permanent(d);
            ob_dereference_object(d);   // drop the creation reference; name keeps it alive
        }
    }

    // Seed a couple of named dispatcher objects so the namespace is not empty.
    ob_object_t* bno = ob_lookup_path("\\BaseNamedObjects");
    ob_object_t* evt = NULL;
    if (NT_SUCCESS(ob_create_object(OB_TYPE_EVENT, "SubOsBootEvent", bno, &evt)) && evt) {
        evt->body.event.manual_reset = true;
        ke_set_event(evt);
        ob_make_permanent(evt);
        ob_dereference_object(evt);
    }
    ob_object_t* sem = NULL;
    if (NT_SUCCESS(ob_create_object(OB_TYPE_SEMAPHORE, "SubOsIoSemaphore", bno, &sem)) && sem) {
        sem->body.sem.limit = 4;
        sem->body.sem.count = 4;
        ob_make_permanent(sem);
        ob_dereference_object(sem);
    }

    uint32_t objs = 0, handles = 0, per[OB_TYPE_MAX];
    ob_get_stats(&objs, &handles, per);
    printk(ANSI_BRIGHT_GREEN "OB: " ANSI_RESET
           "NT Object Manager online (namespace root \\, %u objects, handle table %d slots)\n",
           objs, OB_HANDLE_MAX);
}
