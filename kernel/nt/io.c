// -----------------------------------------------------------------------------
// SUB-OS NT-Inspired I/O Manager (Io)
//
// Drivers register a MajorFunction dispatch table and create named device
// objects. Devices can be stacked (a filter attached on top of a lower device);
// io_call_driver invokes the top device's dispatch, which may forward the IRP
// down to dev->attached_to. Ships three demo drivers: Null, Zero, and a Monitor
// filter stacked above Null that counts and forwards every IRP.
// -----------------------------------------------------------------------------

#include <kernel/nt/io.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/sync.h>

static DRIVER_OBJECT* g_drivers[IO_MAX_DRIVERS];
static int            g_driver_count = 0;
static DEVICE_OBJECT* g_devices[IO_MAX_DEVICES];
static int            g_device_count = 0;
static uint64_t       g_total_irps = 0;
static spinlock_t     g_io_lock = SPINLOCK_INIT;

const char* io_major_name(irp_major_t mj) {
    switch (mj) {
        case IRP_MJ_CREATE:         return "CREATE";
        case IRP_MJ_CLOSE:          return "CLOSE";
        case IRP_MJ_READ:           return "READ";
        case IRP_MJ_WRITE:          return "WRITE";
        case IRP_MJ_DEVICE_CONTROL: return "DEVICE_CONTROL";
        default:                    return "UNKNOWN";
    }
}

DRIVER_OBJECT* io_create_driver(const char* name, driver_entry_t entry) {
    spin_lock(&g_io_lock);
    if (g_driver_count >= IO_MAX_DRIVERS) {
        spin_unlock(&g_io_lock);
        return NULL;
    }
    DRIVER_OBJECT* drv = (DRIVER_OBJECT*)kzalloc(sizeof(DRIVER_OBJECT));
    if (!drv) {
        spin_unlock(&g_io_lock);
        return NULL;
    }
    strncpy(drv->name, name ? name : "", IO_NAME_MAX - 1);
    g_drivers[g_driver_count++] = drv;
    spin_unlock(&g_io_lock);

    if (entry) {
        entry(drv);   // DriverEntry fills the MajorFunction table and creates devices
    }
    return drv;
}

NTSTATUS io_create_device(DRIVER_OBJECT* drv, const char* name,
                          size_t ext_size, DEVICE_OBJECT** out) {
    if (!drv) return STATUS_UNSUCCESSFUL;
    spin_lock(&g_io_lock);
    if (g_device_count >= IO_MAX_DEVICES) {
        spin_unlock(&g_io_lock);
        return STATUS_INSUFFICIENT_RES;
    }
    DEVICE_OBJECT* dev = (DEVICE_OBJECT*)kzalloc(sizeof(DEVICE_OBJECT));
    if (!dev) {
        spin_unlock(&g_io_lock);
        return STATUS_INSUFFICIENT_RES;
    }
    strncpy(dev->name, name ? name : "", IO_NAME_MAX - 1);
    dev->driver = drv;
    if (ext_size > 0) {
        dev->device_extension = kzalloc(ext_size);
    }
    // Link into the driver's device list and the global table.
    dev->next_on_driver = drv->device_list;
    drv->device_list = dev;
    g_devices[g_device_count++] = dev;
    spin_unlock(&g_io_lock);

    if (out) *out = dev;
    return STATUS_SUCCESS;
}

DEVICE_OBJECT* io_attach_device(DEVICE_OBJECT* source, DEVICE_OBJECT* target) {
    if (!source || !target) return NULL;
    spin_lock(&g_io_lock);
    source->attached_to = target;   // source now sits above target in the stack
    spin_unlock(&g_io_lock);
    return target;
}

DEVICE_OBJECT* io_lookup_device(const char* name) {
    if (!name) return NULL;
    spin_lock(&g_io_lock);
    DEVICE_OBJECT* found = NULL;
    for (int i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i]->name, name) == 0) { found = g_devices[i]; break; }
    }
    spin_unlock(&g_io_lock);
    return found;
}

IRP* io_build_irp(irp_major_t mj, void* buffer, uint32_t length) {
    IRP* irp = (IRP*)kzalloc(sizeof(IRP));
    if (!irp) return NULL;
    irp->major_function = mj;
    irp->buffer = buffer;
    irp->length = length;
    irp->status = STATUS_UNSUCCESSFUL;
    irp->information = 0;
    return irp;
}

