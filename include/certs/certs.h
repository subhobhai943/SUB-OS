#ifndef _CERTS_CERTS_H
#define _CERTS_CERTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_CERT_SUBJECT 128
#define MAX_CERT_ISSUER  128
#define MAX_CERTS_STORE  16

typedef enum {
    KEY_ALGO_RSA = 1,
    KEY_ALGO_ECDSA,
    KEY_ALGO_ED25519
} cert_key_algo_t;

typedef struct {
    char subject[MAX_CERT_SUBJECT];
    char issuer[MAX_CERT_ISSUER];
    uint32_t valid_from;
    uint32_t valid_to;
    cert_key_algo_t algo;
    uint8_t public_key[256];
    size_t key_len;
    uint8_t fingerprint[32]; // SHA-256 fingerprint
    bool trusted;
} x509_cert_t;

typedef struct {
    x509_cert_t certs[MAX_CERTS_STORE];
    size_t count;
} kernel_keyring_t;

void certs_init(void);
int certs_add_trusted(const char* subject, const char* issuer, cert_key_algo_t algo, const uint8_t* pubkey, size_t key_len);
bool certs_verify_signature(const uint8_t* data, size_t data_len, const uint8_t* sig, size_t sig_len, const x509_cert_t* cert);
x509_cert_t* certs_find_by_subject(const char* subject);
size_t certs_get_count(void);
const x509_cert_t* certs_get_cert(size_t index);

#endif // _CERTS_CERTS_H
