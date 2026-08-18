//! Intelligent Sensor & Predictive Fan Thermal Governor
//! Memory-safe Rust module for SUB-OS HWMON subsystem

#[repr(C)]
pub struct SensorThermalReport {
    pub target_fan_rpm: u32,
    pub throttle_level: u8,
    pub is_critical: bool,
}

#[no_mangle]
pub extern "C" fn rust_sensor_calc_fan_curve(temp_celsius: i32) -> SensorThermalReport {
    let (fan_rpm, throttle, critical) = match temp_celsius {
        t if t <= 35 => (1200, 0, false), // Silent mode
        t if t <= 50 => (1800, 0, false), // Normal operating mode
        t if t <= 65 => (2400, 0, false), // Performance curve
        t if t <= 80 => (3200, 1, false), // Aggressive cooling + Stage 1 Throttle
        t if t <= 95 => (4200, 2, false), // Emergency cooling + Stage 2 Throttle
        _ => (5000, 3, true),             // Critical overtemperature threshold
    };

    SensorThermalReport {
        target_fan_rpm: fan_rpm,
        throttle_level: throttle,
        is_critical: critical,
    }
}
