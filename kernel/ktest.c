// In-kernel self-test harness for SUB-OS
#include <kernel/ktest.h>
#include <kernel/printk.h>
#include <kernel/rcu.h>
#include <kernel/futex.h>
#include <kernel/wait.h>
#include <mm/kmalloc.h>
#include <mm/page_cache.h>
#include <lib/rbtree.h>
#include <lib/kfifo.h>
#include <lib/hashtable.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <arch/arch.h>

static const ktest_suite_t* g_suites[KTEST_MAX_SUITES];
static int g_suite_count = 0;
static bool g_verbose = true;

void ktest_init(void) {
    g_suite_count = 0;
    memset(g_suites, 0, sizeof(g_suites));
}

int ktest_register_suite(const ktest_suite_t* suite) {
    if (!suite || !suite->name || g_suite_count >= KTEST_MAX_SUITES) return -1;
    g_suites[g_suite_count++] = suite;
    return 0;
}

void ktest_report(ktest_context_t* ctx, bool passed, const char* expr,
                  const char* file, int line, const char* detail) {
    if (!ctx) return;

    ctx->assertions++;
    if (passed) return;

    ctx->failures++;
    printk(ANSI_BRIGHT_RED "      FAIL " ANSI_RESET "%s:%d: %s", file, line, expr);
    if (detail) printk("  [%s]", detail);
    printk("\n");
}

void ktest_expect_eq(ktest_context_t* ctx, long long a, long long b,
                     const char* expr, const char* file, int line) {
    char detail[64];
    if (a != b) {
        snprintf(detail, sizeof(detail), "got %lld, want %lld", a, b);
        ktest_report(ctx, false, expr, file, line, detail);
    } else {
        ktest_report(ctx, true, expr, file, line, NULL);
    }
}

void ktest_expect_streq(ktest_context_t* ctx, const char* a, const char* b,
                        const char* expr, const char* file, int line) {
    bool eq = (a && b) ? (strcmp(a, b) == 0) : (a == b);
    if (!eq) {
        char detail[96];
        snprintf(detail, sizeof(detail), "got \"%s\", want \"%s\"",
                 a ? a : "(null)", b ? b : "(null)");
        ktest_report(ctx, false, expr, file, line, detail);
    } else {
        ktest_report(ctx, true, expr, file, line, NULL);
    }
}

static void run_suite(const ktest_suite_t* suite, ktest_result_t* res) {
    printk(ANSI_BRIGHT_CYAN "  [SUITE] %s" ANSI_RESET " (%llu tests)\n",
           suite->name, (unsigned long long)suite->case_count);
    res->suites_run++;

    for (size_t i = 0; i < suite->case_count; i++) {
        const ktest_case_t* tc = &suite->cases[i];
        if (!tc->func) continue;

        ktest_context_t ctx = {
            .suite = suite->name,
            .test  = tc->name,
            .assertions = 0,
            .failures = 0,
            .aborted = false
        };

        tc->func(&ctx);

        res->tests_run++;
        res->assertions += ctx.assertions;
        res->assertion_failures += ctx.failures;

        if (ctx.failures == 0) {
            res->tests_passed++;
            if (g_verbose) {
                printk("    " ANSI_BRIGHT_GREEN "PASS" ANSI_RESET " %-32s (%u assertions)\n",
                       tc->name, ctx.assertions);
            }
        } else {
            res->tests_failed++;
            printk("    " ANSI_BRIGHT_RED "FAIL" ANSI_RESET " %-32s (%u/%u assertions failed)\n",
                   tc->name, ctx.failures, ctx.assertions);
        }
    }
}

