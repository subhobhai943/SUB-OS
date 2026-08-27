/*
 * TLS 1.3 client for SUB-OS (RFC 8446), single suite:
 * TLS_CHACHA20_POLY1305_SHA256 over X25519. See net/tls.h for the scope and
 * the honest security caveat (the server certificate is not verified).
 *
 * Structure:
 *   - a small record layer (read/write, plaintext then AEAD-protected),
 *   - the handshake: ClientHello -> ServerHello -> key schedule -> decrypt the
 *     server flight -> send Finished -> switch to application keys,
 *   - the TLS 1.3 key schedule driven by crypto/hkdf.c over a running
 *     transcript hash.
 *
 * All large buffers live in the heap-allocated connection object, never on the
 * stack, because this runs on the 16 KB worker-thread stack.
 */
#include <net/tls.h>
#include <net/tcp.h>
#include <net/net.h>
#include <crypto/crypto.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define TLS_PLAINTEXT_MAX 16384
#define TLS_REC_MAX       (TLS_PLAINTEXT_MAX + 256)

// Content types.
#define CT_CHANGE_CIPHER_SPEC 20
#define CT_ALERT              21
#define CT_HANDSHAKE          22
#define CT_APPLICATION_DATA   23

// Handshake message types.
#define HS_CLIENT_HELLO       1
#define HS_SERVER_HELLO       2
#define HS_NEW_SESSION_TICKET 4
#define HS_ENCRYPTED_EXT      8
#define HS_CERTIFICATE        11
#define HS_CERT_REQUEST       13
#define HS_CERT_VERIFY        15
#define HS_FINISHED           20

#define TLS_AEAD_TAG 16

struct tls_conn {
    tcp_conn_t* tcp;

    sha256_ctx_t transcript;

    uint8_t priv[32];              // our X25519 private scalar
    uint8_t handshake_secret[32];
    uint8_t c_hs_secret[32];       // client handshake traffic secret

    uint8_t c_hs_key[32], c_hs_iv[12];
    uint8_t s_hs_key[32], s_hs_iv[12];
    uint8_t c_ap_key[32], c_ap_iv[12];
    uint8_t s_ap_key[32], s_ap_iv[12];

    uint64_t send_seq;
    uint64_t recv_seq;
    bool     app_keys;             // switched from handshake to application keys
    bool     established;

    // Decrypted application-data left over from a record, awaiting the reader.
    uint8_t  rbuf[TLS_PLAINTEXT_MAX];
    int      rbuf_len, rbuf_off;

    // Decrypted handshake bytes of the current record, consumed by hs_getbytes.
    uint8_t  hs_cur[TLS_PLAINTEXT_MAX];
    int      hs_cur_len, hs_cur_off;

    // Scratch for record assembly / reads (kept off the stack).
    uint8_t  rec[TLS_REC_MAX];
    uint8_t  pt[TLS_PLAINTEXT_MAX + 1];
};

static char g_tls_err[96] = "";
static void tls_err(const char* m) {
    strncpy(g_tls_err, m, sizeof(g_tls_err) - 1);
    g_tls_err[sizeof(g_tls_err) - 1] = '\0';
}
const char* tls_last_error(void) { return g_tls_err; }

// ===========================================================================
// TCP framing helpers
// ===========================================================================
static bool read_exact(tcp_conn_t* c, uint8_t* buf, int n, uint32_t timeout_ms) {
    int got = 0;
    while (got < n) {
        int r = tcp_recv(c, buf + got, (uint16_t)(n - got), timeout_ms);
        if (r <= 0) return false;
        got += r;
    }
    return true;
}

static bool send_all(tcp_conn_t* c, const uint8_t* buf, int n) {
    return tcp_send(c, buf, (uint16_t)n) == n;
}

// ===========================================================================
// Transcript hash
// ===========================================================================
static void transcript_add(tls_conn_t* t, const uint8_t* data, int len) {
    sha256_update(&t->transcript, data, len);
}
static void transcript_hash(tls_conn_t* t, uint8_t out[32]) {
    sha256_ctx_t copy = t->transcript;   // snapshot; hashing is non-destructive
    sha256_final(out, &copy);
}

