// Chained hash table with FNV-1a hashing and load-factor driven growth
#include <lib/hashtable.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

#define HT_MAX_LOAD_NUM 3   // Grow once entries > 3/4 of bucket count
#define HT_MAX_LOAD_DEN 4
#define HT_MAX_BUCKETS  4096

uint64_t hash_fnv1a(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 1469598103934665603ULL; // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;           // FNV prime
    }
    return h;
}

uint64_t hash_string(const char* s) {
    if (!s) return 0;
    return hash_fnv1a(s, strlen(s));
}

uint64_t hash_u64(uint64_t v) {
    // Fibonacci-style mixing; cheap and well distributed for dense ids.
    v ^= v >> 33;
    v *= 0xFF51AFD7ED558CCDULL;
    v ^= v >> 33;
    v *= 0xC4CEB9FE1A85EC53ULL;
    v ^= v >> 33;
    return v;
}

static size_t round_up_pow2(size_t v) {
    if (v < 8) return 8;
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

hashtable_t* hashtable_create(size_t initial_buckets, bool string_keys) {
    hashtable_t* ht = (hashtable_t*)kzalloc(sizeof(hashtable_t));
    if (!ht) return NULL;

    size_t n = round_up_pow2(initial_buckets);
    ht->buckets = (hnode_t**)kzalloc(n * sizeof(hnode_t*));
    if (!ht->buckets) {
        kfree(ht);
        return NULL;
    }

    ht->bucket_count = n;
    ht->entries      = 0;
    ht->collisions   = 0;
    ht->string_keys  = string_keys;
    return ht;
}

void hashtable_destroy(hashtable_t* ht) {
    if (!ht) return;

    for (size_t i = 0; i < ht->bucket_count; i++) {
        hnode_t* n = ht->buckets[i];
        while (n) {
            hnode_t* next = n->next;
            if (n->skey) kfree(n->skey);
            kfree(n);
            n = next;
        }
    }
    kfree(ht->buckets);
    kfree(ht);
}

static void hashtable_grow(hashtable_t* ht) {
    if (ht->bucket_count >= HT_MAX_BUCKETS) return;

    size_t new_count = ht->bucket_count * 2;
    hnode_t** nb = (hnode_t**)kzalloc(new_count * sizeof(hnode_t*));
    if (!nb) return; // Growth is best effort; chains simply stay longer.

    size_t collisions = 0;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        hnode_t* n = ht->buckets[i];
        while (n) {
            hnode_t* next = n->next;
            size_t idx = (size_t)(n->hash & (new_count - 1));
            if (nb[idx]) collisions++;
            n->next = nb[idx];
            nb[idx] = n;
            n = next;
        }
    }

    kfree(ht->buckets);
    ht->buckets      = nb;
    ht->bucket_count = new_count;
    ht->collisions   = collisions;
}

static hnode_t* find_node(const hashtable_t* ht, uint64_t hash,
                          const char* skey, uint64_t ikey) {
    size_t idx = (size_t)(hash & (ht->bucket_count - 1));
    for (hnode_t* n = ht->buckets[idx]; n; n = n->next) {
        if (n->hash != hash) continue;
        if (ht->string_keys) {
            if (n->skey && skey && strcmp(n->skey, skey) == 0) return n;
        } else if (n->ikey == ikey) {
            return n;
        }
    }
    return NULL;
}

static int hashtable_insert(hashtable_t* ht, uint64_t hash, const char* skey,
                            uint64_t ikey, void* value) {
    hnode_t* existing = find_node(ht, hash, skey, ikey);
    if (existing) {
        existing->value = value;
        return 0;
    }

    if (ht->entries * HT_MAX_LOAD_DEN > ht->bucket_count * HT_MAX_LOAD_NUM) {
        hashtable_grow(ht);
    }

    hnode_t* n = (hnode_t*)kzalloc(sizeof(hnode_t));
    if (!n) return -1;

    n->hash  = hash;
    n->ikey  = ikey;
    n->value = value;

    if (ht->string_keys && skey) {
        size_t len = strlen(skey);
        n->skey = (char*)kmalloc(len + 1);
        if (!n->skey) {
            kfree(n);
            return -1;
        }
        memcpy(n->skey, skey, len + 1);
    }

    size_t idx = (size_t)(hash & (ht->bucket_count - 1));
    if (ht->buckets[idx]) ht->collisions++;
    n->next = ht->buckets[idx];
    ht->buckets[idx] = n;
    ht->entries++;
    return 0;
}

static bool hashtable_unlink(hashtable_t* ht, uint64_t hash, const char* skey,
                             uint64_t ikey) {
    size_t idx = (size_t)(hash & (ht->bucket_count - 1));
    hnode_t** link = &ht->buckets[idx];

    while (*link) {
        hnode_t* n = *link;
        bool match = (n->hash == hash) &&
                     (ht->string_keys ? (n->skey && skey && strcmp(n->skey, skey) == 0)
                                      : (n->ikey == ikey));
        if (match) {
            *link = n->next;
            if (n->skey) kfree(n->skey);
            kfree(n);
            ht->entries--;
            return true;
        }
        link = &n->next;
    }
    return false;
}

int hashtable_put(hashtable_t* ht, const char* key, void* value) {
    if (!ht || !key || !ht->string_keys) return -1;
    return hashtable_insert(ht, hash_string(key), key, 0, value);
}

void* hashtable_get(const hashtable_t* ht, const char* key) {
    if (!ht || !key || !ht->string_keys) return NULL;
    hnode_t* n = find_node(ht, hash_string(key), key, 0);
    return n ? n->value : NULL;
}

bool hashtable_remove(hashtable_t* ht, const char* key) {
    if (!ht || !key || !ht->string_keys) return false;
    return hashtable_unlink(ht, hash_string(key), key, 0);
}

int hashtable_put_u64(hashtable_t* ht, uint64_t key, void* value) {
    if (!ht || ht->string_keys) return -1;
    return hashtable_insert(ht, hash_u64(key), NULL, key, value);
}

void* hashtable_get_u64(const hashtable_t* ht, uint64_t key) {
    if (!ht || ht->string_keys) return NULL;
    hnode_t* n = find_node(ht, hash_u64(key), NULL, key);
    return n ? n->value : NULL;
}

bool hashtable_remove_u64(hashtable_t* ht, uint64_t key) {
    if (!ht || ht->string_keys) return false;
    return hashtable_unlink(ht, hash_u64(key), NULL, key);
}

size_t hashtable_size(const hashtable_t* ht)       { return ht ? ht->entries : 0; }
size_t hashtable_buckets(const hashtable_t* ht)    { return ht ? ht->bucket_count : 0; }
size_t hashtable_collisions(const hashtable_t* ht) { return ht ? ht->collisions : 0; }

size_t hashtable_longest_chain(const hashtable_t* ht) {
    if (!ht) return 0;
    size_t longest = 0;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        size_t len = 0;
        for (hnode_t* n = ht->buckets[i]; n; n = n->next) len++;
        if (len > longest) longest = len;
    }
    return longest;
}

void hashtable_foreach(const hashtable_t* ht, hashtable_iter_fn fn, void* ctx) {
    if (!ht || !fn) return;
    for (size_t i = 0; i < ht->bucket_count; i++) {
        for (hnode_t* n = ht->buckets[i]; n; n = n->next) fn(n, ctx);
    }
}
