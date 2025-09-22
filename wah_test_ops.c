#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define FLOAT_TOLERANCE 1e-6

// --- Test Helper Functions ---

// Generic test runner
#define DEFINE_RUN_TEST(T, ty, fmt, compare) \
int run_test_##T(const char* test_name, const uint8_t* wasm_bytecode, size_t bytecode_size, \
                 wah_value_t* params, uint32_t param_count, ty expected_result, bool expect_trap) { \
    wah_module_t module; \
    wah_exec_context_t ctx; \
    wah_error_t err; \
    wah_value_t result; \
    int test_status = 0; /* 0 for pass, 1 for fail */ \
    \
    err = wah_parse_module(wasm_bytecode, bytecode_size, &module); \
    if (err != WAH_OK) { \
        printf("  %s: Failed to parse module: %s\n", test_name, wah_strerror(err)); \
        return 1; \
    } \
    err = wah_exec_context_create(&ctx, &module); \
    if (err != WAH_OK) { \
        fprintf(stderr, "  %s: Error creating execution context: %s\n", test_name, wah_strerror(err)); \
        wah_free_module(&module); \
        return 1; \
    } \
    \
    err = wah_call(&ctx, &module, 0, params, param_count, &result); \
    if (expect_trap) { \
        if (err == WAH_ERROR_TRAP) { \
            printf("  %s: PASSED. Expected trap.\n", test_name); \
        } else { \
            printf("  %s: FAILED! Expected trap, but got %s\n", test_name, wah_strerror(err)); \
            test_status = 1; \
        } \
    } else { \
        if (err != WAH_OK) { \
            printf("  %s: Failed to execute: %s\n", test_name, wah_strerror(err)); \
            test_status = 1; \
        } else { \
            if (compare) { \
                printf("  %s: PASSED. Result: " fmt "\n", test_name, result.T); \
            } else { \
                printf("  %s: FAILED! Expected " fmt ", got " fmt "\n", test_name, expected_result, result.T); \
                test_status = 1; \
            } \
        } \
    } \
    \
    wah_exec_context_destroy(&ctx); \
    wah_free_module(&module); \
    return test_status; \
}

#define FLOAT_COMPARE(result_val, expected_val, type) \
    ((isnan(result_val) && isnan(expected_val)) || \
     (isinf(result_val) && isinf(expected_val) && (signbit(result_val) == signbit(expected_val))) || \
     (fabs(result_val - expected_val) <= FLOAT_TOLERANCE))

DEFINE_RUN_TEST(i32, int32_t, "%d", result.i32 == expected_result)
DEFINE_RUN_TEST(i64, int64_t, "%lld", result.i64 == expected_result)
DEFINE_RUN_TEST(f32, float, "%f", FLOAT_COMPARE(result.f32, expected_result, float))
DEFINE_RUN_TEST(f64, double, "%f", FLOAT_COMPARE(result.f64, expected_result, double))

#define run_test_i32(n,b,p,e,t) run_test_i32(n,b,sizeof(b),p,sizeof(p)/sizeof(*p),e,t)
#define run_test_i64(n,b,p,e,t) run_test_i64(n,b,sizeof(b),p,sizeof(p)/sizeof(*p),e,t)
#define run_test_f32(n,b,p,e,t) run_test_f32(n,b,sizeof(b),p,sizeof(p)/sizeof(*p),e,t)
#define run_test_f64(n,b,p,e,t) run_test_f64(n,b,sizeof(b),p,sizeof(p)/sizeof(*p),e,t)

// --- Individual Test Functions ---

// A simple test for bitwise AND: (module (func (param i32 i32) (result i32) (i32.and (local.get 0) (local.get 1))))
const uint8_t and_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section
    0x03, 0x02, 0x01, 0x00,
    // Code section
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x71, 0x0b
};

int test_i32_and() {
    int failures = 0;
    wah_value_t params[2];

    printf("\n=== Testing I32.AND ===\n");
    params[0].i32 = 0xFF; params[1].i32 = 0x0F;
    failures += run_test_i32("I32.AND (0xFF & 0x0F)", and_test_wasm, params, 0x0F, false);

    return failures;
}

// Test for comparison: (module (func (param i32 i32) (result i32) (i32.eq (local.get 0) (local.get 1))))
const uint8_t eq_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section
    0x03, 0x02, 0x01, 0x00,
    // Code section
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x46, 0x0b
};

