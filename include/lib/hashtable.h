#ifndef _LIB_HASHTABLE_H
#define _LIB_HASHTABLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Separately-chained hash table with FNV-1a hashing and automatic growth.
// Keys are either NUL-terminated strings (owned copies) or raw 64-bit ids.

typedef struct hnode {
    uint64_t      hash;
    char*         skey;   // NULL for integer-keyed entries
    uint64_t      ikey;
    void*         value;
    struct hnode* next;
} hnode_t;

typedef struct hashtable {
    hnode_t** buckets;
    size_t    bucket_count;
    size_t    entries;
    size_t    collisions;
    bool      string_keys;
} hashtable_t;

uint64_t hash_fnv1a(const void* data, size_t len);
uint64_t hash_string(const char* s);
uint64_t hash_u64(uint64_t v);

hashtable_t* hashtable_create(size_t initial_buckets, bool string_keys);
void         hashtable_destroy(hashtable_t* ht);

int   hashtable_put(hashtable_t* ht, const char* key, void* value);
void* hashtable_get(const hashtable_t* ht, const char* key);
bool  hashtable_remove(hashtable_t* ht, const char* key);

int   hashtable_put_u64(hashtable_t* ht, uint64_t key, void* value);
void* hashtable_get_u64(const hashtable_t* ht, uint64_t key);
bool  hashtable_remove_u64(hashtable_t* ht, uint64_t key);

size_t hashtable_size(const hashtable_t* ht);
size_t hashtable_buckets(const hashtable_t* ht);
size_t hashtable_collisions(const hashtable_t* ht);
size_t hashtable_longest_chain(const hashtable_t* ht);

typedef void (*hashtable_iter_fn)(const hnode_t* node, void* ctx);
void hashtable_foreach(const hashtable_t* ht, hashtable_iter_fn fn, void* ctx);

#endif // _LIB_HASHTABLE_H
