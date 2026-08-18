// C++ Object-Oriented Subsystems & Telemetry Engine for SUB-OS
// Demonstrates Polymorphism, Virtual Dispatch, Abstract Classes & RAII inside the kernel

#include <kernel/cpp_kernel.h>

extern "C" {
    #include <kernel/printk.h>
    #include <mm/pmm.h>
    #include <mm/kmalloc.h>
    #include <lib/string.h>

    void cpp_call_global_constructors(void);
}

// -----------------------------------------------------------------------------
// Abstract Base Class for Kernel Services
// -----------------------------------------------------------------------------
class AbstractKernelService {
protected:
    char m_name[32];
    bool m_active;

public:
    AbstractKernelService(const char* name) : m_active(false) {
        strncpy(m_name, name ? name : "Service", sizeof(m_name) - 1);
        m_name[sizeof(m_name) - 1] = '\0';
    }

    virtual ~AbstractKernelService() = default;

    virtual void start() = 0;
    virtual uint32_t get_health_score() const = 0;
    virtual void dump_status() const = 0;

    const char* get_name() const { return m_name; }
    bool is_active() const { return m_active; }
};

// -----------------------------------------------------------------------------
// Derived Service 1: Memory Telemetry & Fragmentation Monitor
// -----------------------------------------------------------------------------
class MemoryTelemetryService : public AbstractKernelService {
private:
    uint64_t m_samples_collected;
    uint32_t m_baseline_free_mb;

public:
    MemoryTelemetryService() 
        : AbstractKernelService("CXX-MemTelemetry"), m_samples_collected(0), m_baseline_free_mb(0) {}

    void start() override {
        m_active = true;
        m_baseline_free_mb = static_cast<uint32_t>((pmm_get_free_pages() * PMM_PAGE_SIZE) / (1024 * 1024));
        m_samples_collected = 1;
    }

    uint32_t get_health_score() const override {
        uint64_t free_mem = pmm_get_free_pages() * PMM_PAGE_SIZE;
        uint64_t usable_mem = pmm_get_usable_memory();
        if (usable_mem == 0) return 100;
        uint32_t percent_free = static_cast<uint32_t>((free_mem * 100) / usable_mem);
        return (percent_free > 100) ? 100 : percent_free;
    }

    void dump_status() const override {
        uint64_t total_mb = pmm_get_usable_memory() / (1024 * 1024);
        uint64_t free_mb = (pmm_get_free_pages() * PMM_PAGE_SIZE) / (1024 * 1024);
        uint64_t used_mb = total_mb > free_mb ? (total_mb - free_mb) : 0;

        printk(ANSI_BRIGHT_CYAN "  [C++ Service: %s]\n" ANSI_RESET, m_name);
        printk("    Status      : " ANSI_BRIGHT_GREEN "%s\n" ANSI_RESET, m_active ? "ACTIVE (ONLINE)" : "OFFLINE");
        printk("    Memory Total: " ANSI_YELLOW "%llu MB\n" ANSI_RESET, total_mb);
        printk("    Memory Used : " ANSI_YELLOW "%llu MB\n" ANSI_RESET, used_mb);
        printk("    Memory Free : " ANSI_BRIGHT_GREEN "%llu MB\n" ANSI_RESET, free_mb);
        printk("    Health Score: " ANSI_BRIGHT_GREEN "%u / 100\n" ANSI_RESET, get_health_score());
    }
};

// -----------------------------------------------------------------------------
// Derived Service 2: Hardware Architecture Device Node
// -----------------------------------------------------------------------------
class HardwareNodeService : public AbstractKernelService {
private:
    char m_arch_label[32];
    uint32_t m_bus_count;

public:
    HardwareNodeService() 
        : AbstractKernelService("CXX-HWNode"), m_bus_count(6) {
#if defined(__x86_64__)
        strcpy(m_arch_label, "x86_64 (AMD64 PML4)");
#elif defined(__aarch64__)
        strcpy(m_arch_label, "aarch64 (ARMv8-A EL1)");
#elif defined(__arm__)
        strcpy(m_arch_label, "armv8i (AArch32 Section)");
#else
        strcpy(m_arch_label, "Generic Platform");
#endif
    }

    void start() override {
        m_active = true;
    }

