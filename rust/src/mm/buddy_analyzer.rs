//! Memory Fragmentation Analyzer & Buddy Allocator Simulator for SUB-OS
//! Computes fragmentation indices, order distribution, and memory efficiency in pure no_std Rust.

use crate::kprintln;

pub const BUDDY_MAX_ORDER: usize = 11; // Order 0 (4KB) to Order 10 (4MB)

#[derive(Debug, Clone, Copy)]
pub struct BuddyStats {
    pub total_pages: u64,
    pub free_pages: u64,
    pub order_counts: [u32; BUDDY_MAX_ORDER],
    pub fragmentation_index: u32, // Percentage (0-100%)
    pub largest_order: usize,
}

static mut GLOBAL_BUDDY_STATS: BuddyStats = BuddyStats {
    total_pages: 0,
    free_pages: 0,
    order_counts: [0; BUDDY_MAX_ORDER],
    fragmentation_index: 0,
    largest_order: 0,
};

pub fn compute_buddy_distribution(total_pages: u64, free_pages: u64) -> BuddyStats {
    let mut order_counts = [0u32; BUDDY_MAX_ORDER];
    let mut remaining = free_pages;
    let mut largest_order = 0;

    // Distribute free pages in realistic power-of-two buddy blocks
    for order in (0..BUDDY_MAX_ORDER).rev() {
        let block_size = 1u64 << order;
        if remaining >= block_size {
            // Allocate a portion of available pages to this order
            let count = (remaining / block_size) / 2 + 1;
            let actual = count.min(remaining / block_size);
            order_counts[order] = actual as u32;
            remaining -= actual * block_size;
            if actual > 0 && order > largest_order {
                largest_order = order;
            }
        }
    }
    if remaining > 0 {
        order_counts[0] += remaining as u32;
    }

    // External fragmentation calculation:
    // If largest block is much smaller than total free memory, fragmentation is high
    let largest_block_pages = 1u64 << largest_order;
    let frag_index = if free_pages > 0 && free_pages > largest_block_pages {
        let ratio = ((free_pages - largest_block_pages) * 100) / free_pages;
        ratio.min(99) as u32
    } else {
        0
    };

    BuddyStats {
        total_pages,
        free_pages,
        order_counts,
        fragmentation_index: frag_index,
        largest_order,
    }
}

pub fn dump_buddy_info() {
    let stats = unsafe { GLOBAL_BUDDY_STATS };
    kprintln!("\x1b[96m=== SUB-OS Memory Fragmentation & Buddy Allocator Telemetry ===\x1b[0m");
    kprintln!("  Total Pages      : \x1b[93m{}\x1b[0m ({} MB)", stats.total_pages, (stats.total_pages * 4) / 1024);
    kprintln!("  Free Pages       : \x1b[92m{}\x1b[0m ({} MB)", stats.free_pages, (stats.free_pages * 4) / 1024);
    kprintln!("  Fragmentation    : \x1b[{};1m{}%\x1b[0m (External Memory Fragmentation)",
              if stats.fragmentation_index > 50 { "91" } else { "92" }, stats.fragmentation_index);
    kprintln!("  Largest Block    : Order {} ({} KB Contiguous)", stats.largest_order, (1 << stats.largest_order) * 4);
    kprintln!("-----------------------------------------------------------------");
    kprintln!("ORDER   BLOCK SIZE    FREE BLOCKS    TOTAL MEMORY");
    kprintln!("-----------------------------------------------------------------");

    for order in 0..BUDDY_MAX_ORDER {
        let block_kb = (1 << order) * 4;
        let count = stats.order_counts[order];
        let total_kb = count as u64 * block_kb as u64;
        kprintln!("{:<6}  {:<10}    {:<11}    {} KB",
                 order,
                 if block_kb >= 1024 { ">= 1 MB" } else { "4-512 KB" },
                 count, total_kb);
    }
    kprintln!("");
}

// -----------------------------------------------------------------------------
// C-FFI Bridge Exports
// -----------------------------------------------------------------------------
#[no_mangle]
pub extern "C" fn rust_buddy_analyze(total_pages: u64, free_pages: u64) {
    let stats = compute_buddy_distribution(total_pages, free_pages);
    unsafe {
        GLOBAL_BUDDY_STATS = stats;
    }
}

#[no_mangle]
pub extern "C" fn rust_buddy_dump_stats() {
    dump_buddy_info();
}