// ===========================================================================
// Key schedule (RFC 8446 section 7.1)
// ===========================================================================
static void derive_traffic_keys(const uint8_t secret[32],
                                uint8_t key[32], uint8_t iv[12]) {
    tls13_expand_label(secret, "key", NULL, 0, key, 32);
    tls13_expand_label(secret, "iv",  NULL, 0, iv, 12);
}

// After ClientHello and ServerHello are in the transcript, derive the
// handshake traffic keys from the X25519 shared secret.
static void key_schedule_handshake(tls_conn_t* t, const uint8_t shared[32]) {
    uint8_t empty_hash[32];
    sha256((const uint8_t*)"", 0, empty_hash);

    uint8_t psk[32] = { 0 };
    uint8_t early_secret[32];
    hkdf_extract(NULL, 0, psk, 32, early_secret);

    uint8_t derived[32];
    tls13_derive_secret(early_secret, "derived", empty_hash, derived);
    hkdf_extract(derived, 32, shared, 32, t->handshake_secret);

    uint8_t th[32];
    transcript_hash(t, th);                       // CH || SH

    uint8_t s_hs_secret[32];
    tls13_derive_secret(t->handshake_secret, "c hs traffic", th, t->c_hs_secret);
    tls13_derive_secret(t->handshake_secret, "s hs traffic", th, s_hs_secret);

    derive_traffic_keys(t->c_hs_secret, t->c_hs_key, t->c_hs_iv);
    derive_traffic_keys(s_hs_secret,    t->s_hs_key, t->s_hs_iv);

    t->send_seq = t->recv_seq = 0;
}

// After the server Finished is in the transcript, derive the application keys.
static void key_schedule_application(tls_conn_t* t) {
    uint8_t empty_hash[32];
    sha256((const uint8_t*)"", 0, empty_hash);

    uint8_t derived[32];
    tls13_derive_secret(t->handshake_secret, "derived", empty_hash, derived);

    uint8_t psk[32] = { 0 };
    uint8_t master[32];
    hkdf_extract(derived, 32, psk, 32, master);

    uint8_t th[32];
    transcript_hash(t, th);                       // CH .. server Finished

    uint8_t c_ap_secret[32], s_ap_secret[32];
    tls13_derive_secret(master, "c ap traffic", th, c_ap_secret);
    tls13_derive_secret(master, "s ap traffic", th, s_ap_secret);

    derive_traffic_keys(c_ap_secret, t->c_ap_key, t->c_ap_iv);
    derive_traffic_keys(s_ap_secret, t->s_ap_key, t->s_ap_iv);
}

// ===========================================================================
// Record layer
// ===========================================================================
static void build_nonce(uint8_t nonce[12], const uint8_t iv[12], uint64_t seq) {
    memcpy(nonce, iv, 12);
    for (int i = 0; i < 8; i++) nonce[11 - i] ^= (uint8_t)(seq >> (8 * i));
}

// Encrypt `data` (of inner content type `inner_type`) as one record and send it.
static bool write_record(tls_conn_t* t, uint8_t inner_type,
                         const uint8_t* data, int len) {
    const uint8_t* key = t->app_keys ? t->c_ap_key : t->c_hs_key;
    const uint8_t* iv  = t->app_keys ? t->c_ap_iv  : t->c_hs_iv;

    int ptlen  = len + 1;                     // data || inner_type
    int reclen = ptlen + TLS_AEAD_TAG;

    uint8_t* out = t->rec;
    out[0] = CT_APPLICATION_DATA;
    out[1] = 0x03; out[2] = 0x03;
    out[3] = (uint8_t)(reclen >> 8);
    out[4] = (uint8_t)(reclen & 0xff);

    memcpy(t->pt, data, len);
    t->pt[len] = inner_type;

    uint8_t nonce[12];
    build_nonce(nonce, iv, t->send_seq);

    uint8_t tag[16];
    chacha20poly1305_encrypt(out + 5, tag, key, nonce, out, 5, t->pt, ptlen);
    memcpy(out + 5 + ptlen, tag, TLS_AEAD_TAG);

    t->send_seq++;
    return send_all(t->tcp, out, 5 + reclen);
}

