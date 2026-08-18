//! PCI Configuration Space & Device Info Parser
//! 100% Memory-Safe Rust Driver Subsystem for SUB-OS

#[repr(C)]
pub struct PciDeviceInfo {
    pub bus: u8,
    pub slot: u8,
    pub func: u8,
    pub vendor_id: u16,
    pub device_id: u16,
    pub class_id: u8,
    pub subclass_id: u8,
}

pub struct PciDatabase;

impl PciDatabase {
    pub fn get_vendor_name(vendor_id: u16) -> &'static str {
        match vendor_id {
            0x8086 => "Intel Corporation",
            0x10EC => "Realtek Semiconductor Co.",
            0x1AF4 => "Red Hat / VirtIO Device",
            0x1234 => "Bochs / QEMU Standard VGA",
            0x10DE => "NVIDIA Corporation",
            0x1002 => "Advanced Micro Devices [AMD]",
            0x15AD => "VMware SVGA / VMXNET",
            _ => "Unknown Vendor",
        }
    }

    pub fn get_class_name(class_id: u8, subclass_id: u8) -> &'static str {
        match (class_id, subclass_id) {
            (0x01, 0x01) => "IDE Storage Controller",
            (0x01, 0x06) => "SATA AHCI Controller",
            (0x01, 0x08) => "NVMe Storage Controller",
            (0x02, 0x00) => "Ethernet Network Controller",
            (0x03, 0x00) => "VGA Compatible Controller",
            (0x04, 0x03) => "High Definition Audio (HDA)",
            (0x06, 0x00) => "Host Bridge",
            (0x06, 0x01) => "ISA Bridge",
            (0x0C, 0x03) => "USB Extensible Host Controller (xHCI)",
            _ => "Other PCI Device",
        }
    }
}

// C-FFI
#[no_mangle]
pub extern "C" fn rust_pci_get_vendor_name(vendor_id: u16) -> *const u8 {
    let name = PciDatabase::get_vendor_name(vendor_id);
    name.as_ptr()
}

#[no_mangle]
pub extern "C" fn rust_pci_get_class_name(class_id: u8, subclass_id: u8) -> *const u8 {
    let name = PciDatabase::get_class_name(class_id, subclass_id);
    name.as_ptr()
}
