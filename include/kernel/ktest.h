#ifndef _KERNEL_KTEST_H
#define _KERNEL_KTEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// In-kernel self-test harness (KUnit-style). Suites register a list of cases;
// `ktest` from the LazyBox shell runs them and reports pass/fail per assertion.

#define KTEST_MAX_SUITES 16
#define KTEST_MAX_CASES  24

typedef struct ktest_context {
    const char* suite;
    const char* test;
    uint32_t    assertions;
    uint32_t    failures;
    bool        aborted;
} ktest_context_t;

typedef void (*ktest_fn_t)(ktest_context_t* ctx);

typedef struct {
    const char* name;
    ktest_fn_t  func;
} ktest_case_t;

typedef struct {
    const char*        name;
    const ktest_case_t* cases;
    size_t             case_count;
} ktest_suite_t;

typedef struct {
    uint32_t suites_run;
    uint32_t tests_run;
    uint32_t tests_passed;
    uint32_t tests_failed;
    uint32_t assertions;
    uint32_t assertion_failures;
    uint64_t elapsed_ticks;
} ktest_result_t;

void ktest_init(void);
int  ktest_register_suite(const ktest_suite_t* suite);

// Run every suite, or one by name. Returns the number of failed tests.
int  ktest_run_all(ktest_result_t* result_out);
int  ktest_run_suite(const char* name, ktest_result_t* result_out);
void ktest_list_suites(void);

// Assertion primitives used by the macros below.
void ktest_report(ktest_context_t* ctx, bool passed, const char* expr,
                  const char* file, int line, const char* detail);

#define KTEST_ASSERT(ctx, cond) \
    ktest_report((ctx), (cond), #cond, __FILE__, __LINE__, NULL)

#define KTEST_ASSERT_EQ(ctx, a, b) \
    do { \
        long long _a = (long long)(a); \
        long long _b = (long long)(b); \
        ktest_expect_eq((ctx), _a, _b, #a " == " #b, __FILE__, __LINE__); \
    } while (0)

#define KTEST_ASSERT_STREQ(ctx, a, b) \
    ktest_expect_streq((ctx), (a), (b), #a " == " #b, __FILE__, __LINE__)

#define KTEST_ASSERT_NOT_NULL(ctx, p) \
    ktest_report((ctx), (p) != NULL, #p " != NULL", __FILE__, __LINE__, NULL)

void ktest_expect_eq(ktest_context_t* ctx, long long a, long long b,
                     const char* expr, const char* file, int line);
void ktest_expect_streq(ktest_context_t* ctx, const char* a, const char* b,
                        const char* expr, const char* file, int line);

// Built-in suites covering the core data structures and subsystems.
void ktest_register_builtin_suites(void);

#endif // _KERNEL_KTEST_H