// Read one record. On an encrypted record, decrypt it and return the inner
// content type in *inner_type and the plaintext length; ChangeCipherSpec
// records are skipped transparently. Cleartext handshake (ServerHello) is
// returned as-is. Returns -1 on error.
static int read_record(tls_conn_t* t, uint8_t* inner_type,
                       uint8_t* out, int outcap, uint32_t timeout_ms) {
    for (;;) {
        uint8_t hdr[5];
        if (!read_exact(t->tcp, hdr, 5, timeout_ms)) { tls_err("record header read failed"); return -1; }
        int type = hdr[0];
        int len  = (hdr[3] << 8) | hdr[4];
        if (len < 0 || len > TLS_REC_MAX) { tls_err("record too large"); return -1; }

        if (!read_exact(t->tcp, t->rec, len, timeout_ms)) { tls_err("record body read failed"); return -1; }

        if (type == CT_CHANGE_CIPHER_SPEC) continue;       // middlebox compat: ignore

        if (type == CT_HANDSHAKE) {
            // Cleartext handshake (only ServerHello before keys are set).
            if (len > outcap) { tls_err("handshake record too large"); return -1; }
            memcpy(out, t->rec, len);
            *inner_type = CT_HANDSHAKE;
            return len;
        }

        if (type == CT_ALERT && !t->app_keys && t->recv_seq == 0 && t->send_seq == 0) {
            tls_err("server sent a cleartext alert");
            return -1;
        }

        if (type == CT_APPLICATION_DATA) {
            const uint8_t* key = t->app_keys ? t->s_ap_key : t->s_hs_key;
            const uint8_t* iv  = t->app_keys ? t->s_ap_iv  : t->s_hs_iv;
            if (len < TLS_AEAD_TAG) { tls_err("short encrypted record"); return -1; }
            int ct_len = len - TLS_AEAD_TAG;

            uint8_t nonce[12];
            build_nonce(nonce, iv, t->recv_seq);

            if (!chacha20poly1305_decrypt(t->pt, key, nonce, hdr, 5,
                                          t->rec, ct_len, t->rec + ct_len)) {
                tls_err("record decrypt/auth failed");
                return -1;
            }
            t->recv_seq++;

            // Strip zero padding; the last non-zero byte is the inner type.
            int n = ct_len;
            while (n > 0 && t->pt[n - 1] == 0) n--;
            if (n == 0) { tls_err("empty inner plaintext"); return -1; }
            *inner_type = t->pt[n - 1];
            int body = n - 1;
            if (body > outcap) { tls_err("inner plaintext too large"); return -1; }
            memcpy(out, t->pt, body);
            return body;
        }

        tls_err("unexpected record type");
        return -1;
    }
}

// ===========================================================================
// Handshake byte stream (server flight), feeding the transcript as it goes
// ===========================================================================
static bool hs_getbytes(tls_conn_t* t, uint8_t* dst, int n, uint32_t timeout_ms) {
    int got = 0;
    while (got < n) {
        if (t->hs_cur_off >= t->hs_cur_len) {
            uint8_t inner_type;
            int r = read_record(t, &inner_type, t->hs_cur, sizeof(t->hs_cur), timeout_ms);
            if (r < 0) return false;
            if (inner_type == CT_ALERT) { tls_err("alert during handshake"); return false; }
            if (inner_type != CT_HANDSHAKE) { tls_err("non-handshake in flight"); return false; }
            transcript_add(t, t->hs_cur, r);       // every handshake byte is hashed
            t->hs_cur_len = r;
            t->hs_cur_off = 0;
        }
        int take = t->hs_cur_len - t->hs_cur_off;
        if (take > n - got) take = n - got;
        memcpy(dst + got, t->hs_cur + t->hs_cur_off, take);
        t->hs_cur_off += take;
        got += take;
    }
    return true;
}