int test_i32_eq() {
    int failures = 0;
    wah_value_t params[2];

    printf("\n=== Testing I32.EQ ===\n");
    params[0].i32 = 42; params[1].i32 = 42;
    failures += run_test_i32("I32.EQ (42 == 42)", eq_test_wasm, params, 1, false);

    params[0].i32 = 42; params[1].i32 = 24;
    failures += run_test_i32("I32.EQ (42 == 24)", eq_test_wasm, params, 0, false);

    return failures;
}

// Test for i32.popcnt: (module (func (param i32) (result i32) (i32.popcnt (local.get 0))))
const uint8_t i32_popcnt_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 69 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0x69, // i32.popcnt
            0x0b // end
};

int test_i32_popcnt() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.POPCNT ===\n");
    params[0].i32 = 0b10101010; // 0xAA
    failures += run_test_i32("I32.POPCNT (0xAA)", i32_popcnt_test_wasm, params, 4, false);

    return failures;
}

// Test for i64.clz: (module (func (param i64) (result i64) (i64.clz (local.get 0))))
const uint8_t i64_clz_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 79 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0x79, // i64.clz
            0x0b // end
};

int test_i64_clz() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.CLZ ===\n");
    params[0].i64 = 0x00000000000000FFULL; // 56 leading zeros;
    failures += run_test_i64("I64.CLZ (0x00...0FF)", i64_clz_test_wasm, params, 56, false);

    return failures;
}

// Test for i32.rotl: (module (func (param i32 i32) (result i32) (i32.rotl (local.get 0) (local.get 1))))
const uint8_t i32_rotl_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32, i32) -> i32)
    0x01, 0x07, // section id (1), size (7)
        0x01, // count (1 function type)
            0x60, // func type
            0x02, 0x7f, 0x7f, // 2 params (i32, i32)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x09, // section id (10), size (9)
        0x01, // count (1 code body)
            0x07, // code body size (LEB128) - 0 locals + 5 instructions (20 00 20 01 77 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0x20, 0x01, // local.get 1
            0x77, // i32.rotl
            0x0b // end
};

// Test for f64.nearest: (module (func (param f64) (result f64) (f64.nearest (local.get 0))))
const uint8_t f64_nearest_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BC 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0x9E, // f64.nearest
            0x0b // end
};

int test_f64_nearest() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.NEAREST ===\n");
    params[0].f64 = 2.5;
    failures += run_test_f64("F64.NEAREST (2.5)", f64_nearest_test_wasm, params, 2.0, false);

    params[0].f64 = 3.5;
    failures += run_test_f64("F64.NEAREST (3.5)", f64_nearest_test_wasm, params, 4.0, false);

    params[0].f64 = -2.5;
    failures += run_test_f64("F64.NEAREST (-2.5)", f64_nearest_test_wasm, params, -2.0, false);

    params[0].f64 = -3.5;
    failures += run_test_f64("F64.NEAREST (-3.5)", f64_nearest_test_wasm, params, -4.0, false);

    return failures;
}

// Test for f32.min: (module (func (param f32 f32) (result f32) (f32.min (local.get 0) (local.get 1))))
const uint8_t f32_min_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32, f32) -> f32)
    0x01, 0x07, // section id (1), size (7)
        0x01, // count (1 function type)
            0x60, // func type
            0x02, 0x7d, 0x7d, // 2 params (f32, f32)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x09, // section id (10), size (9)
        0x01, // count (1 code body)
            0x07, // code body size (LEB128) - 0 locals + 5 instructions (20 00 20 01 94 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0x20, 0x01, // local.get 1
            0x96, // f32.min
            0x0b // end
};

int test_i32_rotl() {
    int failures = 0;
    wah_value_t params[2];

    printf("\n=== Testing I32.ROTL ===\n");
    params[0].i32 = 0x80000001; // 1000...0001
    params[1].i32 = 1;
    failures += run_test_i32("I32.ROTL (0x80000001, 1)", i32_rotl_test_wasm, params, 0x00000003, false);

    return failures;
}

