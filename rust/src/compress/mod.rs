//! Memory-safe run-length encoding (RLE) codec for SUB-OS.
//!
//! A compact PackBits-style byte codec: the stream is a sequence of
//! (count, value) pairs where `count` is 1..=255. It is intended for run-heavy
//! data (framebuffer scanlines, zeroed pages, simple bitmaps) and is fully
//! `no_std` with no allocation -- the caller owns both buffers.

/// Compress `input` into `output` as (count, value) pairs. Returns the number
/// of bytes written, or -1 if `output` is too small.
pub fn rle_compress(input: &[u8], output: &mut [u8]) -> isize {
    let mut out = 0usize;
    let mut i = 0usize;
    let len = input.len();

    while i < len {
        let value = input[i];
        let mut run = 1usize;
        while i + run < len && input[i + run] == value && run < 255 {
            run += 1;
        }
        if out + 2 > output.len() {
            return -1;
        }
        output[out] = run as u8;
        output[out + 1] = value;
        out += 2;
        i += run;
    }
    out as isize
}

/// Expand a (count, value) stream back into `output`. Returns the number of
/// bytes written, or -1 on a malformed stream or an output overflow.
pub fn rle_decompress(input: &[u8], output: &mut [u8]) -> isize {
    let mut out = 0usize;
    let mut i = 0usize;
    let len = input.len();

    while i + 1 < len {
        let run = input[i] as usize;
        let value = input[i + 1];
        if run == 0 {
            return -1; // a zero run is never emitted by the compressor
        }
        if out + run > output.len() {
            return -1;
        }
        for _ in 0..run {
            output[out] = value;
            out += 1;
        }
        i += 2;
    }
    out as isize
}

// ---------------------------------------------------------------------------
// C ABI shims
// ---------------------------------------------------------------------------

#[no_mangle]
pub extern "C" fn rust_rle_compress(
    in_buf: *const u8,
    in_len: usize,
    out_buf: *mut u8,
    max_out: usize,
) -> i32 {
    if in_buf.is_null() || out_buf.is_null() {
        return -1;
    }
    unsafe {
        let input = core::slice::from_raw_parts(in_buf, in_len);
        let output = core::slice::from_raw_parts_mut(out_buf, max_out);
        rle_compress(input, output) as i32
    }
}

#[no_mangle]
pub extern "C" fn rust_rle_decompress(
    in_buf: *const u8,
    in_len: usize,
    out_buf: *mut u8,
    max_out: usize,
) -> i32 {
    if in_buf.is_null() || out_buf.is_null() {
        return -1;
    }
    unsafe {
        let input = core::slice::from_raw_parts(in_buf, in_len);
        let output = core::slice::from_raw_parts_mut(out_buf, max_out);
        rle_decompress(input, output) as i32
    }
}