// ===========================================================================
// ClientHello
// ===========================================================================
// A tiny append-and-backpatch buffer.
typedef struct { uint8_t* p; int len; int cap; } buf_t;
static void bput(buf_t* b, uint8_t v)                 { if (b->len < b->cap) b->p[b->len] = v; b->len++; }
static void bput16(buf_t* b, uint16_t v)              { bput(b, v >> 8); bput(b, v & 0xff); }
static void bputn(buf_t* b, const uint8_t* d, int n)  { for (int i = 0; i < n; i++) bput(b, d[i]); }

static bool send_client_hello(tls_conn_t* t, const char* host, const uint8_t pub[32]) {
    uint8_t body[1024];
    buf_t b = { body, 0, sizeof(body) };

    bput16(&b, 0x0303);                            // legacy_version
    uint8_t random[32];
    prng_get_bytes(random, 32);
    bputn(&b, random, 32);

    uint8_t sid[32];
    prng_get_bytes(sid, 32);
    bput(&b, 32); bputn(&b, sid, 32);              // legacy_session_id (compat)

    bput16(&b, 2); bput16(&b, 0x1303);             // cipher_suites: chacha20-poly1305
    bput(&b, 1); bput(&b, 0);                      // compression: null

    // Extensions.
    int ext_len_at = b.len;
    bput16(&b, 0);                                 // placeholder for extensions length
    int ext_start = b.len;

    // supported_versions (43): TLS 1.3
    bput16(&b, 43); bput16(&b, 3); bput(&b, 2); bput16(&b, 0x0304);

    // supported_groups (10): x25519
    bput16(&b, 10); bput16(&b, 4); bput16(&b, 2); bput16(&b, 0x001d);

    // signature_algorithms (13): a common set (unused -- we do not verify)
    static const uint16_t sigs[] = { 0x0403, 0x0804, 0x0401, 0x0805, 0x0806, 0x0807 };
    int nsig = (int)(sizeof(sigs) / sizeof(sigs[0]));
    bput16(&b, 13); bput16(&b, 2 + nsig * 2); bput16(&b, nsig * 2);
    for (int i = 0; i < nsig; i++) bput16(&b, sigs[i]);

    // key_share (51): x25519 public key
    bput16(&b, 51); bput16(&b, 38); bput16(&b, 36);
    bput16(&b, 0x001d); bput16(&b, 32); bputn(&b, pub, 32);

    // server_name (0): SNI
    int hlen = (int)strlen(host);
    bput16(&b, 0); bput16(&b, hlen + 5); bput16(&b, hlen + 3);
    bput(&b, 0); bput16(&b, hlen); bputn(&b, (const uint8_t*)host, hlen);

    // ALPN (16): http/1.1
    bput16(&b, 16); bput16(&b, 11); bput16(&b, 9); bput(&b, 8);
    bputn(&b, (const uint8_t*)"http/1.1", 8);

    int ext_len = b.len - ext_start;
    body[ext_len_at]     = (uint8_t)(ext_len >> 8);
    body[ext_len_at + 1] = (uint8_t)(ext_len & 0xff);

    if (b.len > (int)sizeof(body)) { tls_err("ClientHello overflow"); return false; }

    // Wrap in a handshake header, then a plaintext record.
    uint8_t hs[4 + sizeof(body)];
    hs[0] = HS_CLIENT_HELLO;
    hs[1] = (uint8_t)(b.len >> 16);
    hs[2] = (uint8_t)(b.len >> 8);
    hs[3] = (uint8_t)(b.len);
    memcpy(hs + 4, body, b.len);
    int hs_len = 4 + b.len;

    transcript_add(t, hs, hs_len);

    uint8_t rec[5 + 4 + sizeof(body)];
    rec[0] = CT_HANDSHAKE; rec[1] = 0x03; rec[2] = 0x01;   // legacy 0x0301 for CH
    rec[3] = (uint8_t)(hs_len >> 8);
    rec[4] = (uint8_t)(hs_len & 0xff);
    memcpy(rec + 5, hs, hs_len);
    return send_all(t->tcp, rec, 5 + hs_len);
}

