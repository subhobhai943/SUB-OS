//! RFC-8439 ChaCha20-Poly1305 AEAD (Authenticated Encryption with Associated Data)
//! Pure no_std implementation providing confidential, authenticated encryption.

use crate::drivers::chacha20::ChaCha20;
use crate::crypto::poly1305::Poly1305;

pub struct ChaCha20Poly1305;

impl ChaCha20Poly1305 {
    fn generate_poly1305_key(key: &[u8; 32], nonce: &[u8; 12]) -> [u8; 32] {
        let mut cipher = ChaCha20::new(key, nonce, 0);
        let mut block = [0u8; 64];
        cipher.apply_keystream(&mut block);
        let mut poly_key = [0u8; 32];
        poly_key.copy_from_slice(&block[0..32]);
        poly_key
    }

    fn pad16(mac: &mut Poly1305, len: usize) {
        let rem = len % 16;
        if rem != 0 {
            let pad = [0u8; 16];
            mac.update(&pad[..16 - rem]);
        }
    }

    pub fn encrypt(
        key: &[u8; 32],
        nonce: &[u8; 12],
        aad: &[u8],
        plaintext: &[u8],
        ciphertext_out: &mut [u8],
    ) -> Result<[u8; 16], &'static str> {
        if ciphertext_out.len() < plaintext.len() {
            return Err("Output buffer too small for ciphertext");
        }

        // 1. Generate Poly1305 one-time key
        let poly_key = Self::generate_poly1305_key(key, nonce);

        // 2. Encrypt plaintext with ChaCha20 starting at counter 1
        ciphertext_out[..plaintext.len()].copy_from_slice(plaintext);
        let mut cipher = ChaCha20::new(key, nonce, 1);
        cipher.apply_keystream(&mut ciphertext_out[..plaintext.len()]);

        // 3. Compute Poly1305 MAC tag
        let mut mac = Poly1305::new(&poly_key);
        mac.update(aad);
        Self::pad16(&mut mac, aad.len());

        mac.update(&ciphertext_out[..plaintext.len()]);
        Self::pad16(&mut mac, plaintext.len());

        let aad_len_bytes = (aad.len() as u64).to_le_bytes();
        let ct_len_bytes = (plaintext.len() as u64).to_le_bytes();
        mac.update(&aad_len_bytes);
        mac.update(&ct_len_bytes);

        Ok(mac.finalize())
    }

    pub fn decrypt(
        key: &[u8; 32],
        nonce: &[u8; 12],
        aad: &[u8],
        ciphertext: &[u8],
        tag: &[u8; 16],
        plaintext_out: &mut [u8],
    ) -> Result<(), &'static str> {
        if plaintext_out.len() < ciphertext.len() {
            return Err("Output buffer too small for plaintext");
        }

        // 1. Generate Poly1305 one-time key
        let poly_key = Self::generate_poly1305_key(key, nonce);

        // 2. Verify MAC tag
        let mut mac = Poly1305::new(&poly_key);
        mac.update(aad);
        Self::pad16(&mut mac, aad.len());

        mac.update(ciphertext);
        Self::pad16(&mut mac, ciphertext.len());

        let aad_len_bytes = (aad.len() as u64).to_le_bytes();
        let ct_len_bytes = (ciphertext.len() as u64).to_le_bytes();
        mac.update(&aad_len_bytes);
        mac.update(&ct_len_bytes);

        let calculated_tag = mac.finalize();

        // Constant-time tag comparison
        let mut diff = 0u8;
        for i in 0..16 {
            diff |= calculated_tag[i] ^ tag[i];
        }

        if diff != 0 {
            return Err("Authentication tag verification failed");
        }

        // 3. Decrypt ciphertext with ChaCha20 starting at counter 1
        plaintext_out[..ciphertext.len()].copy_from_slice(ciphertext);
        let mut cipher = ChaCha20::new(key, nonce, 1);
        cipher.apply_keystream(&mut plaintext_out[..ciphertext.len()]);

        Ok(())
    }
}

// -----------------------------------------------------------------------------
// C-FFI Bridge Exports
// -----------------------------------------------------------------------------
#[no_mangle]
pub extern "C" fn rust_aead_encrypt(
    key: *const u8,
    nonce: *const u8,
    aad: *const u8,
    aad_len: usize,
    plain: *const u8,
    plain_len: usize,
    cipher_out: *mut u8,
    tag_out: *mut u8,
) -> i32 {
    if key.is_null() || nonce.is_null() || (plain_len > 0 && (plain.is_null() || cipher_out.is_null())) || tag_out.is_null() {
        return -1;
    }

    let key_arr: &[u8; 32] = unsafe { &*(key as *const [u8; 32]) };
    let nonce_arr: &[u8; 12] = unsafe { &*(nonce as *const [u8; 12]) };
    let aad_slice = if aad.is_null() || aad_len == 0 { &[] } else { unsafe { core::slice::from_raw_parts(aad, aad_len) } };
    let plain_slice = if plain_len == 0 { &[] } else { unsafe { core::slice::from_raw_parts(plain, plain_len) } };
    let out_slice = if plain_len == 0 { &mut [] } else { unsafe { core::slice::from_raw_parts_mut(cipher_out, plain_len) } };

    match ChaCha20Poly1305::encrypt(key_arr, nonce_arr, aad_slice, plain_slice, out_slice) {
        Ok(tag) => {
            unsafe {
                core::ptr::copy_nonoverlapping(tag.as_ptr(), tag_out, 16);
            }
            0
        }
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn rust_aead_decrypt(
    key: *const u8,
    nonce: *const u8,
    aad: *const u8,
    aad_len: usize,
    cipher: *const u8,
    cipher_len: usize,
    tag: *const u8,
    plain_out: *mut u8,
) -> i32 {
    if key.is_null() || nonce.is_null() || (cipher_len > 0 && (cipher.is_null() || plain_out.is_null())) || tag.is_null() {
        return -1;
    }

    let key_arr: &[u8; 32] = unsafe { &*(key as *const [u8; 32]) };
    let nonce_arr: &[u8; 12] = unsafe { &*(nonce as *const [u8; 12]) };
    let tag_arr: &[u8; 16] = unsafe { &*(tag as *const [u8; 16]) };
    let aad_slice = if aad.is_null() || aad_len == 0 { &[] } else { unsafe { core::slice::from_raw_parts(aad, aad_len) } };
    let cipher_slice = if cipher_len == 0 { &[] } else { unsafe { core::slice::from_raw_parts(cipher, cipher_len) } };
    let out_slice = if cipher_len == 0 { &mut [] } else { unsafe { core::slice::from_raw_parts_mut(plain_out, cipher_len) } };

    match ChaCha20Poly1305::decrypt(key_arr, nonce_arr, aad_slice, cipher_slice, tag_arr, out_slice) {
        Ok(()) => 0,
        Err(_) => -1,
    }
}
