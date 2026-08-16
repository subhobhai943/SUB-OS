#!/bin/bash

echo "Setting up SUB-OS build environment..."

if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo "Cannot detect OS. Please manually install dependencies."
    exit 1
fi

case $OS in
    ubuntu|debian)
        echo "Detected Debian/Ubuntu-based OS."
        sudo apt-get update
        sudo apt-get install -y nasm gcc binutils make qemu-system-x86
        ;;
    fedora)
        echo "Detected Fedora."
        sudo dnf install -y nasm gcc binutils make qemu-system-x86
        ;;
    arch)
        echo "Detected Arch Linux."
        sudo pacman -Sy --noconfirm nasm gcc binutils make qemu-system-x86
        ;;
    *)
        echo "Unsupported OS: $OS. Please install: nasm, gcc, binutils, make, qemu-system-x86."
        ;;
esac

if ! command -v x86_64-elf-gcc &> /dev/null; then
    echo ""
    echo "WARNING: x86_64-elf-gcc (cross-compiler) not found in PATH."
    echo "The Makefile will fallback to the default gcc."
    echo "For the best results, it is highly recommended to build an x86_64-elf GCC cross-compiler."
    echo "Instructions to build a cross-compiler: https://wiki.osdev.org/GCC_Cross-Compiler"
fi

echo "Setup script finished!"
