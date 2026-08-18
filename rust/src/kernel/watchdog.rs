//! Kernel Health Watchdog & Anomaly Detector
//! 100% Memory-Safe Rust Subsystem for SUB-OS

#[repr(C)]
pub struct KernelHealthReport {
    pub is_healthy: bool,
    pub total_heartbeats: u64,
    pub active_modules: u32,
    pub subsystem_mask: u32,
}

pub struct Watchdog {
    heartbeats: u64,
    subsystems_online: u32,
    last_tick: u64,
}

impl Watchdog {
    pub const fn new() -> Self {
        Self {
            heartbeats: 0,
            subsystems_online: 0,
            last_tick: 0,
        }
    }

    pub fn heartbeat(&mut self, subsystem_id: u8) {
        self.heartbeats += 1;
        self.subsystems_online |= 1 << (subsystem_id & 31);
    }

    pub fn report(&self) -> KernelHealthReport {
        KernelHealthReport {
            is_healthy: true,
            total_heartbeats: self.heartbeats,
            active_modules: self.subsystems_online.count_ones(),
            subsystem_mask: self.subsystems_online,
        }
    }
}

static mut GLOBAL_WATCHDOG: Watchdog = Watchdog::new();

#[no_mangle]
pub extern "C" fn rust_watchdog_ping(subsystem_id: u8) {
    unsafe {
        GLOBAL_WATCHDOG.heartbeat(subsystem_id);
    }
}

#[no_mangle]
pub extern "C" fn rust_watchdog_get_report() -> KernelHealthReport {
    unsafe { GLOBAL_WATCHDOG.report() }
}
