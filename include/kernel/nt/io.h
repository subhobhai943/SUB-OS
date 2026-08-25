#ifndef _KERNEL_NT_IO_H
#define _KERNEL_NT_IO_H

// -----------------------------------------------------------------------------
// SUB-OS NT-Inspired I/O Manager
//
// Models the Windows NT I/O subsystem: DRIVER_OBJECTs export a MajorFunction
// dispatch table, DEVICE_OBJECTs are created by drivers and can be stacked
// (attached) into a device stack, and I/O flows as IRPs (I/O Request Packets)
// pushed down the stack with IoCallDriver / completed with IoCompleteRequest.
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/nt/ob.h>   // NTSTATUS, STATUS_* codes

typedef enum {
    IRP_MJ_CREATE = 0,
    IRP_MJ_CLOSE,
    IRP_MJ_READ,
    IRP_MJ_WRITE,
    IRP_MJ_DEVICE_CONTROL,
    IRP_MJ_MAXIMUM
} irp_major_t;

#define IO_NAME_MAX      48
#define IO_MAX_DRIVERS   16
#define IO_MAX_DEVICES   32

typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP           IRP;

typedef NTSTATUS (*driver_dispatch_t)(DEVICE_OBJECT* dev, IRP* irp);
typedef NTSTATUS (*driver_entry_t)(DRIVER_OBJECT* drv);

struct _DRIVER_OBJECT {
    char              name[IO_NAME_MAX];   // e.g. "\Driver\Null"
    driver_dispatch_t major_function[IRP_MJ_MAXIMUM];
    DEVICE_OBJECT*    device_list;         // devices this driver owns
    uint64_t          irp_count;
};

struct _DEVICE_OBJECT {
    char           name[IO_NAME_MAX];      // e.g. "\Device\Null"
    DRIVER_OBJECT* driver;
    DEVICE_OBJECT* next_on_driver;         // next in the driver's device_list
    DEVICE_OBJECT* attached_to;            // lower device in the stack (or NULL)
    void*          device_extension;
    uint64_t       read_count;
    uint64_t       write_count;
    uint64_t       ioctl_count;
};

struct _IRP {
    irp_major_t    major_function;
    void*          buffer;
    uint32_t       length;
    uint32_t       io_control_code;
    NTSTATUS       status;
    uint32_t       information;            // bytes transferred on completion
    DEVICE_OBJECT* current_device;
};

void            io_init(void);
const char*     io_major_name(irp_major_t mj);

DRIVER_OBJECT*  io_create_driver(const char* name, driver_entry_t entry);
NTSTATUS        io_create_device(DRIVER_OBJECT* drv, const char* name,
                                 size_t ext_size, DEVICE_OBJECT** out);
DEVICE_OBJECT*  io_attach_device(DEVICE_OBJECT* source, DEVICE_OBJECT* target);
DEVICE_OBJECT*  io_lookup_device(const char* name);

IRP*            io_build_irp(irp_major_t mj, void* buffer, uint32_t length);
NTSTATUS        io_call_driver(DEVICE_OBJECT* dev, IRP* irp);
void            io_complete_request(IRP* irp, NTSTATUS status);
void            io_free_irp(IRP* irp);

// Enumeration / stats.
int             io_driver_count(void);
DRIVER_OBJECT*  io_driver_at(int index);
int             io_device_count(void);
DEVICE_OBJECT*  io_device_at(int index);
void            io_get_stats(uint32_t* out_drivers, uint32_t* out_devices, uint64_t* out_irps);

#endif // _KERNEL_NT_IO_H
