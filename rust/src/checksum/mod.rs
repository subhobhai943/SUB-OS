//! Memory-safe checksum and non-cryptographic hash engine for SUB-OS.
//!
//! Implements CRC-32C (Castagnoli, the checksum used by ext4 metadata, iSCSI
//! and SSE4.2's `crc32` instruction), the Adler-32 rolling checksum from zlib,
//! and the 64-bit FNV-1a hash used for fast in-kernel hash-table keying. All
//! three are pure `no_std` computations over borrowed slices -- no allocation,
//! no unsafe outside the thin FFI shims.

/// CRC-32C (Castagnoli) using the reflected polynomial 0x82F63B78. Computed
/// bit-by-bit so no lookup table has to live in the kernel image.
pub fn crc32c(data: &[u8]) -> u32 {
    let mut crc: u32 = 0xFFFF_FFFF;
    for &byte in data {
        crc ^= byte as u32;
        for _ in 0..8 {
            let mask = (crc & 1).wrapping_neg(); // 0xFFFFFFFF if LSB set, else 0
            crc = (crc >> 1) ^ (0x82F6_3B78 & mask);
        }
    }
    !crc
}

/// Adler-32 (zlib): two running 16-bit sums modulo 65521.
pub fn adler32(data: &[u8]) -> u32 {
    const MOD: u32 = 65521;
    let mut a: u32 = 1;
    let mut b: u32 = 0;
    for &byte in data {
        a = (a + byte as u32) % MOD;
        b = (b + a) % MOD;
    }
    (b << 16) | a
}

/// 64-bit FNV-1a hash. Good spread and speed for hash-table keys.
pub fn fnv1a64(data: &[u8]) -> u64 {
    const OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
    const PRIME: u64 = 0x0000_0100_0000_01B3;
    let mut hash = OFFSET;
    for &byte in data {
        hash ^= byte as u64;
        hash = hash.wrapping_mul(PRIME);
    }
    hash
}

// ---------------------------------------------------------------------------
// C ABI shims
// ---------------------------------------------------------------------------

#[no_mangle]
pub extern "C" fn rust_crc32c(data: *const u8, len: usize) -> u32 {
    if data.is_null() {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(data, len) };
    crc32c(bytes)
}

#[no_mangle]
pub extern "C" fn rust_adler32(data: *const u8, len: usize) -> u32 {
    if data.is_null() {
        return 1; // Adler-32 of the empty string
    }
    let bytes = unsafe { core::slice::from_raw_parts(data, len) };
    adler32(bytes)
}

#[no_mangle]
pub extern "C" fn rust_fnv1a64(data: *const u8, len: usize) -> u64 {
    if data.is_null() {
        return 0xcbf2_9ce4_8422_2325;
    }
    let bytes = unsafe { core::slice::from_raw_parts(data, len) };
    fnv1a64(bytes)
}
