//! Memory-Safe ELF-64 and ELF-32 Binary Header Parser for SUB-OS
//! Performs zero-copy verification of ELF magic, machine architectures, and program headers.

use crate::kprintln;

pub const ELF_MAGIC: [u8; 4] = [0x7F, b'E', b'L', b'F'];

pub const ELFCLASS32: u8 = 1;
pub const ELFCLASS64: u8 = 2;

pub const ELFDATA2LSB: u8 = 1; // Little endian

pub const EM_386: u16 = 0x03;
pub const EM_ARM: u16 = 0x28;
pub const EM_X86_64: u16 = 0x3E;
pub const EM_AARCH64: u16 = 0xB7;

pub const PT_NULL: u32 = 0;
pub const PT_LOAD: u32 = 1;
pub const PT_DYNAMIC: u32 = 2;
pub const PT_INTERP: u32 = 3;
pub const PT_NOTE: u32 = 4;
pub const PT_SHLIB: u32 = 5;
pub const PT_PHDR: u32 = 6;
pub const PT_TLS: u32 = 7;
pub const PT_GNU_STACK: u32 = 0x6474e551;
pub const PT_GNU_RELRO: u32 = 0x6474e552;

#[derive(Debug, Clone, Copy)]
pub struct ElfHeader {
    pub is_64bit: bool,
    pub machine: u16,
    pub entry_point: u64,
    pub ph_offset: u64,
    pub ph_entry_size: u16,
    pub ph_count: u16,
}

#[derive(Debug, Clone, Copy)]
pub struct ProgramHeader {
    pub p_type: u32,
    pub p_flags: u32,
    pub p_offset: u64,
    pub p_vaddr: u64,
    pub p_paddr: u64,
    pub p_filesz: u64,
    pub p_memsz: u64,
    pub p_align: u64,
}

fn read_u16_le(slice: &[u8], offset: usize) -> u16 {
    if offset + 2 > slice.len() { return 0; }
    u16::from_le_bytes([slice[offset], slice[offset + 1]])
}

fn read_u32_le(slice: &[u8], offset: usize) -> u32 {
    if offset + 4 > slice.len() { return 0; }
    u32::from_le_bytes([slice[offset], slice[offset + 1], slice[offset + 2], slice[offset + 3]])
}

fn read_u64_le(slice: &[u8], offset: usize) -> u64 {
    if offset + 8 > slice.len() { return 0; }
    u64::from_le_bytes([
        slice[offset], slice[offset + 1], slice[offset + 2], slice[offset + 3],
        slice[offset + 4], slice[offset + 5], slice[offset + 6], slice[offset + 7],
    ])
}

pub fn parse_elf_header(data: &[u8]) -> Result<ElfHeader, &'static str> {
    if data.len() < 52 {
        return Err("Buffer too small for ELF header");
    }

    if data[0..4] != ELF_MAGIC {
        return Err("Invalid ELF magic");
    }

    let is_64bit = data[4] == ELFCLASS64;
    let is_little_endian = data[5] == ELFDATA2LSB;

    if !is_little_endian {
        return Err("Only little-endian ELF supported");
    }

    let machine = read_u16_le(data, 18);

    if is_64bit {
        if data.len() < 64 {
            return Err("Buffer too small for ELF64 header");
        }
        let entry_point = read_u64_le(data, 24);
        let ph_offset = read_u64_le(data, 32);
        let ph_entry_size = read_u16_le(data, 54);
        let ph_count = read_u16_le(data, 56);

        Ok(ElfHeader {
            is_64bit: true,
            machine,
            entry_point,
            ph_offset,
            ph_entry_size,
            ph_count,
        })
    } else {
        let entry_point = read_u32_le(data, 24) as u64;
        let ph_offset = read_u32_le(data, 28) as u64;
        let ph_entry_size = read_u16_le(data, 42);
        let ph_count = read_u16_le(data, 44);

        Ok(ElfHeader {
            is_64bit: false,
            machine,
            entry_point,
            ph_offset,
            ph_entry_size,
            ph_count,
        })
    }
}