int test_f32_min() {
    int failures = 0;
    wah_value_t params[2];

    printf("\n=== Testing F32.MIN ===\n");
    params[0].f32 = 10.0f; params[1].f32 = 20.0f;
    failures += run_test_f32("F32.MIN (10.0f, 20.0f)", f32_min_test_wasm, params, 10.0f, false);

    params[0].f32 = 5.0f; params[1].f32 = -5.0f;
    failures += run_test_f32("F32.MIN (5.0f, -5.0f)", f32_min_test_wasm, params, -5.0f, false);

    return failures;
}

// Test for i32.wrap_i64: (module (func (param i64) (result i32) (i32.wrap_i64 (local.get 0))))
const uint8_t i32_wrap_i64_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 A7 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xA7, // i32.wrap_i64
            0x0b // end
};

int test_i32_wrap_i64() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.WRAP_I64 ===\n");
    params[0].i64 = 0x123456789ABCDEF0LL;
    failures += run_test_i32("I32.WRAP_I64 (0x123456789ABCDEF0)", i32_wrap_i64_test_wasm, params, (int32_t)0x9ABCDEF0, false);

    params[0].i64 = 0xFFFFFFFF12345678LL;
    failures += run_test_i32("I32.WRAP_I64 (0xFFFFFFFF12345678)", i32_wrap_i64_test_wasm, params, (int32_t)0x12345678, false);

    return failures;
}

// Test for i32.trunc_f32_s: (module (func (param f32) (result i32) (i32.trunc_f32_s (local.get 0))))
const uint8_t i32_trunc_f32_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7d, // 1 param (f32)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 A8 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xA8, // i32.trunc_f32_s
            0x0b // end
};

int test_i32_trunc_f32_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.TRUNC_F32_S ===\n");
    params[0].f32 = 10.5f;
    failures += run_test_i32("I32.TRUNC_F32_S (10.5f)", i32_trunc_f32_s_test_wasm, params, 10, false);

    params[0].f32 = -10.5f;
    failures += run_test_i32("I32.TRUNC_F32_S (-10.5f)", i32_trunc_f32_s_test_wasm, params, -10, false);

    // Test for trap (NaN)
    params[0].f32 = NAN;
    failures += run_test_i32("I32.TRUNC_F32_S (NaN)", i32_trunc_f32_s_test_wasm, params, 0, true); // Expected result doesn't matter for trap

    // Test for trap (Infinity)
    params[0].f32 = INFINITY;
    failures += run_test_i32("I32.TRUNC_F32_S (Infinity)", i32_trunc_f32_s_test_wasm, params, 0, true);

    // Test for trap (too large)
    params[0].f32 = 2147483648.0f; // INT32_MAX + 1
    failures += run_test_i32("I32.TRUNC_F32_S (too large)", i32_trunc_f32_s_test_wasm, params, 0, true);

    return failures;
}

// Test for i32.trunc_f32_u: (module (func (param f32) (result i32) (i32.trunc_f32_u (local.get 0))))
const uint8_t i32_trunc_f32_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7d, // 1 param (f32)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 A9 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xA9, // i32.trunc_f32_u
            0x0b // end
};

int test_i32_trunc_f32_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.TRUNC_F32_U ===\n");
    params[0].f32 = 10.5f;
    failures += run_test_i32("I32.TRUNC_F32_U (10.5f)", i32_trunc_f32_u_test_wasm, params, 10, false);

    // Test for trap (negative)
    params[0].f32 = -10.5f;
    failures += run_test_i32("I32.TRUNC_F32_U (-10.5f)", i32_trunc_f32_u_test_wasm, params, 0, true);

    // Test for trap (too large)
    params[0].f32 = 4294967296.0f; // UINT32_MAX + 1
    failures += run_test_i32("I32.TRUNC_F32_U (too large)", i32_trunc_f32_u_test_wasm, params, 0, true);

    return failures;
}

// Test for i32.trunc_f64_s: (module (func (param f64) (result i32) (i32.trunc_f64_s (local.get 0))))
const uint8_t i32_trunc_f64_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 AA 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xAA, // i32.trunc_f64_s
            0x0b // end
};