// ===========================================================================
// ServerHello
// ===========================================================================
static bool parse_server_hello(tls_conn_t* t, const uint8_t* msg, int len,
                               uint8_t server_pub[32]) {
    // msg is the handshake message: type(1) len(3) body.
    if (len < 4 || msg[0] != HS_SERVER_HELLO) { tls_err("expected ServerHello"); return false; }
    const uint8_t* p = msg + 4;
    const uint8_t* end = msg + len;

    if (end - p < 2 + 32 + 1) { tls_err("ServerHello truncated"); return false; }
    p += 2;                                        // legacy_version
    p += 32;                                       // random
    int sid_len = *p++;
    if (end - p < sid_len + 3) { tls_err("ServerHello truncated"); return false; }
    p += sid_len;                                  // session id echo

    uint16_t cipher = ((uint16_t)p[0] << 8) | p[1]; p += 2;
    if (cipher != 0x1303) { tls_err("server chose an unsupported cipher"); return false; }
    p += 1;                                         // compression

    if (end - p < 2) { tls_err("ServerHello no extensions"); return false; }
    int ext_total = (p[0] << 8) | p[1]; p += 2;
    const uint8_t* ext_end = p + ext_total;
    if (ext_end > end) { tls_err("ServerHello ext overflow"); return false; }

    bool got_key = false;
    while (p + 4 <= ext_end) {
        uint16_t etype = ((uint16_t)p[0] << 8) | p[1];
        uint16_t elen  = ((uint16_t)p[2] << 8) | p[3];
        p += 4;
        if (p + elen > ext_end) { tls_err("ServerHello ext len"); return false; }

        if (etype == 51) {                          // key_share
            if (elen < 4) { tls_err("bad key_share"); return false; }
            uint16_t group = ((uint16_t)p[0] << 8) | p[1];
            uint16_t klen  = ((uint16_t)p[2] << 8) | p[3];
            if (group != 0x001d || klen != 32 || elen < 4 + 32) {
                tls_err("server key_share is not X25519");
                return false;
            }
            memcpy(server_pub, p + 4, 32);
            got_key = true;
        }
        p += elen;
    }
    if (!got_key) { tls_err("server sent no X25519 key_share (HelloRetry?)"); return false; }
    return true;
}

// ===========================================================================
// Server flight: EncryptedExtensions, Certificate*, CertVerify, Finished
// ===========================================================================
static bool read_server_flight(tls_conn_t* t, uint32_t timeout_ms) {
    for (;;) {
        uint8_t hdr[4];
        if (!hs_getbytes(t, hdr, 4, timeout_ms)) return false;
        int htype = hdr[0];
        int hlen  = (hdr[1] << 16) | (hdr[2] << 8) | hdr[3];

        // Consume (and discard) the message body; every byte was already fed to
        // the transcript by hs_getbytes.
        uint8_t skip[512];
        int left = hlen;
        while (left > 0) {
            int chunk = left < (int)sizeof(skip) ? left : (int)sizeof(skip);
            if (!hs_getbytes(t, skip, chunk, timeout_ms)) return false;
            left -= chunk;
        }

        if (htype == HS_FINISHED) return true;      // end of the server flight
        if (htype == HS_CERT_REQUEST) {
            // We do not present a client certificate; most public servers do not
            // request one. Proceeding without it is the documented limitation.
        }
        // EE / Certificate / CertificateVerify: accepted without verification.
    }
}

// ===========================================================================
// Client Finished
// ===========================================================================
static bool send_client_finished(tls_conn_t* t) {
    uint8_t finished_key[32];
    tls13_expand_label(t->c_hs_secret, "finished", NULL, 0, finished_key, 32);

    uint8_t th[32];
    transcript_hash(t, th);                         // CH .. server Finished

    uint8_t verify[32];
    hmac_sha256(finished_key, 32, th, 32, verify);

    uint8_t msg[4 + 32];
    msg[0] = HS_FINISHED; msg[1] = 0; msg[2] = 0; msg[3] = 32;
    memcpy(msg + 4, verify, 32);

    // Sent under handshake keys.
    return write_record(t, CT_HANDSHAKE, msg, sizeof(msg));
}