pub fn get_machine_name(machine: u16) -> &'static str {
    match machine {
        EM_X86_64 => "x86_64 (AMD64)",
        EM_AARCH64 => "AArch64 (ARM 64-bit)",
        EM_ARM => "ARM (AArch32)",
        EM_386 => "x86 (i386)",
        _ => "Unknown Machine",
    }
}

pub fn get_segment_type_name(seg_type: u32) -> &'static str {
    match seg_type {
        PT_NULL => "NULL",
        PT_LOAD => "LOAD",
        PT_DYNAMIC => "DYNAMIC",
        PT_INTERP => "INTERP",
        PT_NOTE => "NOTE",
        PT_SHLIB => "SHLIB",
        PT_PHDR => "PHDR",
        PT_TLS => "TLS",
        PT_GNU_STACK => "GNU_STACK",
        PT_GNU_RELRO => "GNU_RELRO",
        _ => "OTHER",
    }
}

pub fn dump_elf_info(data: &[u8]) {
    kprintln!("\x1b[96m=== Rust Memory-Safe ELF Binary Inspector ===\x1b[0m");

    match parse_elf_header(data) {
        Ok(hdr) => {
            kprintln!("  Class        : {}", if hdr.is_64bit { "64-bit (ELF64)" } else { "32-bit (ELF32)" });
            kprintln!("  Architecture : \x1b[93m{}\x1b[0m (0x{:04X})", get_machine_name(hdr.machine), hdr.machine);
            kprintln!("  Entry Point  : \x1b[92m0x{:016X}\x1b[0m", hdr.entry_point);
            kprintln!("  Prog Headers : {} entries (offset: 0x{:X}, size: {} B)", hdr.ph_count, hdr.ph_offset, hdr.ph_entry_size);

            if hdr.is_64bit && hdr.ph_entry_size >= 56 {
                kprintln!("-----------------------------------------------------------------");
                kprintln!("TYPE         OFFSET             VADDR              MEMSZ      FLAGS");
                kprintln!("-----------------------------------------------------------------");

                let mut off = hdr.ph_offset as usize;
                for _i in 0..hdr.ph_count as usize {
                    if off + 56 > data.len() { break; }
                    let p_type = read_u32_le(data, off);
                    let p_flags = read_u32_le(data, off + 4);
                    let p_offset = read_u64_le(data, off + 8);
                    let p_vaddr = read_u64_le(data, off + 16);
                    let p_memsz = read_u64_le(data, off + 40);

                    let mut flags_buf = [b' '; 3];
                    if (p_flags & 4) != 0 { flags_buf[0] = b'R'; }
                    if (p_flags & 2) != 0 { flags_buf[1] = b'W'; }
                    if (p_flags & 1) != 0 { flags_buf[2] = b'E'; }
                    let flags_str = core::str::from_utf8(&flags_buf).unwrap_or("   ");

                    kprintln!("{:<11}  0x{:012X}     0x{:014X}   {:<8}   {}",
                             get_segment_type_name(p_type), p_offset, p_vaddr, p_memsz, flags_str);

                    off += hdr.ph_entry_size as usize;
                }
            }
        }
        Err(e) => {
            kprintln!("  \x1b[91mError: {}\x1b[0m", e);
        }
    }
}

// -----------------------------------------------------------------------------
// C-FFI Bridge Exports
// -----------------------------------------------------------------------------
#[no_mangle]
pub extern "C" fn rust_elf_validate(buf: *const u8, len: usize) -> i32 {
    if buf.is_null() || len < 52 { return -1; }
    let slice = unsafe { core::slice::from_raw_parts(buf, len) };
    if parse_elf_header(slice).is_ok() { 0 } else { -1 }
}

#[no_mangle]
pub extern "C" fn rust_elf_get_entry(buf: *const u8, len: usize, out_entry: *mut u64) -> i32 {
    if buf.is_null() || out_entry.is_null() || len < 52 { return -1; }
    let slice = unsafe { core::slice::from_raw_parts(buf, len) };
    match parse_elf_header(slice) {
        Ok(hdr) => {
            unsafe { *out_entry = hdr.entry_point; }
            0
        }
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn rust_elf_dump(buf: *const u8, len: usize) {
    if buf.is_null() || len == 0 { return; }
    let slice = unsafe { core::slice::from_raw_parts(buf, len) };
    dump_elf_info(slice);
}