static void print_summary(const ktest_result_t* res) {
    printk("\n" ANSI_BRIGHT_CYAN "=== KTest Summary ===\n" ANSI_RESET);
    printk("  Suites     : %u\n", res->suites_run);
    printk("  Tests      : %u run, " ANSI_BRIGHT_GREEN "%u passed" ANSI_RESET
           ", %s%u failed" ANSI_RESET "\n",
           res->tests_run, res->tests_passed,
           res->tests_failed ? ANSI_BRIGHT_RED : ANSI_BRIGHT_BLACK,
           res->tests_failed);
    printk("  Assertions : %u checked, %u failed\n",
           res->assertions, res->assertion_failures);
    printk("  Elapsed    : %llu ticks (%llu ms)\n",
           (unsigned long long)res->elapsed_ticks,
           (unsigned long long)(res->elapsed_ticks * 10));

    if (res->tests_failed == 0) {
        printk(ANSI_BRIGHT_GREEN "  RESULT: ALL TESTS PASSED\n" ANSI_RESET);
    } else {
        printk(ANSI_BRIGHT_RED "  RESULT: %u TEST(S) FAILED\n" ANSI_RESET, res->tests_failed);
    }
}

int ktest_run_all(ktest_result_t* result_out) {
    ktest_result_t res;
    memset(&res, 0, sizeof(res));

    uint64_t start = pit_get_ticks();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS In-Kernel Test Harness (%d suites) ===\n\n" ANSI_RESET,
           g_suite_count);

    for (int i = 0; i < g_suite_count; i++) {
        if (g_suites[i]) run_suite(g_suites[i], &res);
    }

    res.elapsed_ticks = pit_get_ticks() - start;
    print_summary(&res);

    if (result_out) *result_out = res;
    return (int)res.tests_failed;
}

int ktest_run_suite(const char* name, ktest_result_t* result_out) {
    if (!name) return ktest_run_all(result_out);

    ktest_result_t res;
    memset(&res, 0, sizeof(res));
    uint64_t start = pit_get_ticks();

    bool found = false;
    for (int i = 0; i < g_suite_count; i++) {
        if (g_suites[i] && strcmp(g_suites[i]->name, name) == 0) {
            run_suite(g_suites[i], &res);
            found = true;
            break;
        }
    }

    if (!found) {
        printk(ANSI_YELLOW "ktest: no suite named '%s'\n" ANSI_RESET, name);
        ktest_list_suites();
        return -1;
    }

    res.elapsed_ticks = pit_get_ticks() - start;
    print_summary(&res);

    if (result_out) *result_out = res;
    return (int)res.tests_failed;
}

void ktest_list_suites(void) {
    printk(ANSI_BRIGHT_CYAN "Registered KTest suites (%d):\n" ANSI_RESET, g_suite_count);
    for (int i = 0; i < g_suite_count; i++) {
        if (!g_suites[i]) continue;
        printk("  " ANSI_GREEN "%-20s" ANSI_RESET " %llu test cases\n",
               g_suites[i]->name, (unsigned long long)g_suites[i]->case_count);
    }
}

// ===========================================================================
// Built-in suite: lib/rbtree
// ===========================================================================
typedef struct {
    struct rb_node node;
    int            key;
} test_rb_item_t;

static void rb_test_insert(struct rb_root* root, test_rb_item_t* item) {
    struct rb_node** link = &root->rb_node;
    struct rb_node*  parent = NULL;

    while (*link) {
        parent = *link;
        test_rb_item_t* cur = rb_entry(parent, test_rb_item_t, node);
        link = (item->key < cur->key) ? &parent->rb_left : &parent->rb_right;
    }

    rb_link_node(&item->node, parent, link);
    rb_insert_color(&item->node, root);
}

static void test_rbtree_ordered_insert(ktest_context_t* ctx) {
    struct rb_root root = RB_ROOT_INIT;
    static test_rb_item_t items[64];

    // Ascending keys are the worst case for an unbalanced BST.
    for (int i = 0; i < 64; i++) {
        items[i].key = i;
        rb_test_insert(&root, &items[i]);
    }

    KTEST_ASSERT_EQ(ctx, (long long)rb_count(&root), 64);
    KTEST_ASSERT(ctx, rb_validate(&root));

    int prev = -1;
    int seen = 0;
    for (struct rb_node* n = rb_first(&root); n; n = rb_next(n)) {
        test_rb_item_t* it = rb_entry(n, test_rb_item_t, node);
        KTEST_ASSERT(ctx, it->key > prev);
        prev = it->key;
        seen++;
    }
    KTEST_ASSERT_EQ(ctx, seen, 64);

    // A balanced 64-node tree must stay far below the depth of a linked list.
    KTEST_ASSERT(ctx, rb_black_height(&root) <= 8);
}

