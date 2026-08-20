//! RFC-8439 Poly1305 One-Time Message Authenticator (no_std)
//! Computes 128-bit cryptographic MAC tags over prime field 2^130 - 5.

pub struct Poly1305 {
    r: [u32; 5],
    h: [u32; 5],
    pad: [u32; 4],
    leftover: usize,
    buffer: [u8; 16],
}

impl Poly1305 {
    pub fn new(key: &[u8; 32]) -> Self {
        // Clamp r
        let mut r = [0u32; 5];
        let t0 = u32::from_le_bytes([key[0], key[1], key[2], key[3]]);
        let t1 = u32::from_le_bytes([key[4], key[5], key[6], key[7]]);
        let t2 = u32::from_le_bytes([key[8], key[9], key[10], key[11]]);
        let t3 = u32::from_le_bytes([key[12], key[13], key[14], key[15]]);

        r[0] = t0 & 0x3ffffff;
        r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
        r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
        r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
        r[4] = (t3 >> 8) & 0x00fffff;

        let pad = [
            u32::from_le_bytes([key[16], key[17], key[18], key[19]]),
            u32::from_le_bytes([key[20], key[21], key[22], key[23]]),
            u32::from_le_bytes([key[24], key[25], key[26], key[27]]),
            u32::from_le_bytes([key[28], key[29], key[30], key[31]]),
        ];

        Poly1305 {
            r,
            h: [0; 5],
            pad,
            leftover: 0,
            buffer: [0; 16],
        }
    }

    fn blocks(&mut self, m: &[u8], is_final: bool) {
        let hibit = if is_final { 0u32 } else { 1u32 << 24 };
        let r0 = self.r[0] as u64;
        let r1 = self.r[1] as u64;
        let r2 = self.r[2] as u64;
        let r3 = self.r[3] as u64;
        let r4 = self.r[4] as u64;

        let s1 = r1 * 5;
        let s2 = r2 * 5;
        let s3 = r3 * 5;
        let s4 = r4 * 5;

        let mut offset = 0;
        while offset < m.len() {
            let t0 = u32::from_le_bytes([m[offset], m[offset + 1], m[offset + 2], m[offset + 3]]);
            let t1 = u32::from_le_bytes([m[offset + 4], m[offset + 5], m[offset + 6], m[offset + 7]]);
            let t2 = u32::from_le_bytes([m[offset + 8], m[offset + 9], m[offset + 10], m[offset + 11]]);
            let t3 = u32::from_le_bytes([m[offset + 12], m[offset + 13], m[offset + 14], m[offset + 15]]);

            let mut h0 = (self.h[0] as u64) + ((t0 & 0x3ffffff) as u64);
            let mut h1 = (self.h[1] as u64) + ((((t0 >> 26) | (t1 << 6)) & 0x3ffffff) as u64);
            let mut h2 = (self.h[2] as u64) + ((((t1 >> 20) | (t2 << 12)) & 0x3ffffff) as u64);
            let mut h3 = (self.h[3] as u64) + ((((t2 >> 14) | (t3 << 18)) & 0x3ffffff) as u64);
            let mut h4 = (self.h[4] as u64) + (((t3 >> 8) | hibit) as u64);

            let d0 = h0 * r0 + h1 * s4 + h2 * s3 + h3 * s2 + h4 * s1;
            let d1 = h0 * r1 + h1 * r0 + h2 * s4 + h3 * s3 + h4 * s2;
            let d2 = h0 * r2 + h1 * r1 + h2 * r0 + h3 * s4 + h4 * s3;
            let d3 = h0 * r3 + h1 * r2 + h2 * r1 + h3 * r0 + h4 * s4;
            let d4 = h0 * r4 + h1 * r3 + h2 * r2 + h3 * r1 + h4 * r0;

            let mut c = d0 >> 26;
            h0 = d0 & 0x3ffffff;
            let d1_c = d1 + c;
            c = d1_c >> 26;
            h1 = d1_c & 0x3ffffff;
            let d2_c = d2 + c;
            c = d2_c >> 26;
            h2 = d2_c & 0x3ffffff;
            let d3_c = d3 + c;
            c = d3_c >> 26;
            h3 = d3_c & 0x3ffffff;
            let d4_c = d4 + c;
            c = d4_c >> 26;
            h4 = d4_c & 0x3ffffff;

            h0 += c * 5;
            c = h0 >> 26;
            h0 &= 0x3ffffff;
            h1 += c;

            self.h[0] = h0 as u32;
            self.h[1] = h1 as u32;
            self.h[2] = h2 as u32;
            self.h[3] = h3 as u32;
            self.h[4] = h4 as u32;

            offset += 16;
        }
    }

