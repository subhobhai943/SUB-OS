# =============================================================================
# SUB-OS Master Multi-Architecture Linux-Style Makefile
# Supports: x86_64, aarch64, armv8i
# Features: Linux Kconfig TUI (make configure / make menuconfig), dynamic Kbuild
# =============================================================================

-include .config

# Architecture Selection (Default: x86_64)
ifdef CONFIG_ARCH
    ARCH ?= $(CONFIG_ARCH)
else
    ARCH ?= x86_64
endif

BUILD_DIR = build
BOOT_DIR  = boot

# -----------------------------------------------------------------------------
# Toolchain & Architecture Flags
# -----------------------------------------------------------------------------
ifeq ($(ARCH), x86_64)
    CROSS_COMPILE ?= x86_64-elf-
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy
    ASM     = nasm

    CFLAGS  = -ffreestanding \
              -fno-pie \
              -fno-pic \
              -mno-red-zone \
              -mno-mmx \
              -mno-sse \
              -mno-sse2 \
              -mcmodel=kernel \
              -Wall \
              -Wextra \
              -Wno-unused-function \
              -Wno-unused-parameter \
              $(if $(CONFIG_OPTIMIZATION),$(CONFIG_OPTIMIZATION),-O2) \
              -Iinclude \
              -D__x86_64__

    ASMFLAGS_ELF  = -f elf64
    ASMFLAGS_BOOT = -f bin -I$(BOOT_DIR)/
    LDFLAGS       = -nostdlib -no-pie -T linker.ld

    ARCH_C_SRCS   = $(shell find arch/x86_64 -name '*.c' 2>/dev/null)
    ARCH_ASM_SRCS = $(shell find arch/x86_64 -name '*.asm' 2>/dev/null)
    ENTRY_OBJ     = $(BUILD_DIR)/arch/x86_64/boot/entry.o
    IMAGE         = SUB-OS.img
    TARGET        = $(IMAGE)
    QEMU_CMD      = qemu-system-x86_64 -drive format=raw,file=$(IMAGE) \
                    -netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80 \
                    -device e1000,netdev=net0 -serial stdio -display none

else ifneq ($(filter $(ARCH), aarch64 arm64),)
    CROSS_COMPILE ?= aarch64-linux-gnu-
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy
    AS      = $(CROSS_COMPILE)as

    CFLAGS  = -ffreestanding \
              -fno-pie \
              -fno-pic \
              -Wall \
              -Wextra \
              -Wno-unused-function \
              -Wno-unused-parameter \
              -mno-outline-atomics \
              $(if $(CONFIG_OPTIMIZATION),$(CONFIG_OPTIMIZATION),-O2) \
              -Iinclude \
              -D__aarch64__ \
              -march=armv8-a

    LDFLAGS = -nostdlib -no-pie -T arch/aarch64/linker.ld

    ARCH_C_SRCS   = $(shell find arch/aarch64 -name '*.c' 2>/dev/null)
    ARCH_S_SRCS   = $(shell find arch/aarch64 -name '*.S' 2>/dev/null)
    ENTRY_OBJ     = $(BUILD_DIR)/arch/aarch64/boot/entry.o
    TARGET        = $(BUILD_DIR)/kernel.elf
    QEMU_CMD      = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic -kernel $(BUILD_DIR)/kernel.elf

else ifneq ($(filter $(ARCH), armv8i armv81 arm32 arm armv7 armv7-a armv7a aarch32),)
    CROSS_COMPILE ?= arm-linux-gnueabihf-
    CC      = $(CROSS_COMPILE)gcc
    LD      = $(CROSS_COMPILE)ld
    OBJCOPY = $(CROSS_COMPILE)objcopy
    AS      = $(CROSS_COMPILE)as

    CFLAGS  = -ffreestanding \
              -fno-pie \
              -fno-pic \
              -Wall \
              -Wextra \
              -Wno-unused-function \
              -Wno-unused-parameter \
              $(if $(CONFIG_OPTIMIZATION),$(CONFIG_OPTIMIZATION),-O2) \
              -Iinclude \
              -D__arm__ \
              -D__armv8i__ \
              -mcpu=cortex-a15 \
              -mfpu=neon-vfpv4 \
              -mfloat-abi=hard \
              -marm

    LDFLAGS = -nostdlib -no-pie -T arch/armv8i/linker.ld

    ARCH_C_SRCS   = $(shell find arch/armv8i -name '*.c' 2>/dev/null)
    ARCH_S_SRCS   = $(shell find arch/armv8i -name '*.S' 2>/dev/null)
    ENTRY_OBJ     = $(BUILD_DIR)/arch/armv8i/boot/entry.o
    TARGET        = $(BUILD_DIR)/kernel.elf
    QEMU_CMD      = qemu-system-arm -M virt -cpu cortex-a15 -m 128M -nographic -kernel $(BUILD_DIR)/kernel.elf