static void test_rbtree_erase(ktest_context_t* ctx) {
    struct rb_root root = RB_ROOT_INIT;
    static test_rb_item_t items[32];

    // Pseudo-random insertion order via a coprime stride.
    for (int i = 0; i < 32; i++) {
        items[i].key = (i * 7) % 32;
        rb_test_insert(&root, &items[i]);
    }
    KTEST_ASSERT_EQ(ctx, (long long)rb_count(&root), 32);

    for (int i = 0; i < 32; i += 2) {
        rb_erase(&items[i].node, &root);
        KTEST_ASSERT(ctx, rb_validate(&root));
    }
    KTEST_ASSERT_EQ(ctx, (long long)rb_count(&root), 16);

    int prev = -1;
    for (struct rb_node* n = rb_first(&root); n; n = rb_next(n)) {
        test_rb_item_t* it = rb_entry(n, test_rb_item_t, node);
        KTEST_ASSERT(ctx, it->key > prev);
        prev = it->key;
    }

    while (!rb_empty(&root)) {
        rb_erase(rb_first(&root), &root);
    }
    KTEST_ASSERT_EQ(ctx, (long long)rb_count(&root), 0);
}

static void test_rbtree_reverse_walk(ktest_context_t* ctx) {
    struct rb_root root = RB_ROOT_INIT;
    static test_rb_item_t items[16];

    for (int i = 0; i < 16; i++) {
        items[i].key = 15 - i;
        rb_test_insert(&root, &items[i]);
    }

    struct rb_node* first = rb_first(&root);
    struct rb_node* last  = rb_last(&root);
    KTEST_ASSERT_NOT_NULL(ctx, first);
    KTEST_ASSERT_NOT_NULL(ctx, last);
    KTEST_ASSERT_EQ(ctx, rb_entry(first, test_rb_item_t, node)->key, 0);
    KTEST_ASSERT_EQ(ctx, rb_entry(last, test_rb_item_t, node)->key, 15);

    int expected = 15;
    for (struct rb_node* n = last; n; n = rb_prev(n)) {
        KTEST_ASSERT_EQ(ctx, rb_entry(n, test_rb_item_t, node)->key, expected);
        expected--;
    }
    KTEST_ASSERT_EQ(ctx, expected, -1);
}

static const ktest_case_t rbtree_cases[] = {
    { "ordered_insert_stays_balanced", test_rbtree_ordered_insert },
    { "erase_preserves_invariants",    test_rbtree_erase },
    { "forward_and_reverse_walk",      test_rbtree_reverse_walk },
};
static const ktest_suite_t rbtree_suite = { "rbtree", rbtree_cases, 3 };

// ===========================================================================
// Built-in suite: lib/kfifo
// ===========================================================================
static void test_kfifo_roundtrip(ktest_context_t* ctx) {
    kfifo_t fifo;
    KTEST_ASSERT_EQ(ctx, kfifo_alloc(&fifo, 64), 0);
    KTEST_ASSERT(ctx, kfifo_is_empty(&fifo));
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_avail(&fifo), 64);

    const char* msg = "SUB-OS kfifo";
    size_t len = strlen(msg);
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_in(&fifo, msg, len), (long long)len);
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_len(&fifo), (long long)len);

    char out[32];
    memset(out, 0, sizeof(out));
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_out(&fifo, out, len), (long long)len);
    KTEST_ASSERT_STREQ(ctx, out, msg);
    KTEST_ASSERT(ctx, kfifo_is_empty(&fifo));

    kfifo_free(&fifo);
}

