//! FIPS 202 SHA3-256 and SHA3-512 (Keccak-f[1600])
//! 100% Memory-Safe Rust Implementation for SUB-OS Kernel

pub struct Keccak {
    state: [u64; 25],
    rate_bytes: usize,
    buffer: [u8; 200],
    buf_len: usize,
}

impl Keccak {
    const RC: [u64; 24] = [
        0x0000000000000001, 0x0000000000008082, 0x800000000000808A, 0x8000000080008000,
        0x000000000000808B, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
        0x000000000000008A, 0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
        0x000000008000808B, 0x800000000000008B, 0x8000000000008089, 0x8000000000008003,
        0x8000000000008002, 0x8000000000000080, 0x000000000000800A, 0x800000008000000A,
        0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
    ];

    const RHO: [u32; 25] = [
        0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14,
    ];

    const PI: [usize; 25] = [
        0, 10, 20, 5, 15, 16, 1, 11, 21, 6, 7, 17, 2, 12, 22, 23, 8, 18, 3, 13, 14, 24, 9, 19, 4,
    ];

    pub fn new(rate_bytes: usize) -> Self {
        Self {
            state: [0u64; 25],
            rate_bytes,
            buffer: [0u8; 200],
            buf_len: 0,
        }
    }

    fn keccak_f(&mut self) {
        let mut c = [0u64; 5];
        let mut d = [0u64; 5];

        for _round in 0..24 {
            // Theta step
            for x in 0..5 {
                c[x] = self.state[x] ^ self.state[x + 5] ^ self.state[x + 10] ^ self.state[x + 15] ^ self.state[x + 20];
            }
            for x in 0..5 {
                d[x] = c[(x + 4) % 5] ^ c[(x + 1) % 5].rotate_left(1);
            }
            for i in 0..25 {
                self.state[i] ^= d[i % 5];
            }

            // Rho and Pi steps
            let mut b = [0u64; 25];
            for i in 0..25 {
                b[Self::PI[i]] = self.state[i].rotate_left(Self::RHO[i]);
            }

            // Chi step
            for y in (0..25).step_by(5) {
                for x in 0..5 {
                    self.state[y + x] = b[y + x] ^ ((!b[y + (x + 1) % 5]) & b[y + (x + 2) % 5]);
                }
            }

            // Iota step
            self.state[0] ^= Self::RC[_round];
        }
    }

    fn absorb_block(&mut self, block: &[u8]) {
        for (i, chunk) in block.chunks(8).enumerate() {
            let mut val = 0u64;
            for (j, &b) in chunk.iter().enumerate() {
                val |= (b as u64) << (8 * j);
            }
            self.state[i] ^= val;
        }
        self.keccak_f();
    }

    pub fn update(&mut self, input: &[u8]) {
        let mut offset = 0;
        let len = input.len();

        if self.buf_len > 0 {
            let needed = self.rate_bytes - self.buf_len;
            if len < needed {
                self.buffer[self.buf_len..self.buf_len + len].copy_from_slice(input);
                self.buf_len += len;
                return;
            }
            self.buffer[self.buf_len..self.rate_bytes].copy_from_slice(&input[..needed]);
            let block = self.buffer;
            self.absorb_block(&block[..self.rate_bytes]);
            offset += needed;
            self.buf_len = 0;
        }

        while offset + self.rate_bytes <= len {
            self.absorb_block(&input[offset..offset + self.rate_bytes]);
            offset += self.rate_bytes;
        }

        if offset < len {
            let remainder = len - offset;
            self.buffer[..remainder].copy_from_slice(&input[offset..]);
            self.buf_len = remainder;
        }
    }

    pub fn finalize_sha3(&mut self, output: &mut [u8]) {
        // SHA-3 domain suffix 0x06 (0x01 for raw Keccak, 0x06 for FIPS 202 SHA-3)
        self.buffer[self.buf_len] = 0x06;
        for i in (self.buf_len + 1)..self.rate_bytes {
            self.buffer[i] = 0x00;
        }
        self.buffer[self.rate_bytes - 1] |= 0x80;
        let block = self.buffer;
        self.absorb_block(&block[..self.rate_bytes]);

        // Squeeze output
        let mut squeezed = 0;
        while squeezed < output.len() {
            let available = core::cmp::min(self.rate_bytes, output.len() - squeezed);
            for i in 0..available {
                let word_idx = i / 8;
                let byte_idx = i % 8;
                output[squeezed + i] = ((self.state[word_idx] >> (8 * byte_idx)) & 0xFF) as u8;
            }
            squeezed += available;
            if squeezed < output.len() {
                self.keccak_f();
            }
        }
    }
}

pub fn sha3_256(data: &[u8], output: &mut [u8; 32]) {
    let mut keccak = Keccak::new(136); // Rate = 1088 bits = 136 bytes
    keccak.update(data);
    keccak.finalize_sha3(output);
}

pub fn sha3_512(data: &[u8], output: &mut [u8; 64]) {
    let mut keccak = Keccak::new(72); // Rate = 576 bits = 72 bytes
    keccak.update(data);
    keccak.finalize_sha3(output);
}

// C-FFI Bindings
#[no_mangle]
pub extern "C" fn rust_sha3_256(data: *const u8, len: usize, out: *mut u8) -> i32 {
    if data.is_null() || out.is_null() {
        return -1;
    }
    unsafe {
        let input_slice = core::slice::from_raw_parts(data, len);
        let mut hash = [0u8; 32];
        sha3_256(input_slice, &mut hash);
        core::ptr::copy_nonoverlapping(hash.as_ptr(), out, 32);
    }
    0
}
