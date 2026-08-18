use core::fmt::{self, Write};

extern "C" {
    fn printk(fmt: *const u8, ...) -> i32;
}

pub struct KernelWriter;

impl Write for KernelWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for byte in s.bytes() {
            let buf = [byte, 0];
            unsafe {
                printk(b"%c\0".as_ptr(), buf[0] as i32);
            }
        }
        Ok(())
    }
}

pub fn _print(args: fmt::Arguments) {
    let mut writer = KernelWriter;
    let _ = writer.write_fmt(args);
}

#[macro_export]
macro_rules! kprint {
    ($($arg:tt)*) => ($crate::printk::_print(format_args!($($arg)*)));
}

#[macro_export]
macro_rules! kprintln {
    () => ($crate::kprint!("\n"));
    ($($arg:tt)*) => ($crate::kprint!("{}\n", format_args!($($arg)*)));
}