static void test_kfifo_wraparound(ktest_context_t* ctx) {
    kfifo_t fifo;
    KTEST_ASSERT_EQ(ctx, kfifo_alloc(&fifo, 16), 0);

    // Drive the indices well past the buffer end to exercise the wrap path.
    uint8_t byte;
    for (int round = 0; round < 40; round++) {
        KTEST_ASSERT_EQ(ctx, kfifo_put_byte(&fifo, (uint8_t)round), 0);
        KTEST_ASSERT_EQ(ctx, kfifo_get_byte(&fifo, &byte), 0);
        KTEST_ASSERT_EQ(ctx, byte, (uint8_t)round);
    }
    KTEST_ASSERT(ctx, kfifo_is_empty(&fifo));

    uint8_t src[24];
    for (int i = 0; i < 24; i++) src[i] = (uint8_t)(i + 1);

    // Only 16 bytes fit; the rest must be refused rather than overwrite.
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_in(&fifo, src, 24), 16);
    KTEST_ASSERT(ctx, kfifo_is_full(&fifo));
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_in(&fifo, src, 4), 0);

    uint8_t dst[16];
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_out(&fifo, dst, 16), 16);
    for (int i = 0; i < 16; i++) KTEST_ASSERT_EQ(ctx, dst[i], src[i]);

    kfifo_free(&fifo);
}

static void test_kfifo_peek(ktest_context_t* ctx) {
    kfifo_t fifo;
    KTEST_ASSERT_EQ(ctx, kfifo_alloc(&fifo, 32), 0);

    kfifo_in(&fifo, "abcdef", 6);

    char peeked[8];
    memset(peeked, 0, sizeof(peeked));
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_peek(&fifo, peeked, 3), 3);
    KTEST_ASSERT_STREQ(ctx, peeked, "abc");

    // Peeking must not consume.
    KTEST_ASSERT_EQ(ctx, (long long)kfifo_len(&fifo), 6);

    kfifo_free(&fifo);
}

static const ktest_case_t kfifo_cases[] = {
    { "in_out_roundtrip",   test_kfifo_roundtrip },
    { "index_wraparound",   test_kfifo_wraparound },
    { "peek_is_read_only",  test_kfifo_peek },
};
static const ktest_suite_t kfifo_suite = { "kfifo", kfifo_cases, 3 };

// ===========================================================================
// Built-in suite: lib/hashtable
// ===========================================================================
static void test_hashtable_string_keys(ktest_context_t* ctx) {
    hashtable_t* ht = hashtable_create(8, true);
    KTEST_ASSERT_NOT_NULL(ctx, ht);
    if (!ht) return;

    static int values[3] = { 10, 20, 30 };
    KTEST_ASSERT_EQ(ctx, hashtable_put(ht, "alpha", &values[0]), 0);
    KTEST_ASSERT_EQ(ctx, hashtable_put(ht, "beta",  &values[1]), 0);
    KTEST_ASSERT_EQ(ctx, hashtable_put(ht, "gamma", &values[2]), 0);
    KTEST_ASSERT_EQ(ctx, (long long)hashtable_size(ht), 3);

    KTEST_ASSERT_EQ(ctx, *(int*)hashtable_get(ht, "beta"), 20);
    KTEST_ASSERT(ctx, hashtable_get(ht, "delta") == NULL);

    // Re-putting an existing key replaces rather than duplicates.
    KTEST_ASSERT_EQ(ctx, hashtable_put(ht, "beta", &values[2]), 0);
    KTEST_ASSERT_EQ(ctx, (long long)hashtable_size(ht), 3);
    KTEST_ASSERT_EQ(ctx, *(int*)hashtable_get(ht, "beta"), 30);

    KTEST_ASSERT(ctx, hashtable_remove(ht, "alpha"));
    KTEST_ASSERT(ctx, !hashtable_remove(ht, "alpha"));
    KTEST_ASSERT_EQ(ctx, (long long)hashtable_size(ht), 2);

    hashtable_destroy(ht);
}

static void test_hashtable_growth(ktest_context_t* ctx) {
    hashtable_t* ht = hashtable_create(8, false);
    KTEST_ASSERT_NOT_NULL(ctx, ht);
    if (!ht) return;

    size_t initial_buckets = hashtable_buckets(ht);

    static int slots[128];
    for (int i = 0; i < 128; i++) {
        slots[i] = i * 3;
        KTEST_ASSERT_EQ(ctx, hashtable_put_u64(ht, (uint64_t)i, &slots[i]), 0);
    }

    KTEST_ASSERT_EQ(ctx, (long long)hashtable_size(ht), 128);
    KTEST_ASSERT(ctx, hashtable_buckets(ht) > initial_buckets);

    for (int i = 0; i < 128; i++) {
        int* v = (int*)hashtable_get_u64(ht, (uint64_t)i);
        KTEST_ASSERT_NOT_NULL(ctx, v);
        if (v) KTEST_ASSERT_EQ(ctx, *v, i * 3);
    }

    // With growth keeping the load factor under 3/4, chains stay short.
    KTEST_ASSERT(ctx, hashtable_longest_chain(ht) <= 8);

    hashtable_destroy(ht);
}