// ===========================================================================
// Public API
// ===========================================================================
tls_conn_t* tls_connect(const char* host, uint32_t ip, uint16_t port,
                        uint32_t timeout_ms) {
    tls_err("");
    if (!host || ip == 0 || port == 0) { tls_err("bad arguments"); return NULL; }

    tls_conn_t* t = (tls_conn_t*)kzalloc(sizeof(tls_conn_t));
    if (!t) { tls_err("out of memory"); return NULL; }

    sha256_init(&t->transcript);

    t->tcp = tcp_connect(ip, port, timeout_ms);
    if (!t->tcp) { tls_err("TCP connect failed"); kfree(t); return NULL; }

    uint8_t pub[32];
    prng_get_bytes(t->priv, 32);
    x25519_base(pub, t->priv);

    if (!send_client_hello(t, host, pub)) { tls_err("ClientHello send failed"); goto fail; }

    // ServerHello (cleartext handshake record).
    uint8_t inner_type;
    int shlen = read_record(t, &inner_type, t->hs_cur, sizeof(t->hs_cur), timeout_ms);
    if (shlen < 0) goto fail;
    if (inner_type != CT_HANDSHAKE) { tls_err("expected ServerHello record"); goto fail; }
    transcript_add(t, t->hs_cur, shlen);

    uint8_t server_pub[32];
    if (!parse_server_hello(t, t->hs_cur, shlen, server_pub)) goto fail;

    uint8_t shared[32];
    x25519(shared, t->priv, server_pub);
    key_schedule_handshake(t, shared);

    // Server flight is now encrypted under the server handshake key.
    if (!read_server_flight(t, timeout_ms)) goto fail;

    // Application keys are derived over CH..server Finished.
    key_schedule_application(t);

    if (!send_client_finished(t)) { tls_err("Finished send failed"); goto fail; }

    // From here on both directions use the application keys.
    t->app_keys = true;
    t->send_seq = t->recv_seq = 0;
    t->established = true;
    return t;

fail:
    if (t->tcp) tcp_close(t->tcp);
    kfree(t);
    return NULL;
}

int tls_send(tls_conn_t* t, const void* data, int len) {
    if (!t || !t->established) return -1;
    const uint8_t* p = (const uint8_t*)data;
    int sent = 0;
    while (sent < len) {
        int chunk = len - sent;
        if (chunk > TLS_PLAINTEXT_MAX) chunk = TLS_PLAINTEXT_MAX;
        if (!write_record(t, CT_APPLICATION_DATA, p + sent, chunk)) return sent ? sent : -1;
        sent += chunk;
    }
    return sent;
}

int tls_recv(tls_conn_t* t, void* buf, int len, uint32_t timeout_ms) {
    if (!t || !t->established) return -1;

    // Serve any leftover decrypted application data first.
    if (t->rbuf_off >= t->rbuf_len) {
        for (;;) {
            uint8_t inner_type;
            int r = read_record(t, &inner_type, t->rbuf, sizeof(t->rbuf), timeout_ms);
            if (r < 0) return -1;
            if (inner_type == CT_APPLICATION_DATA) { t->rbuf_len = r; t->rbuf_off = 0; break; }
            if (inner_type == CT_HANDSHAKE) continue;      // NewSessionTicket etc.: ignore
            if (inner_type == CT_ALERT) return 0;          // close_notify or fatal: end of stream
            // anything else: ignore and keep reading
        }
    }

    int avail = t->rbuf_len - t->rbuf_off;
    int n = avail < len ? avail : len;
    memcpy(buf, t->rbuf + t->rbuf_off, n);
    t->rbuf_off += n;
    return n;
}

void tls_close(tls_conn_t* t) {
    if (!t) return;
    if (t->tcp) tcp_close(t->tcp);
    kfree(t);
}
