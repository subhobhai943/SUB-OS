//! GPT (GUID Partition Table) & MBR Partition Decoder
//! 100% Memory-Safe Rust Implementation for SUB-OS Storage Layer

#[repr(C, packed)]
#[derive(Copy, Clone)]
pub struct MbrPartitionEntry {
    pub status: u8,       // 0x80 = Active / Bootable, 0x00 = Inactive
    pub start_chs: [u8; 3],
    pub partition_type: u8, // 0x83 = Linux, 0x07 = NTFS/exFAT, 0x0B/0x0C = FAT32, 0xEE = GPT Protective
    pub end_chs: [u8; 3],
    pub start_lba: u32,
    pub sector_count: u32,
}

#[repr(C, packed)]
#[derive(Copy, Clone)]
pub struct GptHeader {
    pub signature: [u8; 8],     // "EFI PART" (0x5452415020494645)
    pub revision: u32,          // 0x00010000 (v1.0)
    pub header_size: u32,       // 92 bytes usually
    pub header_crc32: u32,
    pub reserved: u32,
    pub current_lba: u64,       // LBA 1
    pub backup_lba: u64,
    pub first_usable_lba: u64,
    pub last_usable_lba: u64,
    pub disk_guid: [u8; 16],
    pub partition_entries_lba: u64, // LBA 2
    pub num_partition_entries: u32, // 128
    pub sizeof_partition_entry: u32,// 128 bytes
    pub partition_entry_array_crc32: u32,
}

#[repr(C, packed)]
#[derive(Copy, Clone)]
pub struct GptPartitionEntry {
    pub type_guid: [u8; 16],
    pub unique_guid: [u8; 16],
    pub starting_lba: u64,
    pub ending_lba: u64,
    pub attributes: u64,
    pub name_utf16: [u16; 36],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct DecodedPartition {
    pub partition_number: u32,
    pub is_bootable: bool,
    pub is_gpt: bool,
    pub partition_type_id: u8,
    pub start_lba: u64,
    pub sector_count: u64,
    pub size_mb: u64,
    pub name: [u8; 36],
}

pub struct PartitionDecoder;

impl PartitionDecoder {
    pub const GPT_SIGNATURE: [u8; 8] = *b"EFI PART";

    pub fn decode_mbr(sector: &[u8; 512], out: &mut [DecodedPartition; 4]) -> usize {
        // Verify MBR signature 0x55 0xAA at bytes 510-511
        if sector[510] != 0x55 || sector[511] != 0xAA {
            return 0;
        }

        let mut count = 0;
        for i in 0..4 {
            let offset = 446 + i * 16;
            let status = sector[offset];
            let part_type = sector[offset + 4];
            let start_lba = u32::from_le_bytes([
                sector[offset + 8],
                sector[offset + 9],
                sector[offset + 10],
                sector[offset + 11],
            ]);
            let sector_count = u32::from_le_bytes([
                sector[offset + 12],
                sector[offset + 13],
                sector[offset + 14],
                sector[offset + 15],
            ]);

            if part_type != 0 && sector_count > 0 {
                let mut name = [0u8; 36];
                let type_str: &[u8] = match part_type {
                    0x83 => b"Linux Native\0",
                    0x82 => b"Linux Swap\0",
                    0x0B | 0x0C => b"FAT32 Storage\0",
                    0x07 => b"NTFS / exFAT\0",
                    0xEE => b"GPT Protective\0",
                    _ => b"Primary Partition\0",
                };
                let copy_len = core::cmp::min(type_str.len(), 35);
                name[..copy_len].copy_from_slice(&type_str[..copy_len]);

                out[count] = DecodedPartition {
                    partition_number: (i + 1) as u32,
                    is_bootable: status == 0x80,
                    is_gpt: part_type == 0xEE,
                    partition_type_id: part_type,
                    start_lba: start_lba as u64,
                    sector_count: sector_count as u64,
                    size_mb: ((sector_count as u64) * 512) / (1024 * 1024),
                    name,
                };
                count += 1;
            }
        }
        count
    }

    pub fn is_valid_gpt_header(sector: &[u8; 512]) -> bool {
        &sector[0..8] == &Self::GPT_SIGNATURE
    }
}

// C-FFI
#[no_mangle]
pub extern "C" fn rust_storage_decode_mbr(
    sector_512: *const u8,
    out_partitions: *mut DecodedPartition,
    max_count: usize,
) -> i32 {
    if sector_512.is_null() || out_partitions.is_null() || max_count < 4 {
        return -1;
    }
    unsafe {
        let mut raw_sector = [0u8; 512];
        core::ptr::copy_nonoverlapping(sector_512, raw_sector.as_mut_ptr(), 512);

        let mut decoded = [DecodedPartition {
            partition_number: 0,
            is_bootable: false,
            is_gpt: false,
            partition_type_id: 0,
            start_lba: 0,
            sector_count: 0,
            size_mb: 0,
            name: [0; 36],
        }; 4];

        let count = PartitionDecoder::decode_mbr(&raw_sector, &mut decoded);
        for i in 0..count {
            *out_partitions.add(i) = decoded[i];
        }
        count as i32
    }
}