int test_i32_trunc_f64_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.TRUNC_F64_S ===\n");
    params[0].f64 = 10.5;
    failures += run_test_i32("I32.TRUNC_F64_S (10.5)", i32_trunc_f64_s_test_wasm, params, 10, false);

    params[0].f64 = -10.5;
    failures += run_test_i32("I32.TRUNC_F64_S (-10.5)", i32_trunc_f64_s_test_wasm, params, -10, false);

    // Test for trap (NaN)
    params[0].f64 = NAN;
    failures += run_test_i32("I32.TRUNC_F64_S (NaN)", i32_trunc_f64_s_test_wasm, params, 0, true);

    // Test for trap (Infinity)
    params[0].f64 = INFINITY;
    failures += run_test_i32("I32.TRUNC_F64_S (Infinity)", i32_trunc_f64_s_test_wasm, params, 0, true);

    // Test for trap (too large)
    params[0].f64 = 2147483648.0; // INT32_MAX + 1
    failures += run_test_i32("I32.TRUNC_F64_S (too large)", i32_trunc_f64_s_test_wasm, params, 0, true);

    return failures;
}

// Test for i32.trunc_f64_u: (module (func (param f64) (result i32) (i32.trunc_f64_u (local.get 0))))
const uint8_t i32_trunc_f64_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 AB 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xAB, // i32.trunc_f64_u
            0x0b // end
};

int test_i32_trunc_f64_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.TRUNC_F64_U ===\n");
    params[0].f64 = 10.5;
    failures += run_test_i32("I32.TRUNC_F64_U (10.5)", i32_trunc_f64_u_test_wasm, params, 10, false);

    // Test for trap (negative)
    params[0].f64 = -10.5;
    failures += run_test_i32("I32.TRUNC_F64_U (-10.5)", i32_trunc_f64_u_test_wasm, params, 0, true);

    // Test for trap (too large)
    params[0].f64 = 4294967296.0; // UINT32_MAX + 1
    failures += run_test_i32("I32.TRUNC_F64_U (too large)", i32_trunc_f64_u_test_wasm, params, 0, true);

    return failures;
}

// Test for i64.extend_i32_s: (module (func (param i32) (result i64) (i64.extend_i32_s (local.get 0))))
const uint8_t i64_extend_i32_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 AC 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xAC, // i64.extend_i32_s
            0x0b // end
};

int test_i64_extend_i32_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.EXTEND_I32_S ===\n");
    params[0].i32 = 12345;
    failures += run_test_i64("I64.EXTEND_I32_S (12345)", i64_extend_i32_s_test_wasm, params, 12345LL, false);

    params[0].i32 = -12345;
    failures += run_test_i64("I64.EXTEND_I32_S (-12345)", i64_extend_i32_s_test_wasm, params, -12345LL, false);

    params[0].i32 = 0x80000000; // Smallest signed 32-bit integer
    failures += run_test_i64("I64.EXTEND_I32_S (0x80000000)", i64_extend_i32_s_test_wasm, params, (int64_t)0xFFFFFFFF80000000LL, false);

    return failures;
}

// Test for i64.extend_i32_u: (module (func (param i32) (result i64) (i64.extend_i32_u (local.get 0))))
const uint8_t i64_extend_i32_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 AD 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xAD, // i64.extend_i32_u
            0x0b // end
};

int test_i64_extend_i32_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.EXTEND_I32_U ===\n");
    params[0].i32 = 12345;
    failures += run_test_i64("I64.EXTEND_I32_U (12345)", i64_extend_i32_u_test_wasm, params, 12345LL, false);

    params[0].i32 = 0xFFFFFFFF; // Largest unsigned 32-bit integer
    failures += run_test_i64("I64.EXTEND_I32_U (0xFFFFFFFF)", i64_extend_i32_u_test_wasm, params, 0x00000000FFFFFFFFLL, false);

    return failures;
}

// Test for i64.trunc_f32_s: (module (func (param f32) (result i64) (i64.trunc_f32_s (local.get 0))))
const uint8_t i64_trunc_f32_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7d, // 1 param (f32)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 AE 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xAE, // i64.trunc_f32_s
            0x0b // end
};