else
    $(error Unsupported Architecture '$(ARCH)'. Supported: x86_64, aarch64, armv8i)
endif

# -----------------------------------------------------------------------------
# Linux-Style Configurable Kernel Subsystems (Kbuild-Style)
# -----------------------------------------------------------------------------
CORE_SRCS = kernel/main.c kernel/task.c kernel/sync.c kernel/timer.c kernel/printk.c \
            kernel/panic.c kernel/trace.c kernel/sched.c kernel/cron.c kernel/syscall.c \
            kernel/syslog.c kernel/module.c kernel/kobject.c kernel/metrics.c kernel/namespace.c \
            kernel/signal.c kernel/workqueue.c kernel/tsc.c kernel/vt_art.c \
            kernel/sub/sub_runtime.c kernel/sub/sub_vm.c \
            kernel/wait.c kernel/futex.c kernel/rcu.c kernel/ktest.c \
            lib/string.c lib/vsprintf.c lib/bitmap.c lib/rbtree.c lib/kfifo.c lib/hashtable.c lib/font8x8.c \
            init/cmdline.c init/service.c init/bootlogo.c init/logo_data.c usr/initramfs.c block/block.c block/elevator.c \
            ipc/sem.c ipc/shm.c ipc/shm_posix.c ipc/pipe.c ipc/ipc.c ipc/msg.c \
            crypto/sha256.c crypto/md5.c crypto/crc32.c crypto/prng.c sound/beep.c

# If no .config exists, default all core options to y
ifeq ($(wildcard .config),)
    CONFIG_MM_PMM = y
    CONFIG_MM_SLAB = y
    CONFIG_MM_VMA = y
    CONFIG_FS_VFS = y
    CONFIG_FS_FAT32 = y
    CONFIG_FS_EXT2 = y
    CONFIG_FS_SYSFS = y
    CONFIG_DRV_RAMDISK = y
    CONFIG_DRV_AHCI = y
    CONFIG_DRV_NVME = y
    CONFIG_DRV_ATA = y
    CONFIG_DRV_VIRTIO_BLK = y
    CONFIG_NET_STACK = y
    CONFIG_NET_FILTER = y
    CONFIG_NET_HTTPD = y
    CONFIG_NET_SSHD = y
    CONFIG_DRV_E1000 = y
    CONFIG_DRV_RTL8139 = y
    CONFIG_DRV_VIRTIO_NET = y
    CONFIG_DRV_PTY = y
    CONFIG_DRV_VIRTIO_RNG = y
    CONFIG_DRV_CANVAS_2D = y
    CONFIG_DRV_BOCHS_VBE = y
    CONFIG_DRV_SOUND_HDA = y
    CONFIG_DRV_SOUND_AC97 = y
    CONFIG_DRV_USB_XHCI = y
    CONFIG_DRV_HWMON = y
    CONFIG_SECURITY_LSM = y
    CONFIG_SECURITY_KEYRING = y
    CONFIG_BPF_VM = y
    CONFIG_IO_URING = y
    CONFIG_USERLAND_LAZYBOX = y
endif

CONFIG_SRCS-y := $(CORE_SRCS)

ifeq ($(CONFIG_MM_PMM), y)
    CONFIG_SRCS-y += mm/pmm.c mm/kmalloc.c mm/page_cache.c
endif
ifeq ($(CONFIG_MM_SLAB), y)
    CONFIG_SRCS-y += mm/slab.c
endif
ifeq ($(CONFIG_MM_VMA), y)
    CONFIG_SRCS-y += mm/vma.c
endif

ifeq ($(CONFIG_BPF_VM), y)
    CONFIG_SRCS-y += kernel/bpf.c
endif
ifeq ($(CONFIG_IO_URING), y)
    CONFIG_SRCS-y += io_uring/io_uring.c
endif
ifeq ($(CONFIG_SECURITY_LSM), y)
    CONFIG_SRCS-y += security/lsm.c security/auth.c
endif
ifeq ($(CONFIG_SECURITY_KEYRING), y)
    CONFIG_SRCS-y += certs/x509.c
