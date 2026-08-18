//! RFC 4648 Base64 Encoding and Decoding Engine
//! 100% Memory-Safe Rust Implementation for SUB-OS

const BASE64_ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

pub fn base64_encode(input: &[u8], output: &mut [u8]) -> usize {
    let mut out_idx = 0;
    let len = input.len();
    let mut i = 0;

    while i < len {
        let b0 = input[i] as usize;
        let b1 = if i + 1 < len { input[i + 1] as usize } else { 0 };
        let b2 = if i + 2 < len { input[i + 2] as usize } else { 0 };

        let triple = (b0 << 16) | (b1 << 8) | b2;

        if out_idx + 4 > output.len() {
            break;
        }

        output[out_idx] = BASE64_ALPHABET[(triple >> 18) & 0x3F];
        output[out_idx + 1] = BASE64_ALPHABET[(triple >> 12) & 0x3F];
        output[out_idx + 2] = if i + 1 < len { BASE64_ALPHABET[(triple >> 6) & 0x3F] } else { b'=' };
        output[out_idx + 3] = if i + 2 < len { BASE64_ALPHABET[triple & 0x3F] } else { b'=' };

        out_idx += 4;
        i += 3;
    }

    out_idx
}

fn decode_char(c: u8) -> Option<u8> {
    match c {
        b'A'..=b'Z' => Some(c - b'A'),
        b'a'..=b'z' => Some(c - b'a' + 26),
        b'0'..=b'9' => Some(c - b'0' + 52),
        b'+' => Some(62),
        b'/' => Some(63),
        _ => None,
    }
}

pub fn base64_decode(input: &[u8], output: &mut [u8]) -> usize {
    let mut out_idx = 0;
    let mut chunk = [0u8; 4];
    let mut chunk_len = 0;

    for &b in input {
        if b == b'=' || decode_char(b).is_some() {
            chunk[chunk_len] = b;
            chunk_len += 1;

            if chunk_len == 4 {
                let c0 = decode_char(chunk[0]).unwrap_or(0) as usize;
                let c1 = decode_char(chunk[1]).unwrap_or(0) as usize;
                let c2 = if chunk[2] == b'=' { 0 } else { decode_char(chunk[2]).unwrap_or(0) as usize };
                let c3 = if chunk[3] == b'=' { 0 } else { decode_char(chunk[3]).unwrap_or(0) as usize };

                let triple = (c0 << 18) | (c1 << 12) | (c2 << 6) | c3;

                if out_idx < output.len() {
                    output[out_idx] = ((triple >> 16) & 0xFF) as u8;
                    out_idx += 1;
                }
                if chunk[2] != b'=' && out_idx < output.len() {
                    output[out_idx] = ((triple >> 8) & 0xFF) as u8;
                    out_idx += 1;
                }
                if chunk[3] != b'=' && out_idx < output.len() {
                    output[out_idx] = (triple & 0xFF) as u8;
                    out_idx += 1;
                }

                chunk_len = 0;
            }
        }
    }

    out_idx
}

// C-FFI Export
#[no_mangle]
pub extern "C" fn rust_base64_encode(
    in_buf: *const u8,
    in_len: usize,
    out_buf: *mut u8,
    max_out: usize,
) -> i32 {
    if in_buf.is_null() || out_buf.is_null() || max_out == 0 {
        return -1;
    }
    unsafe {
        let input = core::slice::from_raw_parts(in_buf, in_len);
        let output = core::slice::from_raw_parts_mut(out_buf, max_out);
        let written = base64_encode(input, output);
        if written < max_out {
            *out_buf.add(written) = 0; // Null terminator
        }
        written as i32
    }
}

#[no_mangle]
pub extern "C" fn rust_base64_decode(
    in_buf: *const u8,
    in_len: usize,
    out_buf: *mut u8,
    max_out: usize,
) -> i32 {
    if in_buf.is_null() || out_buf.is_null() || max_out == 0 {
        return -1;
    }
    unsafe {
        let input = core::slice::from_raw_parts(in_buf, in_len);
        let output = core::slice::from_raw_parts_mut(out_buf, max_out);
        let written = base64_decode(input, output);
        if written < max_out {
            *out_buf.add(written) = 0;
        }
        written as i32
    }
}