int test_i64_trunc_f32_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.TRUNC_F32_S ===\n");
    params[0].f32 = 10.5f;
    failures += run_test_i64("I64.TRUNC_F32_S (10.5f)", i64_trunc_f32_s_test_wasm, params, 10LL, false);

    params[0].f32 = -10.5f;
    failures += run_test_i64("I64.TRUNC_F32_S (-10.5f)", i64_trunc_f32_s_test_wasm, params, -10LL, false);

    // Test for trap (NaN)
    params[0].f32 = NAN;
    failures += run_test_i64("I64.TRUNC_F32_S (NaN)", i64_trunc_f32_s_test_wasm, params, 0LL, true); // Expected result doesn't matter for trap

    // Test for trap (Infinity)
    params[0].f32 = INFINITY;
    failures += run_test_i64("I64.TRUNC_F32_S (Infinity)", i64_trunc_f32_s_test_wasm, params, 0LL, true);

    // Test for trap (too large)
    params[0].f32 = 9223372036854775808.0f; // INT64_MAX + 1
    failures += run_test_i64("I64.TRUNC_F32_S (too large)", i64_trunc_f32_s_test_wasm, params, 0LL, true);

    return failures;
}

// Test for i64.trunc_f32_u: (module (func (param f32) (result i64) (i64.trunc_f32_u (local.get 0))))
const uint8_t i64_trunc_f32_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7d, // 1 param (f32)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 AF 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xAF, // i64.trunc_f32_u
            0x0b // end
};

int test_i64_trunc_f32_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.TRUNC_F32_U ===\n");
    params[0].f32 = 10.5f;
    failures += run_test_i64("I64.TRUNC_F32_U (10.5f)", i64_trunc_f32_u_test_wasm, params, 10LL, false);

    // Test for trap (negative)
    params[0].f32 = -10.5f;
    failures += run_test_i64("I64.TRUNC_F32_U (-10.5f)", i64_trunc_f32_u_test_wasm, params, 0LL, true);

    // Test for trap (too large)
    params[0].f32 = 18446744073709551616.0f; // UINT64_MAX + 1
    failures += run_test_i64("I64.TRUNC_F32_U (too large)", i64_trunc_f32_u_test_wasm, params, 0LL, true);

    return failures;
}

// Test for i64.trunc_f64_s: (module (func (param f64) (result i64) (i64.trunc_f64_s (local.get 0))))
const uint8_t i64_trunc_f64_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B0 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB0, // i64.trunc_f64_s
            0x0b // end
};

int test_i64_trunc_f64_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.TRUNC_F64_S ===\n");
    params[0].f64 = 10.5;
    failures += run_test_i64("I64.TRUNC_F64_S (10.5)", i64_trunc_f64_s_test_wasm, params, 10LL, false);

    params[0].f64 = -10.5;
    failures += run_test_i64("I64.TRUNC_F64_S (-10.5)", i64_trunc_f64_s_test_wasm, params, -10LL, false);

    // Test for trap (NaN)
    params[0].f64 = NAN;
    failures += run_test_i64("I64.TRUNC_F64_S (NaN)", i64_trunc_f64_s_test_wasm, params, 0LL, true);

    // Test for trap (Infinity)
    params[0].f64 = INFINITY;
    failures += run_test_i64("I64.TRUNC_F64_S (Infinity)", i64_trunc_f64_s_test_wasm, params, 0LL, true);

    // Test for trap (too large)
    params[0].f64 = 9223372036854775808.0; // INT64_MAX + 1
    failures += run_test_i64("I64.TRUNC_F64_S (too large)", i64_trunc_f64_s_test_wasm, params, 0LL, true);

    return failures;
}

// Test for i64.trunc_f64_u: (module (func (param f64) (result i64) (i64.trunc_f64_u (local.get 0))))
const uint8_t i64_trunc_f64_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B1 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB1, // i64.trunc_f64_u
            0x0b // end
};

int test_i64_trunc_f64_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.TRUNC_F64_U ===\n");
    params[0].f64 = 10.5;
    failures += run_test_i64("I64.TRUNC_F64_U (10.5)", i64_trunc_f64_u_test_wasm, params, 10LL, false);

    // Test for trap (negative)
    params[0].f64 = -10.5;
    failures += run_test_i64("I64.TRUNC_F64_U (-10.5)", i64_trunc_f64_u_test_wasm, params, 0LL, true);

    // Test for trap (too large)
    params[0].f64 = 18446744073709551616.0; // UINT64_MAX + 1
    failures += run_test_i64("I64.TRUNC_F64_U (too large)", i64_trunc_f64_u_test_wasm, params, 0LL, true);

    return failures;
}