endif

# Filesystems
ifeq ($(CONFIG_FS_VFS), y)
    CONFIG_SRCS-y += fs/vfs.c fs/ramfs.c fs/devfs.c fs/procfs.c
endif
ifeq ($(CONFIG_FS_FAT32), y)
    CONFIG_SRCS-y += fs/fat32.c
endif
ifeq ($(CONFIG_FS_EXT2), y)
    CONFIG_SRCS-y += fs/ext2.c
endif
ifeq ($(CONFIG_FS_SYSFS), y)
    CONFIG_SRCS-y += fs/sysfs.c
endif

# Storage Drivers
ifeq ($(CONFIG_DRV_RAMDISK), y)
    CONFIG_SRCS-y += drivers/block/ramdisk.c
endif
ifeq ($(CONFIG_DRV_AHCI), y)
    CONFIG_SRCS-y += drivers/block/ahci.c
endif
ifeq ($(CONFIG_DRV_NVME), y)
    CONFIG_SRCS-y += drivers/block/nvme.c
endif
ifeq ($(CONFIG_DRV_ATA), y)
    CONFIG_SRCS-y += drivers/block/ata.c
endif
ifeq ($(CONFIG_DRV_VIRTIO_BLK), y)
    CONFIG_SRCS-y += drivers/virtio/virtio_blk.c virt/virtio.c virt/hypervisor.c
endif

# Network Subsystem
ifeq ($(CONFIG_NET_STACK), y)
    CONFIG_SRCS-y += net/core/net.c net/core/socket.c net/ipv4/tcp.c net/ipv4/udp.c net/ipv4/dhcp.c net/ipv4/dns.c net/ipv4/dns_cache.c
endif
ifeq ($(CONFIG_NET_FILTER), y)
    CONFIG_SRCS-y += net/filter.c
endif
ifeq ($(CONFIG_NET_HTTPD), y)
    CONFIG_SRCS-y += net/httpd.c
endif
ifeq ($(CONFIG_NET_SSHD), y)
    CONFIG_SRCS-y += net/ssh.c
endif
ifeq ($(CONFIG_DRV_E1000), y)
    CONFIG_SRCS-y += drivers/net/e1000.c drivers/pci/pci.c
endif
ifeq ($(CONFIG_DRV_RTL8139), y)
    CONFIG_SRCS-y += drivers/net/rtl8139.c drivers/pci/pci.c
endif
ifeq ($(CONFIG_DRV_VIRTIO_NET), y)
    CONFIG_SRCS-y += drivers/virtio/virtio_net.c virt/virtio.c
endif

# Character, Audio, Video Drivers
ifeq ($(CONFIG_DRV_PTY), y)
    CONFIG_SRCS-y += drivers/char/pty.c
endif
ifeq ($(CONFIG_DRV_VIRTIO_RNG), y)
    CONFIG_SRCS-y += drivers/char/virtio_rng.c
endif
ifeq ($(CONFIG_DRV_USB_XHCI), y)
    CONFIG_SRCS-y += drivers/usb/xhci.c drivers/usb/usb.c
endif
ifeq ($(CONFIG_DRV_HWMON), y)
    CONFIG_SRCS-y += drivers/hwmon/coretemp.c
endif
ifeq ($(CONFIG_DRV_CANVAS_2D), y)
    CONFIG_SRCS-y += drivers/video/canvas.c
endif
ifeq ($(CONFIG_DRV_BOCHS_VBE), y)
    CONFIG_SRCS-y += drivers/video/bochs.c drivers/video/fb.c drivers/video/fbcon.c
endif
ifeq ($(CONFIG_DRV_SOUND_HDA), y)
    CONFIG_SRCS-y += sound/hda.c sound/sound.c sound/pcm.c sound/tts.c sound/melody.c
endif
ifeq ($(CONFIG_DRV_SOUND_AC97), y)
    CONFIG_SRCS-y += sound/ac97.c sound/sound.c sound/pcm.c
endif

# Common Input/Display/Power/GPU/Networking Drivers
CONFIG_SRCS-y += drivers/char/tty.c drivers/char/keyboard.c drivers/char/serial.c drivers/char/vga.c \
                 drivers/input/mouse.c drivers/power/acpi.c drivers/power/cpufreq.c drivers/rtc/rtc.c drivers/sound/speaker.c \
                 drivers/net/e1000e.c drivers/virtio/virtio_gpu.c drivers/virtio/virtio_input.c