NTSTATUS io_call_driver(DEVICE_OBJECT* dev, IRP* irp) {
    if (!dev || !irp) return STATUS_UNSUCCESSFUL;
    irp->current_device = dev;

    spin_lock(&g_io_lock);
    g_total_irps++;
    if (dev->driver) dev->driver->irp_count++;
    switch (irp->major_function) {
        case IRP_MJ_READ:           dev->read_count++;  break;
        case IRP_MJ_WRITE:          dev->write_count++; break;
        case IRP_MJ_DEVICE_CONTROL: dev->ioctl_count++; break;
        default: break;
    }
    driver_dispatch_t fn = NULL;
    if (dev->driver && irp->major_function < IRP_MJ_MAXIMUM) {
        fn = dev->driver->major_function[irp->major_function];
    }
    spin_unlock(&g_io_lock);

    if (!fn) {
        irp->status = STATUS_UNSUCCESSFUL;
        return irp->status;
    }
    return fn(dev, irp);
}

void io_complete_request(IRP* irp, NTSTATUS status) {
    if (!irp) return;
    irp->status = status;
}

void io_free_irp(IRP* irp) {
    if (irp) kfree(irp);
}

int io_driver_count(void) { return g_driver_count; }
DRIVER_OBJECT* io_driver_at(int index) {
    return (index >= 0 && index < g_driver_count) ? g_drivers[index] : NULL;
}
int io_device_count(void) { return g_device_count; }
DEVICE_OBJECT* io_device_at(int index) {
    return (index >= 0 && index < g_device_count) ? g_devices[index] : NULL;
}
void io_get_stats(uint32_t* out_drivers, uint32_t* out_devices, uint64_t* out_irps) {
    if (out_drivers) *out_drivers = (uint32_t)g_driver_count;
    if (out_devices) *out_devices = (uint32_t)g_device_count;
    if (out_irps)    *out_irps    = g_total_irps;
}

// ---------------------------------------------------------------------------
// Demo drivers
// ---------------------------------------------------------------------------

// \Driver\Null : sink writes, return EOF on reads (like NT's \Device\Null).
static NTSTATUS null_read(DEVICE_OBJECT* dev, IRP* irp) {
    (void)dev;
    irp->information = 0;              // 0 bytes == end of file
    io_complete_request(irp, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}
static NTSTATUS null_write(DEVICE_OBJECT* dev, IRP* irp) {
    (void)dev;
    irp->information = irp->length;    // swallow everything
    io_complete_request(irp, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}
static NTSTATUS null_entry(DRIVER_OBJECT* drv) {
    drv->major_function[IRP_MJ_READ]  = null_read;
    drv->major_function[IRP_MJ_WRITE] = null_write;
    io_create_device(drv, "\\Device\\Null", 0, NULL);
    return STATUS_SUCCESS;
}

// \Driver\Zero : reads yield a buffer full of zero bytes.
static NTSTATUS zero_read(DEVICE_OBJECT* dev, IRP* irp) {
    (void)dev;
    if (irp->buffer && irp->length) {
        memset(irp->buffer, 0, irp->length);
    }
    irp->information = irp->length;
    io_complete_request(irp, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}
static NTSTATUS zero_entry(DRIVER_OBJECT* drv) {
    drv->major_function[IRP_MJ_READ] = zero_read;
    io_create_device(drv, "\\Device\\Zero", 0, NULL);
    return STATUS_SUCCESS;
}

// \Driver\Monitor : a filter stacked above \Device\Null. It counts each IRP in
// its device extension and forwards to the lower device, demonstrating a stack.
typedef struct { uint64_t forwarded; } monitor_ext_t;

static NTSTATUS monitor_passthrough(DEVICE_OBJECT* dev, IRP* irp) {
    monitor_ext_t* ext = (monitor_ext_t*)dev->device_extension;
    if (ext) ext->forwarded++;
    if (dev->attached_to) {
        return io_call_driver(dev->attached_to, irp);  // pass down the stack
    }
    io_complete_request(irp, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}
static NTSTATUS monitor_entry(DRIVER_OBJECT* drv) {
    drv->major_function[IRP_MJ_READ]           = monitor_passthrough;
    drv->major_function[IRP_MJ_WRITE]          = monitor_passthrough;
    drv->major_function[IRP_MJ_DEVICE_CONTROL] = monitor_passthrough;

    DEVICE_OBJECT* filter = NULL;
    io_create_device(drv, "\\Device\\NullMonitor", sizeof(monitor_ext_t), &filter);
    DEVICE_OBJECT* lower = io_lookup_device("\\Device\\Null");
    if (filter && lower) {
        io_attach_device(filter, lower);
    }
    return STATUS_SUCCESS;
}

void io_init(void) {
    g_driver_count = 0;
    g_device_count = 0;
    g_total_irps = 0;

    io_create_driver("\\Driver\\Null", null_entry);
    io_create_driver("\\Driver\\Zero", zero_entry);
    io_create_driver("\\Driver\\Monitor", monitor_entry);

    printk(ANSI_BRIGHT_GREEN "IO: " ANSI_RESET
           "NT I/O Manager online (%d drivers, %d devices, IRP-based dispatch)\n",
           g_driver_count, g_device_count);
}