// Test for f32.convert_i32_s: (module (func (param i32) (result f32) (f32.convert_i32_s (local.get 0))))
const uint8_t f32_convert_i32_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> f32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B2 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB2, // f32.convert_i32_s
            0x0b // end
};

int test_f32_convert_i32_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F32.CONVERT_I32_S ===\n");
    params[0].i32 = 12345;
    failures += run_test_f32("F32.CONVERT_I32_S (12345)", f32_convert_i32_s_test_wasm, params, 12345.0f, false);

    params[0].i32 = -12345;
    failures += run_test_f32("F32.CONVERT_I32_S (-12345)", f32_convert_i32_s_test_wasm, params, -12345.0f, false);

    return failures;
}

// Test for f32.convert_i32_u: (module (func (param i32) (result f32) (f32.convert_i32_u (local.get 0))))
const uint8_t f32_convert_i32_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> f32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B3 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB3, // f32.convert_i32_u
            0x0b // end
};

int test_f32_convert_i32_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F32.CONVERT_I32_U ===\n");
    params[0].i32 = 12345;
    failures += run_test_f32("F32.CONVERT_I32_U (12345)", f32_convert_i32_u_test_wasm, params, 12345.0f, false);

    params[0].i32 = 0xFFFFFFFF; // UINT32_MAX
    failures += run_test_f32("F32.CONVERT_I32_U (0xFFFFFFFF)", f32_convert_i32_u_test_wasm, params, 4294967295.0f, false);

    return failures;
}

// Test for f32.convert_i64_s: (module (func (param i64) (result f32) (f32.convert_i64_s (local.get 0))))
const uint8_t f32_convert_i64_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> f32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B4 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB4, // f32.convert_i64_s
            0x0b // end
};

int test_f32_convert_i64_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F32.CONVERT_I64_S ===\n");
    params[0].i64 = 123456789012345LL;
    failures += run_test_f32("F32.CONVERT_I64_S (123456789012345)", f32_convert_i64_s_test_wasm, params, 123456788103168.0f, false); // Precision loss expected

    params[0].i64 = -123456789012345LL;
    failures += run_test_f32("F32.CONVERT_I64_S (-123456789012345)", f32_convert_i64_s_test_wasm, params, -123456788103168.0f, false); // Precision loss expected

    return failures;
}

// Test for f32.convert_i64_u: (module (func (param i64) (result f32) (f32.convert_i64_u (local.get 0))))
const uint8_t f32_convert_i64_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> f32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B5 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB5, // f32.convert_i64_u
            0x0b // end
};

int test_f32_convert_i64_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F32.CONVERT_I64_U ===\n");
    params[0].i64 = 123456789012345ULL;
    failures += run_test_f32("F32.CONVERT_I64_U (123456789012345)", f32_convert_i64_u_test_wasm, params, 123456788103168.0f, false); // Precision loss expected

    params[0].i64 = 0xFFFFFFFFFFFFFFFFULL; // UINT64_MAX
    failures += run_test_f32("F32.CONVERT_I64_U (0xFFFFFFFFFFFFFFFF)", f32_convert_i64_u_test_wasm, params, 1.8446744073709552e+19f, false); // Precision loss expected

    return failures;
}

// Test for f32.demote_f64: (module (func (param f64) (result f32) (f32.demote_f64 (local.get 0))))
const uint8_t f32_demote_f64_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> f32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B6 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB6, // f32.demote_f64
            0x0b // end
};

int test_f32_demote_f64() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F32.DEMOTE_F64 ===\n");
    params[0].f64 = 123.456;
    failures += run_test_f32("F32.DEMOTE_F64 (123.456)", f32_demote_f64_test_wasm, params, 123.456f, false);

    params[0].f64 = 1.2345678901234567e+300; // A large double that will become infinity in float
    failures += run_test_f32("F32.DEMOTE_F64 (large double to float)", f32_demote_f64_test_wasm, params, INFINITY, false);

    return failures;
}

// Test for f64.convert_i32_s: (module (func (param i32) (result f64) (f64.convert_i32_s (local.get 0))))
const uint8_t f64_convert_i32_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B7 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB7, // f64.convert_i32_s
            0x0b // end
};

