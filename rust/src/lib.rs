//! SUB-OS Rust Kernel Layer (no_std)
//! Integrates memory-safe drivers, cryptography, and userland telemetry.

#![no_std]
#![no_main]

pub mod printk;
pub mod drivers;

#[no_mangle]
pub extern "C" fn rust_eh_personality() {}

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    kprintln!("\n[RUST KERNEL PANIC] An unrecoverable panic occurred in Rust subsystem!");
    if let Some(location) = info.location() {
        kprintln!("  Location: {}:{}", location.file(), location.line());
    }
    loop {}
}

#[no_mangle]
pub extern "C" fn rust_kernel_init() -> i32 {
    kprintln!(
        "\x1b[32mRUST:\x1b[0m Rust Core Subsystem active (Rustc 1.75+ \x1b[36mno_std\x1b[0m bare-metal mode)"
    );

    // Initialize ChaCha20 Secure CSPRNG
    drivers::chacha20::csprng_init();
    kprintln!(
        "\x1b[32mRUST:\x1b[0m ChaCha20 RFC-8439 Cryptographic Stream Engine online"
    );

    0
}

#[no_mangle]
pub extern "C" fn rust_kernel_status() -> *const u8 {
    b"SUB-OS Rust Subsystem v0.2.0 (Active: ChaCha20, HWMON Thermal Governor, CSPRNG)\0".as_ptr()
}
