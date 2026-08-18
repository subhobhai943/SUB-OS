//! Memory-Safe Packet Filter & Firewall Rule Engine
//! Pure Rust Implementation for SUB-OS NetFilter Subsystem

pub const FILTER_ACTION_ACCEPT: u8 = 0;
pub const FILTER_ACTION_DROP: u8   = 1;
pub const FILTER_ACTION_REJECT: u8 = 2;
pub const FILTER_ACTION_LOG: u8    = 3;

pub const PROTO_ANY: u8  = 0;
pub const PROTO_ICMP: u8 = 1;
pub const PROTO_TCP: u8  = 6;
pub const PROTO_UDP: u8  = 17;

const MAX_RULES: usize = 32;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct FilterRule {
    pub enabled: bool,
    pub protocol: u8,
    pub src_ip: u32,
    pub src_mask: u32,
    pub dst_ip: u32,
    pub dst_mask: u32,
    pub src_port_start: u16,
    pub src_port_end: u16,
    pub dst_port_start: u16,
    pub dst_port_end: u16,
    pub action: u8,
    pub packet_count: u64,
    pub byte_count: u64,
}

#[repr(C)]
pub struct PacketHeader {
    pub protocol: u8,
    pub src_ip: u32,
    pub dst_ip: u32,
    pub src_port: u16,
    pub dst_port: u16,
    pub length: u32,
}

pub struct PacketFilter {
    rules: [FilterRule; MAX_RULES],
    rule_count: usize,
    default_policy: u8,
    total_processed: u64,
    total_blocked: u64,
}

impl PacketFilter {
    pub const fn new() -> Self {
        Self {
            rules: [FilterRule {
                enabled: false,
                protocol: 0,
                src_ip: 0,
                src_mask: 0,
                dst_ip: 0,
                dst_mask: 0,
                src_port_start: 0,
                src_port_end: 0,
                dst_port_start: 0,
                dst_port_end: 0,
                action: FILTER_ACTION_ACCEPT,
                packet_count: 0,
                byte_count: 0,
            }; MAX_RULES],
            rule_count: 0,
            default_policy: FILTER_ACTION_ACCEPT,
            total_processed: 0,
            total_blocked: 0,
        }
    }

    pub fn add_rule(&mut self, rule: FilterRule) -> bool {
        if self.rule_count < MAX_RULES {
            self.rules[self.rule_count] = rule;
            self.rule_count += 1;
            true
        } else {
            false
        }
    }

    pub fn evaluate(&mut self, pkt: &PacketHeader) -> u8 {
        self.total_processed += 1;

        for i in 0..self.rule_count {
            let r = &mut self.rules[i];
            if !r.enabled {
                continue;
            }

            // Check protocol
            if r.protocol != PROTO_ANY && r.protocol != pkt.protocol {
                continue;
            }

            // Check Source IP / Mask
            if r.src_mask != 0 && (pkt.src_ip & r.src_mask) != (r.src_ip & r.src_mask) {
                continue;
            }

            // Check Dest IP / Mask
            if r.dst_mask != 0 && (pkt.dst_ip & r.dst_mask) != (r.dst_ip & r.dst_mask) {
                continue;
            }

            // Check Ports
            if r.dst_port_start != 0 && (pkt.dst_port < r.dst_port_start || pkt.dst_port > r.dst_port_end) {
                continue;
            }

            // Match Found!
            r.packet_count += 1;
            r.byte_count += pkt.length as u64;

            if r.action == FILTER_ACTION_DROP || r.action == FILTER_ACTION_REJECT {
                self.total_blocked += 1;
            }
            return r.action;
        }

        self.default_policy
    }

    pub fn stats(&self) -> (u64, u64, usize) {
        (self.total_processed, self.total_blocked, self.rule_count)
    }
}

static mut GLOBAL_FILTER: PacketFilter = PacketFilter::new();

// C-FFI
#[no_mangle]
pub extern "C" fn rust_filter_evaluate(pkt: *const PacketHeader) -> u8 {
    if pkt.is_null() {
        return FILTER_ACTION_DROP;
    }
    unsafe {
        GLOBAL_FILTER.evaluate(&*pkt)
    }
}

#[no_mangle]
pub extern "C" fn rust_filter_add_rule(rule: *const FilterRule) -> i32 {
    if rule.is_null() {
        return -1;
    }
    unsafe {
        if GLOBAL_FILTER.add_rule(*rule) { 0 } else { -1 }
    }
}

#[no_mangle]
pub extern "C" fn rust_filter_get_stats(out_processed: *mut u64, out_blocked: *mut u64, out_rule_count: *mut usize) {
    unsafe {
        let (p, b, rc) = GLOBAL_FILTER.stats();
        if !out_processed.is_null() { *out_processed = p; }
        if !out_blocked.is_null() { *out_blocked = b; }
        if !out_rule_count.is_null() { *out_rule_count = rc; }
    }
}