# Userland
ifeq ($(CONFIG_USERLAND_LAZYBOX), y)
    CONFIG_SRCS-y += userland/lazybox/lazybox.c userland/lazybox/shell.c userland/lazybox/sh.c userland/lazybox/nano.c userland/lazybox/snake.c userland/lazybox/tree.c userland/lazybox/coreutils.c
endif

# Deduplicate
CONFIG_SRCS-y := $(sort $(CONFIG_SRCS-y))

# Rust & C++ Layer Configuration
ifeq ($(ARCH), x86_64)
    RUSTC ?= rustc
    RUST_SRCS = $(shell find rust/src -name '*.rs' 2>/dev/null)
    RUST_LIB = $(BUILD_DIR)/rust/librust_kernel.a
    CXX ?= g++
    CXXFLAGS = -ffreestanding -fno-pie -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
               -mcmodel=kernel -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
               $(if $(CONFIG_OPTIMIZATION),$(CONFIG_OPTIMIZATION),-O2) -Iinclude -D__x86_64__ \
               -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
    CPP_SRCS = $(shell find kernel/cpp -name '*.cpp' 2>/dev/null)
    CPP_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CPP_SRCS))
    ALL_C_SRCS = $(ARCH_C_SRCS) $(CONFIG_SRCS-y)
    C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ALL_C_SRCS))
    ASM_OBJS   = $(patsubst %.asm, $(BUILD_DIR)/%.o, $(ARCH_ASM_SRCS))
    OTHER_OBJS = $(filter-out $(ENTRY_OBJ), $(ASM_OBJS) $(C_OBJS)) $(CPP_OBJS) $(RUST_LIB)
else
    CONFIG_SRCS-y += rust/rust_fallback.c kernel/cpp/cpp_fallback.c
    ALL_C_SRCS = $(ARCH_C_SRCS) $(CONFIG_SRCS-y)
    C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ALL_C_SRCS))
    S_OBJS     = $(patsubst %.S, $(BUILD_DIR)/%.o, $(ARCH_S_SRCS))
    OTHER_OBJS = $(filter-out $(ENTRY_OBJ), $(S_OBJS) $(C_OBJS))
endif

# Generated dependency files, one per object. The include lives at the very
# end of this file: a .d file's first rule would otherwise become make's
# default goal and turn a bare `make` into a no-op.
DEPS = $(C_OBJS:.o=.d) $(CPP_OBJS:.o=.d) $(S_OBJS:.o=.d)

.DEFAULT_GOAL := all

.PHONY: all clean run run-fullscreen debug info configure menuconfig config tui nconfig defconfig x86_64_defconfig aarch64_defconfig armv8_defconfig armv8i_defconfig armv81_defconfig arm32_defconfig qemu help

all: $(TARGET)

# -----------------------------------------------------------------------------
# Configuration Targets (Linux Kconfig TUI Configurator)
# -----------------------------------------------------------------------------
configure menuconfig config tui nconfig:
	@python3 scripts/kconfig/menuconfig.py

defconfig:
	@python3 scripts/kconfig/menuconfig.py --defconfig $(ARCH)

x86_64_defconfig:
	@python3 scripts/kconfig/menuconfig.py --x86_64

aarch64_defconfig:
	@python3 scripts/kconfig/menuconfig.py --aarch64

armv8_defconfig armv8i_defconfig armv81_defconfig arm32_defconfig:
	@python3 scripts/kconfig/menuconfig.py --armv8i

# -----------------------------------------------------------------------------
# Compilation Rules
# -----------------------------------------------------------------------------
# Header dependency tracking: without this a header change leaves stale objects
# behind, and a struct layout edit silently corrupts every unrebuilt caller.
DEPFLAGS = -MMD -MP

# C Source Files
$(BUILD_DIR)/%.o: %.c include/config/autoconf.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# C++ Source Files
$(BUILD_DIR)/%.o: %.cpp include/config/autoconf.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# x86_64 NASM Assembly Files
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

# ARM/AArch64 GNU Assembly Files (.S)
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Rust Source Compilation
$(RUST_LIB): rust/src/lib.rs $(RUST_SRCS)
	@mkdir -p $(dir $@)
	$(RUSTC) --crate-type staticlib -C panic=abort -C relocation-model=static -C opt-level=2 -C no-redzone=y -o $@ rust/src/lib.rs


