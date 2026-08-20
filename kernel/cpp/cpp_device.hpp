#ifndef _KERNEL_CPP_DEVICE_HPP
#define _KERNEL_CPP_DEVICE_HPP

#include "cpp_containers.hpp"

namespace kernel {

enum class DeviceType {
    Block,
    Char,
    Network,
    Bus,
    Pseudo
};

// -----------------------------------------------------------------------------
// Base Abstract Device Class
// -----------------------------------------------------------------------------
class Device {
protected:
    String m_name;
    DeviceType m_type;
    bool m_initialized;

public:
    Device(const char* name, DeviceType type) 
        : m_name(name), m_type(type), m_initialized(false) {}

    virtual ~Device() = default;

    virtual bool init() = 0;
    virtual int read(void* buffer, size_t size, uint64_t offset) = 0;
    virtual int write(const void* buffer, size_t size, uint64_t offset) = 0;
    virtual uint64_t get_size() const = 0;

    const char* get_name() const noexcept { return m_name.c_str(); }
    DeviceType get_type() const noexcept { return m_type; }
    bool is_initialized() const noexcept { return m_initialized; }
    const char* get_type_str() const noexcept {
        switch (m_type) {
            case DeviceType::Block: return "Block";
            case DeviceType::Char: return "Char";
            case DeviceType::Network: return "Network";
            case DeviceType::Bus: return "Bus";
            case DeviceType::Pseudo: return "Pseudo";
        }
        return "Unknown";
    }
};

// -----------------------------------------------------------------------------
// Block Device Interface
// -----------------------------------------------------------------------------
class BlockDevice : public Device {
public:
    BlockDevice(const char* name) : Device(name, DeviceType::Block) {}
    virtual uint32_t get_sector_size() const = 0;
    virtual uint64_t get_sector_count() const = 0;
};

// -----------------------------------------------------------------------------
// Character Device Interface
// -----------------------------------------------------------------------------
class CharDevice : public Device {
public:
    CharDevice(const char* name) : Device(name, DeviceType::Char) {}
};

// -----------------------------------------------------------------------------
// Device Manager Singleton
// -----------------------------------------------------------------------------
class DeviceManager {
private:
    Vector<UniquePtr<Device>> m_devices;

public:
    DeviceManager() = default;

    static DeviceManager& instance() {
        static DeviceManager* s_instance = nullptr;
        if (!s_instance) {
            s_instance = new DeviceManager();
        }
        return *s_instance;
    }

    void register_device(UniquePtr<Device> device) {
        if (device && device->init()) {
            m_devices.push_back(move(device));
        }
    }

    Device* find_device(const char* name) {
        if (!name) return nullptr;
        for (auto& dev : m_devices) {
            if (strcmp(dev->get_name(), name) == 0) {
                return dev.get();
            }
        }
        return nullptr;
    }

    void dump_devices() const;
    size_t count() const noexcept { return m_devices.size(); }
};

} // namespace kernel

#endif // _KERNEL_CPP_DEVICE_HPP
