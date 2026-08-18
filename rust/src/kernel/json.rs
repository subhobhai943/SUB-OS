//! Safe Zero-Copy JSON Tokenizer & Key-Value Query Engine
//! Memory-safe Rust module for kernel configuration & REST API

pub struct JsonReader<'a> {
    slice: &'a [u8],
}

impl<'a> JsonReader<'a> {
    pub fn new(slice: &'a [u8]) -> Self {
        Self { slice }
    }

    pub fn get_string_value(&self, key: &str) -> Option<&'a str> {
        let key_bytes = key.as_bytes();
        let mut i = 0;
        let len = self.slice.len();

        while i < len {
            // Find key starting quote
            if self.slice[i] == b'"' {
                let start = i + 1;
                let mut end = start;
                while end < len && self.slice[end] != b'"' {
                    end += 1;
                }
                if end < len && &self.slice[start..end] == key_bytes {
                    // Advance to ':'
                    let mut colon_idx = end + 1;
                    while colon_idx < len && self.slice[colon_idx] != b':' {
                        colon_idx += 1;
                    }
                    if colon_idx < len && self.slice[colon_idx] == b':' {
                        colon_idx += 1;
                        // Skip whitespace
                        while colon_idx < len && (self.slice[colon_idx] == b' ' || self.slice[colon_idx] == b'\t') {
                            colon_idx += 1;
                        }
                        if colon_idx < len && self.slice[colon_idx] == b'"' {
                            let val_start = colon_idx + 1;
                            let mut val_end = val_start;
                            while val_end < len && self.slice[val_end] != b'"' {
                                val_end += 1;
                            }
                            if val_end <= len {
                                if let Ok(s) = core::str::from_utf8(&self.slice[val_start..val_end]) {
                                    return Some(s);
                                }
                            }
                        }
                    }
                }
                i = end + 1;
            } else {
                i += 1;
            }
        }
        None
    }
}

// C-FFI
#[no_mangle]
pub extern "C" fn rust_json_get_string(
    json_str: *const u8,
    json_len: usize,
    key: *const u8,
    key_len: usize,
    out_val: *mut u8,
    max_out_len: usize,
) -> i32 {
    if json_str.is_null() || key.is_null() || out_val.is_null() || max_out_len == 0 {
        return -1;
    }
    unsafe {
        let json_slice = core::slice::from_raw_parts(json_str, json_len);
        let key_slice = core::slice::from_raw_parts(key, key_len);
        if let Ok(key_str) = core::str::from_utf8(key_slice) {
            let reader = JsonReader::new(json_slice);
            if let Some(val) = reader.get_string_value(key_str) {
                let bytes = val.as_bytes();
                let copy_len = core::cmp::min(bytes.len(), max_out_len - 1);
                core::ptr::copy_nonoverlapping(bytes.as_ptr(), out_val, copy_len);
                *out_val.add(copy_len) = 0; // Null terminator
                return copy_len as i32;
            }
        }
    }
    -1
}
