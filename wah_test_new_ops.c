#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <string.h> // For strcmp, if needed
#include <math.h>   // For fabs, if needed

// Helper for floating point comparisons
#define FLOAT_TOLERANCE 1e-6

// --- Test Helper Functions ---

// Generic test runner
#define DEFINE_RUN_TEST(T, ty, fmt, compare) \
int run_test_##T(const char* test_name, const uint8_t* wasm_bytecode, size_t bytecode_size, \
                 wah_value_t* params, uint32_t param_count, ty expected_result) { \
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
    \
    wah_exec_context_destroy(&ctx); \
    wah_free_module(&module); \
    return test_status; \
}

DEFINE_RUN_TEST(i32, int32_t, "%d", result.i32 == expected_result)
DEFINE_RUN_TEST(i64, int64_t, "%lld", result.i64 == expected_result)
DEFINE_RUN_TEST(f32, float, "%f", fabsf(result.f32 - expected_result) <= FLOAT_TOLERANCE)
DEFINE_RUN_TEST(f64, double, "%f", fabs(result.f64 - expected_result) <= FLOAT_TOLERANCE)

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
    failures += run_test_i32("I32.AND (0xFF & 0x0F)", and_test_wasm, sizeof(and_test_wasm), params, 2, 0x0F);
    
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
    failures += run_test_i32("I32.EQ (42 == 42)", eq_test_wasm, sizeof(eq_test_wasm), params, 2, 1);
    
    params[0].i32 = 42; params[1].i32 = 24;
    failures += run_test_i32("I32.EQ (42 == 24)", eq_test_wasm, sizeof(eq_test_wasm), params, 2, 0);

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
    failures += run_test_i32("I32.POPCNT (0xAA)", i32_popcnt_test_wasm, sizeof(i32_popcnt_test_wasm), params, 1, 4);

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
    failures += run_test_i64("I64.CLZ (0x00...0FF)", i64_clz_test_wasm, sizeof(i64_clz_test_wasm), params, 1, 56);

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
    failures += run_test_f64("F64.NEAREST (2.5)", f64_nearest_test_wasm, sizeof(f64_nearest_test_wasm), params, 1, 2.0);

    params[0].f64 = 3.5;
    failures += run_test_f64("F64.NEAREST (3.5)", f64_nearest_test_wasm, sizeof(f64_nearest_test_wasm), params, 1, 4.0);

    params[0].f64 = -2.5;
    failures += run_test_f64("F64.NEAREST (-2.5)", f64_nearest_test_wasm, sizeof(f64_nearest_test_wasm), params, 1, -2.0);

    params[0].f64 = -3.5;
    failures += run_test_f64("F64.NEAREST (-3.5)", f64_nearest_test_wasm, sizeof(f64_nearest_test_wasm), params, 1, -4.0);

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
    failures += run_test_i32("I32.ROTL (0x80000001, 1)", i32_rotl_test_wasm, sizeof(i32_rotl_test_wasm), params, 2, 0x00000003);

    return failures;
}

int test_f32_min() {
    int failures = 0;
    wah_value_t params[2];

    printf("\n=== Testing F32.MIN ===\n");
    params[0].f32 = 10.0f; params[1].f32 = 20.0f;
    failures += run_test_f32("F32.MIN (10.0f, 20.0f)", f32_min_test_wasm, sizeof(f32_min_test_wasm), params, 2, 10.0f);

    params[0].f32 = 5.0f; params[1].f32 = -5.0f;
    failures += run_test_f32("F32.MIN (5.0f, -5.0f)", f32_min_test_wasm, sizeof(f32_min_test_wasm), params, 2, -5.0f);

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

    if (total_failures > 0) {
        printf("\nSUMMARY: %d test(s) FAILED!\n", total_failures);
        return 1;
    } else {
        printf("\nSUMMARY: All tests PASSED!\n");
        return 0;
    }
}