int test_f64_convert_i32_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.CONVERT_I32_S ===\n");
    params[0].i32 = 12345;
    failures += run_test_f64("F64.CONVERT_I32_S (12345)", f64_convert_i32_s_test_wasm, params, 12345.0, false);

    params[0].i32 = -12345;
    failures += run_test_f64("F64.CONVERT_I32_S (-12345)", f64_convert_i32_s_test_wasm, params, -12345.0, false);

    return failures;
}

// Test for f64.convert_i32_u: (module (func (param i32) (result f64) (f64.convert_i32_u (local.get 0))))
const uint8_t f64_convert_i32_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B8 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB8, // f64.convert_i32_u
            0x0b // end
};

int test_f64_convert_i32_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.CONVERT_I32_U ===\n");
    params[0].i32 = 12345;
    failures += run_test_f64("F64.CONVERT_I32_U (12345)", f64_convert_i32_u_test_wasm, params, 12345.0, false);

    params[0].i32 = 0xFFFFFFFF; // UINT32_MAX
    failures += run_test_f64("F64.CONVERT_I32_U (0xFFFFFFFF)", f64_convert_i32_u_test_wasm, params, 4294967295.0, false);

    return failures;
}

// Test for f64.convert_i64_s: (module (func (param i64) (result f64) (f64.convert_i64_s (local.get 0))))
const uint8_t f64_convert_i64_s_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 B9 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xB9, // f64.convert_i64_s
            0x0b // end
};

int test_f64_convert_i64_s() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.CONVERT_I64_S ===\n");
    params[0].i64 = 1234567890123456789LL;
    failures += run_test_f64("F64.CONVERT_I64_S (1234567890123456789)", f64_convert_i64_s_test_wasm, params, 1234567890123456768.0, false); // Precision loss expected

    params[0].i64 = -1234567890123456789LL;
    failures += run_test_f64("F64.CONVERT_I64_S (-1234567890123456789)", f64_convert_i64_s_test_wasm, params, -1234567890123456768.0, false); // Precision loss expected

    return failures;
}

// Test for f64.convert_i64_u: (module (func (param i64) (result f64) (f64.convert_i64_u (local.get 0))))
const uint8_t f64_convert_i64_u_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BA 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xBA, // f64.convert_i64_u
            0x0b // end
};

int test_f64_convert_i64_u() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.CONVERT_I64_U ===\n");
    params[0].i64 = 1234567890123456789ULL;
    failures += run_test_f64("F64.CONVERT_I64_U (1234567890123456789)", f64_convert_i64_u_test_wasm, params, 1234567890123456768.0, false); // Precision loss expected

    params[0].i64 = 0xFFFFFFFFFFFFFFFFULL; // UINT64_MAX
    failures += run_test_f64("F64.CONVERT_I64_U (0xFFFFFFFFFFFFFFFF)", f64_convert_i64_u_test_wasm, params, 1.8446744073709552e+19, false); // Precision loss expected

    return failures;
}

// Test for f64.promote_f32: (module (func (param f32) (result f64) (f64.promote_f32 (local.get 0))))
const uint8_t f64_promote_f32_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7d, // 1 param (f32)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BB 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xBB, // f64.promote_f32
            0x0b // end
};

int test_f64_promote_f32() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.PROMOTE_F32 ===\n");
    params[0].f32 = 123.456f;
    failures += run_test_f64("F64.PROMOTE_F32 (123.456f)", f64_promote_f32_test_wasm, params, (double)123.456f, false);

    return failures;
}

// Test for i32.reinterpret_f32: (module (func (param f32) (result i32) (i32.reinterpret_f32 (local.get 0))))
const uint8_t i32_reinterpret_f32_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f32) -> i32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7d, // 1 param (f32)
            0x01, 0x7f, // 1 result (i32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BC 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xBC, // i32.reinterpret_f32
            0x0b // end
};

int test_i32_reinterpret_f32() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I32.REINTERPRET_F32 ===\n");
    params[0].f32 = 1.0f;
    failures += run_test_i32("I32.REINTERPRET_F32 (1.0f)", i32_reinterpret_f32_test_wasm, params, 0x3F800000, false);

    return failures;
}

// Test for i64.reinterpret_f64: (module (func (param f64) (result i64) (i64.reinterpret_f64 (local.get 0))))
const uint8_t i64_reinterpret_f64_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(f64) -> i64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7c, // 1 param (f64)
            0x01, 0x7e, // 1 result (i64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BD 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xBD, // i64.reinterpret_f64
            0x0b // end
};