# Header Generation Fallback
include/config/autoconf.h:
	@mkdir -p include/config
	@if [ ! -f .config ]; then python3 scripts/kconfig/menuconfig.py --defconfig $(ARCH); fi

# -----------------------------------------------------------------------------
# Link & Image Assembly
# -----------------------------------------------------------------------------
$(BUILD_DIR)/boot/boot.bin: $(BOOT_DIR)/boot.asm | $(BUILD_DIR)/boot
	$(ASM) $(ASMFLAGS_BOOT) $< -o $@

$(BUILD_DIR)/boot/stage2.bin: $(BOOT_DIR)/stage2.asm $(BOOT_DIR)/gdt.asm | $(BUILD_DIR)/boot
	$(ASM) $(ASMFLAGS_BOOT) $< -o $@

$(BUILD_DIR)/boot:
	mkdir -p $(BUILD_DIR)/boot

LIBGCC ?= $(shell $(CC) $(CFLAGS) -print-libgcc-file-name 2>/dev/null)

$(BUILD_DIR)/kernel.elf: $(ENTRY_OBJ) $(OTHER_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(ENTRY_OBJ) $(OTHER_OBJS) $(LIBGCC)
	@echo "=== [Kernel ELF Linked: $@ (Architecture: $(ARCH))] ==="

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(IMAGE): $(BUILD_DIR)/boot/boot.bin $(BUILD_DIR)/boot/stage2.bin $(BUILD_DIR)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$(BUILD_DIR)/boot/boot.bin of=$@ bs=512 conv=notrunc status=none
	dd if=$(BUILD_DIR)/boot/stage2.bin of=$@ bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=16 conv=notrunc status=none
	@echo "=== [SUB-OS x86_64 Disk Image Built: $@ (Size: $$(du -h $@ | cut -f1))] ==="

# -----------------------------------------------------------------------------
# Execution & Emulation Targets
# -----------------------------------------------------------------------------
run qemu: $(TARGET)
	$(QEMU_CMD)

# -vga std gives the Bochs VBE adapter the kernel drives. vgamem_mb is raised
# because a 1280x720 32-bit framebuffer needs more than the 8 MB default.
QEMU_VGA = -device VGA,vgamem_mb=32

run-gui: $(TARGET)
ifeq ($(ARCH), x86_64)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE) \
		-netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80 \
		-device e1000,netdev=net0 -serial stdio
else ifneq ($(filter $(ARCH), aarch64 arm64),)
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -kernel $(BUILD_DIR)/kernel.elf -serial stdio
else
	qemu-system-arm -M virt -cpu cortex-a15 -m 128M -kernel $(BUILD_DIR)/kernel.elf -serial stdio
endif

run-server: $(IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE) \
		-netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80 \
		-device e1000,netdev=net0 -serial stdio -display none

debug: $(TARGET)
ifeq ($(ARCH), x86_64)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE) \
		-netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80 \
		-device e1000,netdev=net0 -serial stdio -s -S
else ifneq ($(filter $(ARCH), aarch64 arm64),)
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic -kernel $(BUILD_DIR)/kernel.elf -s -S
else
	qemu-system-arm -M virt -cpu cortex-a15 -m 128M -nographic -kernel $(BUILD_DIR)/kernel.elf -s -S
endif

info:
	@echo "Target Architecture: $(ARCH)"
	@echo "Cross Compiler:      $(CROSS_COMPILE)"
	@echo "Compiled Sources:    $(words $(ALL_C_SRCS)) C files"
	@echo "Target Binary:       $(TARGET)"

help:
	@echo "SUB-OS Kernel Build System (Linux-Style Kconfig)"
	@echo "=================================================="
	@echo "  make configure        - Launch interactive Linux-style TUI Configurator"
	@echo "  make menuconfig       - Alias for make configure"
	@echo "  make defconfig        - Reset to default configuration for active ARCH"
	@echo "  make x86_64_defconfig - Load x86_64 default configuration"
	@echo "  make aarch64_defconfig- Load AArch64 default configuration"
	@echo "  make armv8i_defconfig - Load ARMv8i (32-bit ARM) default configuration"
	@echo "  make all [ARCH=...]   - Compile kernel image for selected architecture"
	@echo "  make qemu [ARCH=...]  - Boot the compiled kernel in QEMU emulator"
	@echo "  make clean            - Remove all compiled objects and disk images"
	@echo "  make info             - Display build configuration and source metrics"

clean:
	rm -rf $(BUILD_DIR) $(IMAGE)

-include $(DEPS)
