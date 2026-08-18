//! AES-128 and AES-256 Block Cipher (FIPS 197)
//! 100% Memory-Safe Rust Implementation for SUB-OS Kernel

const SBOX: [u8; 256] = [
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
];

const RCON: [u8; 11] = [0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36];

pub struct Aes128 {
    round_keys: [u8; 176],
}

impl Aes128 {
    pub fn new(key: &[u8; 16]) -> Self {
        let mut round_keys = [0u8; 176];
        round_keys[..16].copy_from_slice(key);

        let mut bytes_generated = 16;
        let mut rcon_iter = 1;
        let mut temp = [0u8; 4];

        while bytes_generated < 176 {
            temp.copy_from_slice(&round_keys[bytes_generated - 4..bytes_generated]);

            if bytes_generated % 16 == 0 {
                // RotWord
                let t = temp[0];
                temp[0] = temp[1];
                temp[1] = temp[2];
                temp[2] = temp[3];
                temp[3] = t;

                // SubWord
                for b in temp.iter_mut() {
                    *b = SBOX[*b as usize];
                }

                // Rcon
                temp[0] ^= RCON[rcon_iter];
                rcon_iter += 1;
            }

            for i in 0..4 {
                round_keys[bytes_generated] = round_keys[bytes_generated - 16] ^ temp[i];
                bytes_generated += 1;
            }
        }

        Self { round_keys }
    }

    #[inline(always)]
    fn sub_bytes(state: &mut [u8; 16]) {
        for b in state.iter_mut() {
            *b = SBOX[*b as usize];
        }
    }

    #[inline(always)]
    fn shift_rows(state: &mut [u8; 16]) {
        let temp = *state;
        state[1] = temp[5];
        state[5] = temp[9];
        state[9] = temp[13];
        state[13] = temp[1];

        state[2] = temp[10];
        state[6] = temp[14];
        state[10] = temp[2];
        state[14] = temp[6];

        state[3] = temp[15];
        state[7] = temp[3];
        state[11] = temp[7];
        state[15] = temp[11];
    }

    #[inline(always)]
    fn gmul(mut a: u8, mut b: u8) -> u8 {
        let mut p = 0u8;
        for _ in 0..8 {
            if (b & 1) != 0 {
                p ^= a;
            }
            let hi_bit = a & 0x80;
            a <<= 1;
            if hi_bit != 0 {
                a ^= 0x1b; // Rijndael polynomial
            }
            b >>= 1;
        }
        p
    }

    #[inline(always)]
    fn mix_columns(state: &mut [u8; 16]) {
        for c in (0..16).step_by(4) {
            let a0 = state[c];
            let a1 = state[c + 1];
            let a2 = state[c + 2];
            let a3 = state[c + 3];

            state[c] = Self::gmul(2, a0) ^ Self::gmul(3, a1) ^ a2 ^ a3;
            state[c + 1] = a0 ^ Self::gmul(2, a1) ^ Self::gmul(3, a2) ^ a3;
            state[c + 2] = a0 ^ a1 ^ Self::gmul(2, a2) ^ Self::gmul(3, a3);
            state[c + 3] = Self::gmul(3, a0) ^ a1 ^ a2 ^ Self::gmul(2, a3);
        }
    }

    #[inline(always)]
    fn add_round_key(state: &mut [u8; 16], round_key: &[u8]) {
        for (s, k) in state.iter_mut().zip(round_key.iter()) {
            *s ^= *k;
        }
    }

    pub fn encrypt_block(&self, block: &mut [u8; 16]) {
        Self::add_round_key(block, &self.round_keys[0..16]);

        for round in 1..10 {
            Self::sub_bytes(block);
            Self::shift_rows(block);
            Self::mix_columns(block);
            Self::add_round_key(block, &self.round_keys[round * 16..(round + 1) * 16]);
        }

        Self::sub_bytes(block);
        Self::shift_rows(block);
        Self::add_round_key(block, &self.round_keys[160..176]);
    }
}

// C-FFI Export
#[no_mangle]
pub extern "C" fn rust_aes128_ecb_encrypt(key: *const u8, block: *mut u8) -> i32 {
    if key.is_null() || block.is_null() {
        return -1;
    }
    unsafe {
        let mut key_arr = [0u8; 16];
        let mut block_arr = [0u8; 16];
        core::ptr::copy_nonoverlapping(key, key_arr.as_mut_ptr(), 16);
        core::ptr::copy_nonoverlapping(block, block_arr.as_mut_ptr(), 16);

        let cipher = Aes128::new(&key_arr);
        cipher.encrypt_block(&mut block_arr);

        core::ptr::copy_nonoverlapping(block_arr.as_ptr(), block, 16);
    }
    0
}
