// C++ Object-Oriented Device Driver Implementations for SUB-OS
#include "cpp_device.hpp"

extern "C" {
    #include <kernel/printk.h>
    #include <lib/string.h>
    #include <mm/kmalloc.h>
}

namespace kernel {

// -----------------------------------------------------------------------------
// Concrete Virtual RAM Disk Device
// -----------------------------------------------------------------------------
class VirtualRamDiskDevice : public BlockDevice {
private:
    uint8_t* m_storage;
    uint32_t m_sector_size;
    uint64_t m_sector_count;

public:
    VirtualRamDiskDevice(const char* name, uint64_t num_sectors)
        : BlockDevice(name), m_storage(nullptr), m_sector_size(512), m_sector_count(num_sectors) {}

    ~VirtualRamDiskDevice() override {
        if (m_storage) kfree(m_storage);
    }

    bool init() override {
        size_t total_bytes = m_sector_count * m_sector_size;
        m_storage = static_cast<uint8_t*>(kzalloc(total_bytes));
        m_initialized = (m_storage != nullptr);
        return m_initialized;
    }

    int read(void* buffer, size_t size, uint64_t offset) override {
        if (!m_initialized || !buffer) return -1;
        uint64_t total_bytes = m_sector_count * m_sector_size;
        if (offset >= total_bytes) return 0;
        size_t to_read = (offset + size > total_bytes) ? (total_bytes - offset) : size;
        memcpy(buffer, m_storage + offset, to_read);
        return static_cast<int>(to_read);
    }

    int write(const void* buffer, size_t size, uint64_t offset) override {
        if (!m_initialized || !buffer) return -1;
        uint64_t total_bytes = m_sector_count * m_sector_size;
        if (offset >= total_bytes) return 0;
        size_t to_write = (offset + size > total_bytes) ? (total_bytes - offset) : size;
        memcpy(m_storage + offset, buffer, to_write);
        return static_cast<int>(to_write);
    }

    uint64_t get_size() const override { return m_sector_count * m_sector_size; }
    uint32_t get_sector_size() const override { return m_sector_size; }
    uint64_t get_sector_count() const override { return m_sector_count; }
};

// -----------------------------------------------------------------------------
// Concrete Virtual /dev/null Char Device
// -----------------------------------------------------------------------------
class VirtualNullDevice : public CharDevice {
public:
    VirtualNullDevice() : CharDevice("null") {}

    bool init() override {
        m_initialized = true;
        return true;
    }

    int read(void* /*buffer*/, size_t /*size*/, uint64_t /*offset*/) override {
        return 0; // EOF
    }

    int write(const void* /*buffer*/, size_t size, uint64_t /*offset*/) override {
        return static_cast<int>(size); // Discard all bytes
    }

    uint64_t get_size() const override { return 0; }
};

// -----------------------------------------------------------------------------
// Concrete Virtual /dev/zero Char Device
// -----------------------------------------------------------------------------
class VirtualZeroDevice : public CharDevice {
public:
    VirtualZeroDevice() : CharDevice("zero") {}

    bool init() override {
        m_initialized = true;
        return true;
    }

    int read(void* buffer, size_t size, uint64_t /*offset*/) override {
        if (!buffer) return -1;
        memset(buffer, 0, size);
        return static_cast<int>(size);
    }

    int write(const void* /*buffer*/, size_t size, uint64_t /*offset*/) override {
        return static_cast<int>(size);
    }

    uint64_t get_size() const override { return 0; }
};

void DeviceManager::dump_devices() const {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS C++ Object-Oriented Device Tree ===\n" ANSI_RESET);
    printk(ANSI_BOLD "%-16s  %-10s  %12s  %12s\n" ANSI_RESET, "DEVICE NAME", "TYPE", "STATUS", "CAPACITY");
    printk("-----------------------------------------------------------------\n");

    for (const auto& dev : m_devices) {
        uint64_t sz = dev->get_size();
        if (sz >= 1024 * 1024) {
            printk(ANSI_BRIGHT_YELLOW "%-16s" ANSI_RESET "  %-10s  " ANSI_BRIGHT_GREEN "%-12s" ANSI_RESET "  %8llu MB\n",
                   dev->get_name(), dev->get_type_str(),
                   dev->is_initialized() ? "ONLINE" : "OFFLINE",
                   sz / (1024 * 1024));
        } else if (sz > 0) {
            printk(ANSI_BRIGHT_YELLOW "%-16s" ANSI_RESET "  %-10s  " ANSI_BRIGHT_GREEN "%-12s" ANSI_RESET "  %8llu KB\n",
                   dev->get_name(), dev->get_type_str(),
                   dev->is_initialized() ? "ONLINE" : "OFFLINE",
                   sz / 1024);
        } else {
            printk(ANSI_BRIGHT_YELLOW "%-16s" ANSI_RESET "  %-10s  " ANSI_BRIGHT_GREEN "%-12s" ANSI_RESET "  %11s\n",
                   dev->get_name(), dev->get_type_str(),
                   dev->is_initialized() ? "ONLINE" : "OFFLINE",
                   "Stream");
        }
    }
    printk("\n");
}

} // namespace kernel

// -----------------------------------------------------------------------------
// C-FFI Bridge Exports
// -----------------------------------------------------------------------------
extern "C" {

void cpp_device_init_all(void) {
    auto& mgr = kernel::DeviceManager::instance();
    mgr.register_device(kernel::make_unique<kernel::VirtualRamDiskDevice>("ramdisk0", 2048)); // 1MB RAM Disk
    mgr.register_device(kernel::make_unique<kernel::VirtualRamDiskDevice>("cxx_blk0", 4096)); // 2MB Fast C++ Disk
    mgr.register_device(kernel::make_unique<kernel::VirtualNullDevice>());
    mgr.register_device(kernel::make_unique<kernel::VirtualZeroDevice>());

    printk(KERN_INFO "CXX: Object-Oriented Device Framework online (%llu devices active)\n",
           static_cast<unsigned long long>(mgr.count()));
}

void cpp_device_dump(void) {
    kernel::DeviceManager::instance().dump_devices();
}

} // extern "C"