    pub fn update(&mut self, data: &[u8]) {
        let mut offset = 0;
        let len = data.len();

        if self.leftover > 0 {
            let want = 16 - self.leftover;
            if len < want {
                self.buffer[self.leftover..self.leftover + len].copy_from_slice(data);
                self.leftover += len;
                return;
            }
            self.buffer[self.leftover..16].copy_from_slice(&data[..want]);
            let block = self.buffer;
            self.blocks(&block, false);
            offset += want;
            self.leftover = 0;
        }

        if len - offset >= 16 {
            let full_len = (len - offset) & !15;
            self.blocks(&data[offset..offset + full_len], false);
            offset += full_len;
        }

        if offset < len {
            self.leftover = len - offset;
            self.buffer[..self.leftover].copy_from_slice(&data[offset..]);
        }
    }

    pub fn finalize(mut self) -> [u8; 16] {
        if self.leftover > 0 {
            self.buffer[self.leftover] = 1;
            for i in self.leftover + 1..16 {
                self.buffer[i] = 0;
            }
            let block = self.buffer;
            self.blocks(&block, true);
        }

        let mut h0 = self.h[0];
        let mut h1 = self.h[1];
        let mut h2 = self.h[2];
        let mut h3 = self.h[3];
        let mut h4 = self.h[4];

        let mut c = h1 >> 26;
        h1 &= 0x3ffffff;
        h2 += c;
        c = h2 >> 26;
        h2 &= 0x3ffffff;
        h3 += c;
        c = h3 >> 26;
        h3 &= 0x3ffffff;
        h4 += c;
        c = h4 >> 26;
        h4 &= 0x3ffffff;
        h0 += c * 5;
        c = h0 >> 26;
        h0 &= 0x3ffffff;
        h1 += c;

        // Compute h + -p
        let g0 = h0.wrapping_add(5);
        c = g0 >> 26;
        let g1 = h1.wrapping_add(c);
        c = g1 >> 26;
        let g2 = h2.wrapping_add(c);
        c = g2 >> 26;
        let g3 = h3.wrapping_add(c);
        c = g3 >> 26;
        let g4 = h4.wrapping_add(c).wrapping_sub(1 << 26);

        let mask = ((g4 >> 31) as u32).wrapping_sub(1);
        let nmask = !mask;

        h0 = (h0 & nmask) | (g0 & mask & 0x3ffffff);
        h1 = (h1 & nmask) | (g1 & mask & 0x3ffffff);
        h2 = (h2 & nmask) | (g2 & mask & 0x3ffffff);
        h3 = (h3 & nmask) | (g3 & mask & 0x3ffffff);
        h4 = (h4 & nmask) | (g4 & mask & 0x3ffffff);

        // Convert to 32-bit words
        let f0 = (h0 | (h1 << 26)) as u64 + self.pad[0] as u64;
        let f1 = ((h1 >> 6) | (h2 << 20)) as u64 + self.pad[1] as u64 + (f0 >> 32);
        let f2 = ((h2 >> 12) | (h3 << 14)) as u64 + self.pad[2] as u64 + (f1 >> 32);
        let f3 = ((h3 >> 18) | (h4 << 8)) as u64 + self.pad[3] as u64 + (f2 >> 32);

        let mut out = [0u8; 16];
        out[0..4].copy_from_slice(&(f0 as u32).to_le_bytes());
        out[4..8].copy_from_slice(&(f1 as u32).to_le_bytes());
        out[8..12].copy_from_slice(&(f2 as u32).to_le_bytes());
        out[12..16].copy_from_slice(&(f3 as u32).to_le_bytes());
        out
    }
}
