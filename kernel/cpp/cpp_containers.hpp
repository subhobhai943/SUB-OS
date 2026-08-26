#ifndef _KERNEL_CPP_CONTAINERS_HPP
#define _KERNEL_CPP_CONTAINERS_HPP

#include <stddef.h>
#include <stdint.h>

// Freestanding placement new in global namespace
inline void* operator new(size_t /*size*/, void* ptr) noexcept { return ptr; }
inline void* operator new[](size_t /*size*/, void* ptr) noexcept { return ptr; }

extern "C" {
    #include <mm/kmalloc.h>
    #include <lib/string.h>
}

namespace kernel {

// -----------------------------------------------------------------------------
// Move Semantics & Utility Helpers
// -----------------------------------------------------------------------------
template<typename T>
struct RemoveReference { typedef T type; };

template<typename T>
struct RemoveReference<T&> { typedef T type; };

template<typename T>
struct RemoveReference<T&&> { typedef T type; };

template<typename T>
constexpr typename RemoveReference<T>::type&& move(T&& arg) noexcept {
    return static_cast<typename RemoveReference<T>::type&&>(arg);
}

template<typename T>
constexpr T&& forward(typename RemoveReference<T>::type& arg) noexcept {
    return static_cast<T&&>(arg);
}

template<typename T>
constexpr T&& forward(typename RemoveReference<T>::type&& arg) noexcept {
    return static_cast<T&&>(arg);
}

// -----------------------------------------------------------------------------
// Freestanding numeric algorithms (namespaced to avoid clashing with the C
// kmin/kmax macros/inlines the kernel exposes elsewhere).
// -----------------------------------------------------------------------------
template<typename T>
constexpr const T& kmin(const T& a, const T& b) { return (a < b) ? a : b; }

template<typename T>
constexpr const T& kmax(const T& a, const T& b) { return (a > b) ? a : b; }

template<typename T>
constexpr const T& kclamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

template<typename T>
constexpr void kswap(T& a, T& b) {
    T tmp = move(a);
    a = move(b);
    b = move(tmp);
}

// -----------------------------------------------------------------------------
// Pair (Freestanding two-element tuple)
// -----------------------------------------------------------------------------
template<typename A, typename B>
struct Pair {
    A first;
    B second;

    constexpr Pair() : first(), second() {}
    constexpr Pair(const A& a, const B& b) : first(a), second(b) {}
};

template<typename A, typename B>
constexpr Pair<A, B> make_pair(const A& a, const B& b) { return Pair<A, B>(a, b); }

// -----------------------------------------------------------------------------
// Optional (Freestanding maybe-value, no heap)
// -----------------------------------------------------------------------------
template<typename T>
class Optional {
private:
    alignas(T) unsigned char m_storage[sizeof(T)];
    bool m_has;

    T* ptr() { return reinterpret_cast<T*>(m_storage); }
    const T* ptr() const { return reinterpret_cast<const T*>(m_storage); }

public:
    constexpr Optional() noexcept : m_has(false) {}

    Optional(const T& value) : m_has(true) {
        new (static_cast<void*>(m_storage)) T(value);
    }

    Optional(const Optional& other) : m_has(other.m_has) {
        if (m_has) new (static_cast<void*>(m_storage)) T(*other.ptr());
    }

    ~Optional() { reset(); }

    Optional& operator=(const Optional& other) {
        if (this != &other) {
            reset();
            m_has = other.m_has;
            if (m_has) new (static_cast<void*>(m_storage)) T(*other.ptr());
        }
        return *this;
    }

    void reset() {
        if (m_has) { ptr()->~T(); m_has = false; }
    }

    bool has_value() const noexcept { return m_has; }
    explicit operator bool() const noexcept { return m_has; }

    T& value() { return *ptr(); }
    const T& value() const { return *ptr(); }
    T value_or(const T& fallback) const { return m_has ? *ptr() : fallback; }
};

// -----------------------------------------------------------------------------
// UniquePtr (Freestanding Smart Pointer RAII)
// -----------------------------------------------------------------------------
template<typename T>
class UniquePtr {
private:
    T* m_ptr;

public:
    constexpr UniquePtr() noexcept : m_ptr(nullptr) {}
    constexpr UniquePtr(nullptr_t) noexcept : m_ptr(nullptr) {}
    explicit UniquePtr(T* ptr) noexcept : m_ptr(ptr) {}

    ~UniquePtr() {
        reset();
    }

    // Non-copyable
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // Move constructible & assignable
    UniquePtr(UniquePtr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    // Converting Move Constructor for Polymorphic Base Classes
    template<typename U>
    UniquePtr(UniquePtr<U>&& other) noexcept : m_ptr(other.release()) {}

    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            reset(other.m_ptr);
            other.m_ptr = nullptr;
        }
        return *this;
    }

    template<typename U>
    UniquePtr& operator=(UniquePtr<U>&& other) noexcept {
        reset(other.release());
        return *this;
    }

    T* get() const noexcept { return m_ptr; }
    T* release() noexcept {
        T* tmp = m_ptr;
        m_ptr = nullptr;
        return tmp;
    }

