# =============================================================================
# SUB-OS Master Multi-Architecture Linux-Style Makefile
# Supports: x86_64, aarch64, armv8i
# Features: Linux Kconfig TUI (make menuconfig), multi-arch cross-compilation
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
              -O2 \
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
                    -device e1000,netdev=net0 -serial stdio

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
              -O2 \
              -Iinclude \
              -D__aarch64__ \
              -march=armv8-a

    LDFLAGS = -nostdlib -no-pie -T arch/aarch64/linker.ld

    ARCH_C_SRCS   = $(shell find arch/aarch64 -name '*.c' 2>/dev/null)
    ARCH_S_SRCS   = $(shell find arch/aarch64 -name '*.S' 2>/dev/null)
    ENTRY_OBJ     = $(BUILD_DIR)/arch/aarch64/boot/entry.o
    TARGET        = $(BUILD_DIR)/kernel.elf
    QEMU_CMD      = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic -kernel $(BUILD_DIR)/kernel.elf

else ifneq ($(filter $(ARCH), armv8i arm32 arm),)
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
              -O2 \
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
# Common Kernel Subsystems
# -----------------------------------------------------------------------------
KERNEL_C_SRCS   = $(shell find kernel -name '*.c' 2>/dev/null)
MM_C_SRCS       = $(shell find mm -name '*.c' 2>/dev/null)
DRIVERS_C_SRCS  = $(shell find drivers -name '*.c' 2>/dev/null)
FS_C_SRCS       = $(shell find fs -name '*.c' 2>/dev/null)
NET_C_SRCS      = $(shell find net -name '*.c' 2>/dev/null)
CRYPTO_C_SRCS   = $(shell find crypto -name '*.c' 2>/dev/null)
LIB_C_SRCS      = $(shell find lib -name '*.c' 2>/dev/null)
USERLAND_C_SRCS = $(shell find userland -name '*.c' 2>/dev/null)
CERTS_C_SRCS    = $(shell find certs -name '*.c' 2>/dev/null)
INIT_C_SRCS     = $(shell find init -name '*.c' 2>/dev/null)
IO_URING_C_SRCS = $(shell find io_uring -name '*.c' 2>/dev/null)
SECURITY_C_SRCS = $(shell find security -name '*.c' 2>/dev/null)
USR_C_SRCS      = $(shell find usr -name '*.c' 2>/dev/null)
VIRT_C_SRCS     = $(shell find virt -name '*.c' 2>/dev/null)
BLOCK_C_SRCS    = $(shell find block -name '*.c' 2>/dev/null)
IPC_C_SRCS      = $(shell find ipc -name '*.c' 2>/dev/null)
SOUND_C_SRCS    = $(shell find sound -name '*.c' 2>/dev/null)

ALL_C_SRCS = $(ARCH_C_SRCS) $(KERNEL_C_SRCS) $(MM_C_SRCS) $(DRIVERS_C_SRCS) \
             $(FS_C_SRCS) $(NET_C_SRCS) $(CRYPTO_C_SRCS) $(LIB_C_SRCS) $(USERLAND_C_SRCS) \
             $(CERTS_C_SRCS) $(INIT_C_SRCS) $(IO_URING_C_SRCS) $(SECURITY_C_SRCS) \
             $(USR_C_SRCS) $(VIRT_C_SRCS) $(BLOCK_C_SRCS) $(IPC_C_SRCS) $(SOUND_C_SRCS)

# Object Files Mapping
C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ALL_C_SRCS))

ifeq ($(ARCH), x86_64)
    ASM_OBJS   = $(patsubst %.asm, $(BUILD_DIR)/%.o, $(ARCH_ASM_SRCS))
    OTHER_OBJS = $(filter-out $(ENTRY_OBJ), $(ASM_OBJS) $(C_OBJS))
else
    S_OBJS     = $(patsubst %.S, $(BUILD_DIR)/%.o, $(ARCH_S_SRCS))
    OTHER_OBJS = $(filter-out $(ENTRY_OBJ), $(S_OBJS) $(C_OBJS))
endif

.PHONY: all clean run debug info menuconfig config defconfig x86_64_defconfig aarch64_defconfig armv8_defconfig armv8i_defconfig qemu

all: $(TARGET)

# -----------------------------------------------------------------------------
# Configuration Targets (Linux Kconfig TUI)
# -----------------------------------------------------------------------------
menuconfig config:
	@python3 scripts/kconfig/menuconfig.py

defconfig:
	@python3 scripts/kconfig/menuconfig.py --defconfig $(ARCH)

x86_64_defconfig:
	@python3 scripts/kconfig/menuconfig.py --x86_64

aarch64_defconfig:
	@python3 scripts/kconfig/menuconfig.py --aarch64

armv8_defconfig armv8i_defconfig arm32_defconfig:
	@python3 scripts/kconfig/menuconfig.py --armv8i

# -----------------------------------------------------------------------------
# Compilation Rules
# -----------------------------------------------------------------------------
# C Source Files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# x86_64 NASM Assembly Files
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

# ARM/AArch64 GNU Assembly Files (.S)
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# x86_64 Bootloader & Image Rules
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
	@echo "C Sources:           $(words $(ALL_C_SRCS)) files"
	@echo "Target Binary:       $(TARGET)"

clean:
	rm -rf $(BUILD_DIR) $(IMAGE)
