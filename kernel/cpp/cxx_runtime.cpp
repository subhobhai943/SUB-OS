// Freestanding C++ Runtime for SUB-OS Kernel
// Provides operator new/delete, pure virtual error handling, and global constructor calling

#include <kernel/cpp_kernel.h>

extern "C" {
    #include <mm/kmalloc.h>
    #include <kernel/printk.h>
    #include <kernel/panic.h>
    #include <lib/string.h>

    void* __dso_handle = nullptr;

    typedef void (*ctor_fn_t)(void);
    extern ctor_fn_t __init_array_start[];
    extern ctor_fn_t __init_array_end[];

    void cpp_call_global_constructors(void) {
        if (+__init_array_end <= +__init_array_start) return;
        size_t count = __init_array_end - __init_array_start;
        for (size_t i = 0; i < count; i++) {
            if (__init_array_start[i]) {
                __init_array_start[i]();
            }
        }
    }

    int __cxa_atexit(void (*/*destructor*/)(void*), void* /*arg*/, void* /*dso*/) {
        return 0; // Kernel runs indefinitely until shutdown
    }

    void __cxa_pure_virtual(void) {
        panic("C++ Pure Virtual Function Called");
    }
}

// Freestanding Dynamic Memory Allocation Operators
void* operator new(size_t size) {
    if (size == 0) size = 1;
    void* ptr = kmalloc(size);
    return ptr;
}

void* operator new[](size_t size) {
    if (size == 0) size = 1;
    void* ptr = kmalloc(size);
    return ptr;
}

// Placement new
void* operator new(size_t /*size*/, void* ptr) noexcept {
    return ptr;
}

void* operator new[](size_t /*size*/, void* ptr) noexcept {
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}

void operator delete[](void* ptr) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}

void operator delete(void* ptr, size_t /*size*/) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}

void operator delete[](void* ptr, size_t /*size*/) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}