    void reset(T* ptr = nullptr) noexcept {
        if (m_ptr) {
            delete m_ptr;
        }
        m_ptr = ptr;
    }

    T& operator*() const { return *m_ptr; }
    T* operator->() const noexcept { return m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }
};

template<typename T, typename... Args>
UniquePtr<T> make_unique(Args&&... args) {
    return UniquePtr<T>(new T(forward<Args>(args)...));
}

// -----------------------------------------------------------------------------
// Vector (Freestanding Dynamic Array)
// -----------------------------------------------------------------------------
template<typename T>
class Vector {
private:
    T* m_data;
    size_t m_size;
    size_t m_capacity;

    void reallocate(size_t new_capacity) {
        if (new_capacity == 0) new_capacity = 4;
        T* new_data = static_cast<T*>(kmalloc(new_capacity * sizeof(T)));
        if (!new_data) return;

        for (size_t i = 0; i < m_size; i++) {
            new (static_cast<void*>(&new_data[i])) T(move(m_data[i]));
            m_data[i].~T();
        }

        if (m_data) kfree(m_data);
        m_data = new_data;
        m_capacity = new_capacity;
    }

public:
    Vector() : m_data(nullptr), m_size(0), m_capacity(0) {}

    explicit Vector(size_t initial_capacity) : m_data(nullptr), m_size(0), m_capacity(0) {
        reallocate(initial_capacity);
    }

    ~Vector() {
        clear();
        if (m_data) kfree(m_data);
    }

    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    Vector(Vector&& other) noexcept 
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    void push_back(const T& val) {
        if (m_size >= m_capacity) {
            reallocate(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        new (static_cast<void*>(&m_data[m_size])) T(val);
        m_size++;
    }

    void push_back(T&& val) {
        if (m_size >= m_capacity) {
            reallocate(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        new (static_cast<void*>(&m_data[m_size])) T(move(val));
        m_size++;
    }

    void pop_back() {
        if (m_size > 0) {
            m_size--;
            m_data[m_size].~T();
        }
    }

    void clear() {
        for (size_t i = 0; i < m_size; i++) {
            m_data[i].~T();
        }
        m_size = 0;
    }

    T& operator[](size_t index) { return m_data[index]; }
    const T& operator[](size_t index) const { return m_data[index]; }

    size_t size() const noexcept { return m_size; }
    size_t capacity() const noexcept { return m_capacity; }
    bool empty() const noexcept { return m_size == 0; }

    T* begin() noexcept { return m_data; }
    T* end() noexcept { return m_data + m_size; }
    const T* begin() const noexcept { return m_data; }
    const T* end() const noexcept { return m_data + m_size; }
};

// -----------------------------------------------------------------------------
// String (Freestanding Dynamic String)
// -----------------------------------------------------------------------------
class String {
private:
    char* m_buffer;
    size_t m_length;
    size_t m_capacity;

    void ensure_capacity(size_t req) {
        if (req + 1 > m_capacity) {
            size_t new_cap = req + 16;
            char* new_buf = static_cast<char*>(kmalloc(new_cap));
            if (m_buffer) {
                memcpy(new_buf, m_buffer, m_length + 1);
                kfree(m_buffer);
            } else {
                new_buf[0] = '\0';
            }
            m_buffer = new_buf;
            m_capacity = new_cap;
        }
    }

public:
    String() : m_buffer(nullptr), m_length(0), m_capacity(0) {
        ensure_capacity(0);
        m_buffer[0] = '\0';
    }

    String(const char* str) : m_buffer(nullptr), m_length(0), m_capacity(0) {
        if (str) {
            m_length = strlen(str);
            ensure_capacity(m_length);
            memcpy(m_buffer, str, m_length + 1);
        } else {
            ensure_capacity(0);
            m_buffer[0] = '\0';
        }
    }

    ~String() {
        if (m_buffer) kfree(m_buffer);
    }

    String(const String& other) : m_buffer(nullptr), m_length(other.m_length), m_capacity(0) {
        ensure_capacity(m_length);
        memcpy(m_buffer, other.m_buffer, m_length + 1);
    }

    String& operator=(const String& other) {
        if (this != &other) {
            m_length = other.m_length;
            ensure_capacity(m_length);
            memcpy(m_buffer, other.m_buffer, m_length + 1);
        }
        return *this;
    }

    String(String&& other) noexcept 
        : m_buffer(other.m_buffer), m_length(other.m_length), m_capacity(other.m_capacity) {
        other.m_buffer = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
    }

    String& operator+=(const char* str) {
        if (str) {
            size_t add_len = strlen(str);
            ensure_capacity(m_length + add_len);
            memcpy(m_buffer + m_length, str, add_len + 1);
            m_length += add_len;
        }
        return *this;
    }

    const char* c_str() const noexcept { return m_buffer ? m_buffer : ""; }
    size_t length() const noexcept { return m_length; }
    bool empty() const noexcept { return m_length == 0; }
};

} // namespace kernel

#endif // _KERNEL_CPP_CONTAINERS_HPP
