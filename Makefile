# =============================================================================
# SUB-OS Master Makefile - Modular Monolithic Linux-Like Kernel Build System
# =============================================================================

# Cross Compiler & Toolchain Detection
CROSS_COMPILE ?= x86_64-elf-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
ASM     = nasm

# Build Directories
BUILD_DIR = build
BOOT_DIR  = boot

# Target Architecture Flags
CFLAGS = -ffreestanding \
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
         -Iinclude

ASMFLAGS_ELF  = -f elf64
ASMFLAGS_BOOT = -f bin -I$(BOOT_DIR)/

LDFLAGS = -nostdlib -no-pie -T linker.ld

# Source Files Discovery
ARCH_C_SRCS     = $(shell find arch -name '*.c' 2>/dev/null)
ARCH_ASM_SRCS   = $(shell find arch -name '*.asm' 2>/dev/null)
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

ALL_C_SRCS = $(ARCH_C_SRCS) $(KERNEL_C_SRCS) $(MM_C_SRCS) $(DRIVERS_C_SRCS) \
             $(FS_C_SRCS) $(NET_C_SRCS) $(CRYPTO_C_SRCS) $(LIB_C_SRCS) $(USERLAND_C_SRCS) \
             $(CERTS_C_SRCS) $(INIT_C_SRCS) $(IO_URING_C_SRCS) $(SECURITY_C_SRCS) \
             $(USR_C_SRCS) $(VIRT_C_SRCS)

# Object Files Mapping
C_OBJS   = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ALL_C_SRCS))
ASM_OBJS = $(patsubst %.asm, $(BUILD_DIR)/%.o, $(ARCH_ASM_SRCS))

# Put entry.o first in linker sequence
ENTRY_OBJ = $(BUILD_DIR)/arch/x86_64/boot/entry.o
OTHER_OBJS = $(filter-out $(ENTRY_OBJ), $(ASM_OBJS) $(C_OBJS))

IMAGE = SUB-OS.img

.PHONY: all clean run debug info

all: $(IMAGE)

# Assemble 16-bit Bootloader Stage 1
$(BUILD_DIR)/boot/boot.bin: $(BOOT_DIR)/boot.asm | $(BUILD_DIR)/boot
	$(ASM) $(ASMFLAGS_BOOT) $< -o $@

# Assemble 16/32/64-bit Bootloader Stage 2
$(BUILD_DIR)/boot/stage2.bin: $(BOOT_DIR)/stage2.asm $(BOOT_DIR)/gdt.asm | $(BUILD_DIR)/boot
	$(ASM) $(ASMFLAGS_BOOT) $< -o $@

# Compile 64-bit C source files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble 64-bit ASM source files
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

# Link Kernel ELF
$(BUILD_DIR)/kernel.elf: $(ENTRY_OBJ) $(OTHER_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ENTRY_OBJ) $(OTHER_OBJS)

# Convert ELF to Flat Binary
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# Create Bootable Floppy / HDD Raw Disk Image
$(IMAGE): $(BUILD_DIR)/boot/boot.bin $(BUILD_DIR)/boot/stage2.bin $(BUILD_DIR)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$(BUILD_DIR)/boot/boot.bin of=$@ bs=512 conv=notrunc status=none
	dd if=$(BUILD_DIR)/boot/stage2.bin of=$@ bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=16 conv=notrunc status=none
	@echo "=== [SUB-OS Disk Image Built Successfully: $@ (Size: $$(du -h $@ | cut -f1))] ==="

$(BUILD_DIR)/boot:
	mkdir -p $(BUILD_DIR)/boot

run: $(IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE) -nic model=e1000 -serial stdio

debug: $(IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE) -nic model=e1000 -serial stdio -s -S

info:
	@echo "C Sources:   $(words $(ALL_C_SRCS)) files"
	@echo "ASM Sources: $(words $(ARCH_ASM_SRCS)) files"
	@echo "Target:      $(IMAGE)"

clean:
	rm -rf $(BUILD_DIR) $(IMAGE)
