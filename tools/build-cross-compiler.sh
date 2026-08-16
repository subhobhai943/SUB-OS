#!/bin/bash
# =============================================================================
# SUB-OS Cross-Compiler Build Script
# Builds x86_64-elf-gcc and x86_64-elf-binutils from source
# =============================================================================

set -e

# Configuration
export TARGET=x86_64-elf
export PREFIX="/usr/local/cross"
export PATH="$PREFIX/bin:$PATH"

BINUTILS_VERSION="2.42"
GCC_VERSION="13.3.0"

BUILD_DIR="/tmp/cross-compiler-build"
JOBS=$(nproc)

echo "============================================"
echo "  Building x86_64-elf Cross-Compiler"
echo "============================================"
echo "  Target:   $TARGET"
echo "  Prefix:   $PREFIX"
echo "  Binutils: $BINUTILS_VERSION"
echo "  GCC:      $GCC_VERSION"
echo "  Jobs:     $JOBS"
echo "============================================"

# Create directories
sudo mkdir -p "$PREFIX"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# ---- Step 1: Download sources ----
echo ""
echo "[1/4] Downloading sources..."

if [ ! -f "binutils-${BINUTILS_VERSION}.tar.xz" ]; then
    curl -LO "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
fi

if [ ! -f "gcc-${GCC_VERSION}.tar.xz" ]; then
    curl -LO "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
fi

# ---- Step 2: Extract sources ----
echo ""
echo "[2/4] Extracting sources..."

if [ ! -d "binutils-${BINUTILS_VERSION}" ]; then
    tar xf "binutils-${BINUTILS_VERSION}.tar.xz"
fi

if [ ! -d "gcc-${GCC_VERSION}" ]; then
    tar xf "gcc-${GCC_VERSION}.tar.xz"
fi

# ---- Step 3: Build Binutils ----
echo ""
echo "[3/4] Building binutils for ${TARGET}..."

mkdir -p build-binutils
cd build-binutils

../binutils-${BINUTILS_VERSION}/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

make -j$JOBS
sudo make install

cd "$BUILD_DIR"

# ---- Step 4: Build GCC ----
echo ""
echo "[4/4] Building GCC for ${TARGET}..."

mkdir -p build-gcc
cd build-gcc

../gcc-${GCC_VERSION}/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers

make -j$JOBS all-gcc
make -j$JOBS all-target-libgcc
sudo make install-gcc
sudo make install-target-libgcc

cd "$BUILD_DIR"

# ---- Verify ----
echo ""
echo "============================================"
echo "  Cross-Compiler Build Complete!"
echo "============================================"
echo ""
$PREFIX/bin/$TARGET-gcc --version
$PREFIX/bin/$TARGET-ld --version | head -1
echo ""
echo "Installed to: $PREFIX/bin/"
echo ""
echo "Add to your PATH:"
echo "  export PATH=\"$PREFIX/bin:\$PATH\""
echo ""
echo "Or add to ~/.bashrc for persistence:"
echo "  echo 'export PATH=\"$PREFIX/bin:\$PATH\"' >> ~/.bashrc"
echo "============================================"