int test_i64_reinterpret_f64() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing I64.REINTERPRET_F64 ===\n");
    params[0].f64 = 1.0;
    failures += run_test_i64("I64.REINTERPRET_F64 (1.0)", i64_reinterpret_f64_test_wasm, params, 0x3FF0000000000000ULL, false);

    return failures;
}

// Test for f32.reinterpret_i32: (module (func (param i32) (result f32) (f32.reinterpret_i32 (local.get 0))))
const uint8_t f32_reinterpret_i32_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i32) -> f32)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7f, // 1 param (i32)
            0x01, 0x7d, // 1 result (f32)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BE 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xBE, // f32.reinterpret_i32
            0x0b // end
};

int test_f32_reinterpret_i32() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F32.REINTERPRET_I32 ===\n");
    params[0].i32 = 0x3F800000;
    failures += run_test_f32("F32.REINTERPRET_I32 (0x3F800000)", f32_reinterpret_i32_test_wasm, params, 1.0f, false);

    return failures;
}

// Test for f64.reinterpret_i64: (module (func (param i64) (result f64) (f64.reinterpret_i64 (local.get 0))))
const uint8_t f64_reinterpret_i64_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1 type: func(i64) -> f64)
    0x01, 0x06, // section id (1), size (6)
        0x01, // count (1 function type)
            0x60, // func type
            0x01, 0x7e, // 1 param (i64)
            0x01, 0x7c, // 1 result (f64)

    // Function section (1 function, type index 0)
    0x03, 0x02, // section id (3), size (2)
        0x01, // count (1 function)
            0x00, // function type index (0)

    // Code section (1 code body)
    0x0a, 0x07, // section id (10), size (7)
        0x01, // count (1 code body)
            0x05, // code body size (LEB128) - 0 locals + 4 instructions (20 00 BF 0B)
            0x00, // local count (0)
            0x20, 0x00, // local.get 0
            0xBF, // f64.reinterpret_i64
            0x0b // end
};

int test_f64_reinterpret_i64() {
    int failures = 0;
    wah_value_t params[1];

    printf("\n=== Testing F64.REINTERPRET_I64 ===\n");
    params[0].i64 = 0x3FF0000000000000ULL;
    failures += run_test_f64("F64.REINTERPRET_I64 (0x3FF0000000000000)", f64_reinterpret_i64_test_wasm, params, 1.0, false);

    return failures;
}

int main() {
    int total_failures = 0;

    total_failures += test_i32_and();
    total_failures += test_i32_eq();
    total_failures += test_i32_popcnt();
    total_failures += test_i64_clz();
    total_failures += test_i32_rotl();
    total_failures += test_f64_nearest();
    total_failures += test_f32_min();

    total_failures += test_i32_wrap_i64();
    total_failures += test_i32_trunc_f32_s();
    total_failures += test_i32_trunc_f32_u();
    total_failures += test_i32_trunc_f64_s();
    total_failures += test_i32_trunc_f64_u();

    total_failures += test_i64_extend_i32_s();
    total_failures += test_i64_extend_i32_u();
    total_failures += test_i64_trunc_f32_s();
    total_failures += test_i64_trunc_f32_u();
    total_failures += test_i64_trunc_f64_s();
    total_failures += test_i64_trunc_f64_u();

    total_failures += test_f32_convert_i32_s();
    total_failures += test_f32_convert_i32_u();
    total_failures += test_f32_convert_i64_s();
    total_failures += test_f32_convert_i64_u();

    total_failures += test_f32_demote_f64();

    total_failures += test_f64_convert_i32_s();
    total_failures += test_f64_convert_i32_u();
    total_failures += test_f64_convert_i64_s();
    total_failures += test_f64_convert_i64_u();

    total_failures += test_f64_promote_f32();

    total_failures += test_i32_reinterpret_f32();
    total_failures += test_i64_reinterpret_f64();
    total_failures += test_f32_reinterpret_i32();
    total_failures += test_f64_reinterpret_i64();

    if (total_failures > 0) {
        printf("\nSUMMARY: %d test(s) FAILED!\n", total_failures);
        return 1;
    } else {
        printf("\nSUMMARY: All tests PASSED!\n");
        return 0;
    }
}