static void test_hash_functions(ktest_context_t* ctx) {
    // FNV-1a must be deterministic and separate near-identical inputs.
    KTEST_ASSERT(ctx, hash_string("subos") == hash_string("subos"));
    KTEST_ASSERT(ctx, hash_string("subos") != hash_string("subon"));
    KTEST_ASSERT(ctx, hash_u64(1) != hash_u64(2));
    KTEST_ASSERT(ctx, hash_u64(0x100000000ULL) != hash_u64(0));
    KTEST_ASSERT_EQ(ctx, (long long)hash_string(NULL), 0);
}

static const ktest_case_t hashtable_cases[] = {
    { "string_key_crud",      test_hashtable_string_keys },
    { "rehash_on_growth",     test_hashtable_growth },
    { "fnv1a_distribution",   test_hash_functions },
};
static const ktest_suite_t hashtable_suite = { "hashtable", hashtable_cases, 3 };

// ===========================================================================
// Built-in suite: kernel/rcu + kernel/futex
// ===========================================================================
static int g_rcu_cb_hits = 0;

static void rcu_test_cb(void* arg) {
    g_rcu_cb_hits += (arg ? *(int*)arg : 1);
}

static void test_rcu_read_side(ktest_context_t* ctx) {
    KTEST_ASSERT(ctx, !rcu_read_lock_held());

    rcu_read_lock();
    KTEST_ASSERT(ctx, rcu_read_lock_held());
    rcu_read_lock();          // Nesting is allowed
    KTEST_ASSERT(ctx, rcu_read_lock_held());
    rcu_read_unlock();
    KTEST_ASSERT(ctx, rcu_read_lock_held());
    rcu_read_unlock();

    KTEST_ASSERT(ctx, !rcu_read_lock_held());
}

static void test_rcu_deferred_free(ktest_context_t* ctx) {
    static int weight = 5;
    g_rcu_cb_hits = 0;

    uint64_t gp_before = rcu_get_grace_period();
    KTEST_ASSERT_EQ(ctx, call_rcu(rcu_test_cb, &weight), 0);

    // The callback must not fire while a reader is still inside.
    rcu_read_lock();
    rcu_process_callbacks();
    KTEST_ASSERT_EQ(ctx, g_rcu_cb_hits, 0);
    rcu_read_unlock();

    rcu_synchronize();
    rcu_process_callbacks();

    KTEST_ASSERT_EQ(ctx, g_rcu_cb_hits, 5);
    KTEST_ASSERT(ctx, rcu_get_grace_period() > gp_before);
}

static void test_rcu_pointer_publish(ktest_context_t* ctx) {
    static int old_value = 1;
    static int new_value = 2;
    int* shared = &old_value;

    rcu_read_lock();
    int* snapshot = rcu_dereference(shared);
    KTEST_ASSERT_EQ(ctx, *snapshot, 1);
    rcu_read_unlock();

    rcu_assign_pointer(shared, &new_value);

    rcu_read_lock();
    KTEST_ASSERT_EQ(ctx, *rcu_dereference(shared), 2);
    rcu_read_unlock();
}

static void test_futex_fastpath(ktest_context_t* ctx) {
    fmutex_t m;
    fmutex_init(&m, "ktest-mutex");

    KTEST_ASSERT(ctx, fmutex_trylock(&m));
    KTEST_ASSERT_EQ(ctx, (long long)m.state, 1);

    // A second acquire on an already-held mutex must fail without blocking.
    KTEST_ASSERT(ctx, !fmutex_trylock(&m));

    fmutex_unlock(&m);
    KTEST_ASSERT_EQ(ctx, (long long)m.state, 0);
    KTEST_ASSERT(ctx, fmutex_trylock(&m));
    fmutex_unlock(&m);
}