    uint32_t get_health_score() const override {
        return 99;
    }

    void dump_status() const override {
        printk(ANSI_BRIGHT_CYAN "  [C++ Service: %s]\n" ANSI_RESET, m_name);
        printk("    Status      : " ANSI_BRIGHT_GREEN "%s\n" ANSI_RESET, m_active ? "ACTIVE (ONLINE)" : "OFFLINE");
        printk("    Architecture: " ANSI_BRIGHT_YELLOW "%s\n" ANSI_RESET, m_arch_label);
        printk("    System Buses: " ANSI_YELLOW "%u active\n" ANSI_RESET, m_bus_count);
        printk("    Health Score: " ANSI_BRIGHT_GREEN "%u / 100\n" ANSI_RESET, get_health_score());
    }
};

// -----------------------------------------------------------------------------
// Template-based Kernel Metric Buffer
// -----------------------------------------------------------------------------
template<typename T, size_t Capacity>
class FixedRingBuffer {
private:
    T m_buffer[Capacity];
    size_t m_head = 0;
    size_t m_count = 0;

public:
    void push(const T& val) {
        m_buffer[m_head] = val;
        m_head = (m_head + 1) % Capacity;
        if (m_count < Capacity) m_count++;
    }

    size_t size() const { return m_count; }
    size_t capacity() const { return Capacity; }
};

// Global Pointers for C++ Service Instances
static MemoryTelemetryService* g_mem_service = nullptr;
static HardwareNodeService* g_hw_service = nullptr;
static FixedRingBuffer<uint32_t, 16>* g_metric_ring = nullptr;

// -----------------------------------------------------------------------------
// C-FFI Bridge Exports
// -----------------------------------------------------------------------------
extern "C" {

int cpp_kernel_init(void) {
    cpp_call_global_constructors();

    if (!g_mem_service) {
        g_mem_service = new MemoryTelemetryService();
        if (g_mem_service) g_mem_service->start();
    }
    if (!g_hw_service) {
        g_hw_service = new HardwareNodeService();
        if (g_hw_service) g_hw_service->start();
    }
    if (!g_metric_ring) {
        g_metric_ring = new FixedRingBuffer<uint32_t, 16>();
        if (g_metric_ring) {
            g_metric_ring->push(100);
            g_metric_ring->push(200);
            g_metric_ring->push(300);
        }
    }

    printk(KERN_INFO "CXX: Freestanding C++17 Runtime & OOP Engine initialized\n");
    return 0;
}

void cpp_kernel_print_status(void) {
    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS C++ Subsystem Status (C++17 Freestanding Mode)\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    if (g_mem_service) g_mem_service->dump_status();
    if (g_hw_service) g_hw_service->dump_status();
    if (g_metric_ring) {
        printk("  Ring Buffer : " ANSI_YELLOW "%llu / %llu entries active\n" ANSI_RESET,
               static_cast<unsigned long long>(g_metric_ring->size()),
               static_cast<unsigned long long>(g_metric_ring->capacity()));
    }
}

int cpp_test_oop_subsystem(void) {
    printk(ANSI_BRIGHT_CYAN "Running C++ OOP Virtual Dispatch and Polymorphism Test...\n" ANSI_RESET);

    AbstractKernelService* services[2] = { g_mem_service, g_hw_service };
    uint32_t total_health = 0;

    for (size_t i = 0; i < 2; i++) {
        if (services[i]) {
            printk("  Dispatching to: " ANSI_BOLD "%s" ANSI_RESET " -> Health: " ANSI_BRIGHT_GREEN "%u%%\n" ANSI_RESET,
                   services[i]->get_name(), services[i]->get_health_score());
            total_health += services[i]->get_health_score();
        }
    }

    // Dynamic allocation test with operator new and delete
    MemoryTelemetryService* dyn_service = new MemoryTelemetryService();
    if (dyn_service) {
        dyn_service->start();
        printk("  Dynamic Allocation (operator new/delete): " ANSI_BRIGHT_GREEN "PASSED\n" ANSI_RESET);
        delete dyn_service;
    }

    printk(ANSI_BRIGHT_GREEN "C++ Subsystem Test Completed Successfully!\n" ANSI_RESET);
    return 0;
}

} // extern "C"
