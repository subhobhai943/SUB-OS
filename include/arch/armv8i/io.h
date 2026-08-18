#ifndef _ARCH_ARMV8I_IO_H
#define _ARCH_ARMV8I_IO_H

#include <stdint.h>

static inline void mmio_write8(uintptr_t reg, uint8_t val) {
    *(volatile uint8_t*)reg = val;
}

static inline uint8_t mmio_read8(uintptr_t reg) {
    return *(volatile uint8_t*)reg;
}

static inline void mmio_write16(uintptr_t reg, uint16_t val) {
    *(volatile uint16_t*)reg = val;
}

static inline uint16_t mmio_read16(uintptr_t reg) {
    return *(volatile uint16_t*)reg;
}

static inline void mmio_write32(uintptr_t reg, uint32_t val) {
    *(volatile uint32_t*)reg = val;
}

static inline uint32_t mmio_read32(uintptr_t reg) {
    return *(volatile uint32_t*)reg;
}

static inline void mmio_write64(uintptr_t reg, uint64_t val) {
    *(volatile uint32_t*)reg = (uint32_t)(val & 0xFFFFFFFF);
    *(volatile uint32_t*)(reg + 4) = (uint32_t)(val >> 32);
}

static inline uint64_t mmio_read64(uintptr_t reg) {
    uint32_t low = *(volatile uint32_t*)reg;
    uint32_t high = *(volatile uint32_t*)(reg + 4);
    return ((uint64_t)high << 32) | low;
}

// Memory barriers
static inline void dsb(void) {
    __asm__ volatile("dsb" ::: "memory");
}

static inline void dmb(void) {
    __asm__ volatile("dmb" ::: "memory");
}

static inline void isb(void) {
    __asm__ volatile("isb" ::: "memory");
}

// x86 Port I/O Stubs
static inline void outb(uint16_t port, uint8_t val) { (void)port; (void)val; }
static inline uint8_t inb(uint16_t port) { (void)port; return 0; }
static inline void outw(uint16_t port, uint16_t val) { (void)port; (void)val; }
static inline uint16_t inw(uint16_t port) { (void)port; return 0; }
static inline void outl(uint16_t port, uint32_t val) { (void)port; (void)val; }
static inline uint32_t inl(uint16_t port) { (void)port; return 0; }
static inline void io_wait(void) { __asm__ volatile("nop"); }

static inline void cli(void) {
    __asm__ volatile("cpsid i" ::: "memory");
}

static inline void sti(void) {
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline void hlt(void) {
    __asm__ volatile("wfi");
}

#endif // _ARCH_ARMV8I_IO_H