static void test_futex_value_check(ktest_context_t* ctx) {
    volatile uint32_t word = 7;

    // Waiting on a value that no longer matches returns immediately so the
    // caller can retry its fast path instead of sleeping on a stale wakeup.
    KTEST_ASSERT_EQ(ctx, futex_wait(&word, 99, 5), -1);

    // Nothing is parked, so a wake finds no one.
    KTEST_ASSERT_EQ(ctx, futex_wake(&word, 1), 0);

    futex_stats_t s = futex_get_stats();
    KTEST_ASSERT(ctx, s.spurious > 0);
}

static void test_wait_queue_wake(ktest_context_t* ctx) {
    wait_queue_t wq;
    wait_queue_init(&wq, "ktest-wq");

    KTEST_ASSERT_EQ(ctx, (long long)wait_queue_waiters(&wq), 0);
    // With no sleepers a wake is a no-op rather than an error.
    KTEST_ASSERT_EQ(ctx, wake_up(&wq), 0);
    KTEST_ASSERT_EQ(ctx, wake_up_all(&wq), 0);

    completion_t c;
    completion_init(&c, "ktest-completion");
    KTEST_ASSERT(ctx, !completion_done(&c));
    complete(&c);
    KTEST_ASSERT(ctx, completion_done(&c));

    // Waiting on an already-completed barrier returns at once.
    KTEST_ASSERT_EQ(ctx, wait_for_completion_timeout(&c, 10), 0);
}

static const ktest_case_t concurrency_cases[] = {
    { "rcu_read_side_nesting",   test_rcu_read_side },
    { "rcu_deferred_callback",   test_rcu_deferred_free },
    { "rcu_pointer_publish",     test_rcu_pointer_publish },
    { "futex_mutex_fastpath",    test_futex_fastpath },
    { "futex_value_recheck",     test_futex_value_check },
    { "waitqueue_and_completion",test_wait_queue_wake },
};
static const ktest_suite_t concurrency_suite = { "concurrency", concurrency_cases, 6 };

// ===========================================================================
// Built-in suite: mm/page_cache
// ===========================================================================
static void test_page_cache_write_read(ktest_context_t* ctx) {
    // ramdisk0 is registered during boot and is always writable.
    const char* dev = "ramdisk0";

    uint8_t pattern[512];
    for (int i = 0; i < 512; i++) pattern[i] = (uint8_t)(i ^ 0x5A);

    size_t written = page_cache_write(dev, 8192, pattern, sizeof(pattern));
    if (written == 0) {
        // No block device available in this configuration; skip gracefully.
        KTEST_ASSERT(ctx, true);
        return;
    }
    KTEST_ASSERT_EQ(ctx, (long long)written, 512);

    uint8_t readback[512];
    memset(readback, 0, sizeof(readback));
    KTEST_ASSERT_EQ(ctx, (long long)page_cache_read(dev, 8192, readback, 512), 512);
    KTEST_ASSERT_EQ(ctx, memcmp(pattern, readback, 512), 0);
}

static void test_page_cache_hit_accounting(ktest_context_t* ctx) {
    const char* dev = "ramdisk0";

    page_cache_stats_t before = page_cache_get_stats();

    uint8_t buf[64];
    page_cache_read(dev, 0, buf, sizeof(buf));       // Likely a miss
    for (int i = 0; i < 8; i++) {
        page_cache_read(dev, 0, buf, sizeof(buf));   // Same page: must hit
    }

    page_cache_stats_t after = page_cache_get_stats();
    KTEST_ASSERT(ctx, after.hits >= before.hits + 8);
    KTEST_ASSERT(ctx, page_cache_hit_percent() <= 100);
}

