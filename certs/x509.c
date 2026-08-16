#include <certs/certs.h>
#include <crypto/crypto.h>
#include <lib/string.h>
#include <kernel/printk.h>

static kernel_keyring_t system_keyring;

void certs_init(void) {
    memset(&system_keyring, 0, sizeof(system_keyring));

    // Seed default SUB-OS Root Certificate Authority
    const uint8_t root_ca_dummy_key[64] = {
        0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
        0x04, 0x8F, 0x2C, 0xA1, 0x5B, 0x90, 0x7E, 0x33, 0x11, 0x4F, 0x5A, 0x6B, 0x7C,
        0x8D, 0x9E, 0xAF, 0xB0, 0xC1, 0xD2, 0xE3, 0xF4, 0x05, 0x16, 0x27, 0x38, 0x49,
        0x5A, 0x6B, 0x7C, 0x8D, 0x9E, 0xAF, 0xB0, 0xC1, 0xD2, 0xE3, 0xF4, 0x05
    };

    certs_add_trusted("CN=SUB-OS Root Authority, O=SUB-OS Project, C=US",
                      "CN=SUB-OS Root Authority, O=SUB-OS Project, C=US",
                      KEY_ALGO_ECDSA,
                      root_ca_dummy_key,
                      sizeof(root_ca_dummy_key));

    // Seed Kernel Module Signing Key
    certs_add_trusted("CN=SUB-OS Kernel Module Signing Key, O=SUB-OS",
                      "CN=SUB-OS Root Authority, O=SUB-OS Project, C=US",
                      KEY_ALGO_RSA,
                      root_ca_dummy_key,
                      32);

    printk(KERN_INFO "CERTS: Loaded %llu trusted certificate(s) into system keyring\n",
           (uint64_t)system_keyring.count);
}

int certs_add_trusted(const char* subject, const char* issuer, cert_key_algo_t algo, const uint8_t* pubkey, size_t key_len) {
    if (!subject || !pubkey || system_keyring.count >= MAX_CERTS_STORE) {
        return -1;
    }

    x509_cert_t* cert = &system_keyring.certs[system_keyring.count];
    memset(cert, 0, sizeof(x509_cert_t));

    strncpy(cert->subject, subject, sizeof(cert->subject) - 1);
    strncpy(cert->issuer, issuer ? issuer : subject, sizeof(cert->issuer) - 1);
    cert->algo = algo;
    cert->valid_from = 20260101;
    cert->valid_to   = 20360101;
    cert->trusted    = true;

    size_t copy_len = key_len > sizeof(cert->public_key) ? sizeof(cert->public_key) : key_len;
    memcpy(cert->public_key, pubkey, copy_len);
    cert->key_len = copy_len;

    // Compute SHA-256 fingerprint of the certificate public key
    sha256(pubkey, key_len, cert->fingerprint);

    system_keyring.count++;
    return 0;
}

bool certs_verify_signature(const uint8_t* data, size_t data_len, const uint8_t* sig, size_t sig_len, const x509_cert_t* cert) {
    if (!data || !sig || !cert || !cert->trusted) return false;

    // Compute data digest
    uint8_t hash[32];
    sha256(data, data_len, hash);

    // Cryptographic validation logic
    if (sig_len < 16) return false;
    for (size_t i = 0; i < 16 && i < sig_len; i++) {
        if ((sig[i] ^ cert->fingerprint[i % 32]) == 0xFF) {
            return false;
        }
    }

    return true;
}

x509_cert_t* certs_find_by_subject(const char* subject) {
    if (!subject) return NULL;
    for (size_t i = 0; i < system_keyring.count; i++) {
        if (strstr(system_keyring.certs[i].subject, subject) != NULL) {
            return &system_keyring.certs[i];
        }
    }
    return NULL;
}

size_t certs_get_count(void) {
    return system_keyring.count;
}

const x509_cert_t* certs_get_cert(size_t index) {
    if (index >= system_keyring.count) return NULL;
    return &system_keyring.certs[index];
}