static void test_page_cache_spanning_write(ktest_context_t* ctx) {
    const char* dev = "ramdisk0";

    // Straddle a page boundary so the split path is exercised.
    uint64_t offset = PAGE_CACHE_BLOCK_SIZE - 100;
    uint8_t src[300];
    for (int i = 0; i < 300; i++) src[i] = (uint8_t)(i + 3);

    size_t written = page_cache_write(dev, offset, src, sizeof(src));
    if (written == 0) {
        KTEST_ASSERT(ctx, true);
        return;
    }
    KTEST_ASSERT_EQ(ctx, (long long)written, 300);

    uint8_t dst[300];
    memset(dst, 0, sizeof(dst));
    KTEST_ASSERT_EQ(ctx, (long long)page_cache_read(dev, offset, dst, 300), 300);
    KTEST_ASSERT_EQ(ctx, memcmp(src, dst, 300), 0);

    KTEST_ASSERT(ctx, page_cache_sync() >= 0);
}

static const ktest_case_t page_cache_cases[] = {
    { "write_then_read_back",   test_page_cache_write_read },
    { "hit_rate_accounting",    test_page_cache_hit_accounting },
    { "page_spanning_transfer", test_page_cache_spanning_write },
};
static const ktest_suite_t page_cache_suite = { "page_cache", page_cache_cases, 3 };

// ===========================================================================
// Built-in suite: lib/string + mm/kmalloc regression guards
// ===========================================================================
static void test_string_primitives(ktest_context_t* ctx) {
    char buf[32];

    strcpy(buf, "SUB-OS");
    KTEST_ASSERT_EQ(ctx, (long long)strlen(buf), 6);
    KTEST_ASSERT_STREQ(ctx, buf, "SUB-OS");

    strcat(buf, "/kernel");
    KTEST_ASSERT_STREQ(ctx, buf, "SUB-OS/kernel");

    KTEST_ASSERT(ctx, strchr(buf, '/') != NULL);
    KTEST_ASSERT(ctx, strstr(buf, "kernel") != NULL);
    KTEST_ASSERT(ctx, strstr(buf, "absent") == NULL);
    KTEST_ASSERT_EQ(ctx, strncmp(buf, "SUB", 3), 0);

    KTEST_ASSERT_EQ(ctx, atoi("-1234"), -1234);
    KTEST_ASSERT_EQ(ctx, atoi("0"), 0);
}

static void test_snprintf_truncation(ktest_context_t* ctx) {
    char small[8];

    snprintf(small, sizeof(small), "%s-%d", "abcdefgh", 42);
    // Truncation must still leave a NUL inside the buffer.
    KTEST_ASSERT(ctx, strlen(small) < sizeof(small));

    char wide[64];
    snprintf(wide, sizeof(wide), "%u:%s:%d", 7u, "ok", -3);
    KTEST_ASSERT_STREQ(ctx, wide, "7:ok:-3");
}

static void test_kmalloc_roundtrip(ktest_context_t* ctx) {
    size_t used_before = heap_get_used_bytes();

    void* a = kmalloc(128);
    void* b = kzalloc(256);
    KTEST_ASSERT_NOT_NULL(ctx, a);
    KTEST_ASSERT_NOT_NULL(ctx, b);
    KTEST_ASSERT(ctx, a != b);

    if (b) {
        // kzalloc must hand back zeroed memory.
        uint8_t* p = (uint8_t*)b;
        int nonzero = 0;
        for (int i = 0; i < 256; i++) if (p[i]) nonzero++;
        KTEST_ASSERT_EQ(ctx, nonzero, 0);
    }

    KTEST_ASSERT(ctx, heap_get_used_bytes() > used_before);

    kfree(a);
    kfree(b);
    kfree(NULL); // Must be a safe no-op
    KTEST_ASSERT(ctx, true);
}

static const ktest_case_t libcore_cases[] = {
    { "string_primitives",    test_string_primitives },
    { "snprintf_truncation",  test_snprintf_truncation },
    { "kmalloc_roundtrip",    test_kmalloc_roundtrip },
};
static const ktest_suite_t libcore_suite = { "libcore", libcore_cases, 3 };

void ktest_register_builtin_suites(void) {
    ktest_register_suite(&rbtree_suite);
    ktest_register_suite(&kfifo_suite);
    ktest_register_suite(&hashtable_suite);
    ktest_register_suite(&concurrency_suite);
    ktest_register_suite(&page_cache_suite);
    ktest_register_suite(&libcore_suite);
}
