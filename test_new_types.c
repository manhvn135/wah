#define WAH_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "wah.h"

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", message); \
        return 1; \
    } else { \
        printf("PASS: %s\n", message); \
    } \
} while(0)

#define CHECK_ERR(err, expected, message) do { \
    if (err != expected) { \
        fprintf(stderr, "FAIL: %s (got %d, expected %d)\n", message, err, expected); \
        return 1; \
    } else { \
        printf("PASS: %s\n", message); \
    } \
} while(0)

#define CHECK_FLOAT(val, exp, epsilon, message) do { \
    if (fabsf(val - exp) > epsilon) { \
        fprintf(stderr, "FAIL: %s (got %f, expected %f)\n", message, val, exp); \
        return 1; \
    } else { \
        printf("PASS: %s\n", message); \
    } \
} while(0)

#define CHECK_DOUBLE(val, exp, epsilon, message) do { \
    if (fabs(val - exp) > epsilon) { \
        fprintf(stderr, "FAIL: %s (got %f, expected %f)\n", message, val, exp); \
        return 1; \
    } else { \
        printf("PASS: %s\n", message); \
    } \
} while(0)


// (module
//   (func (param i64 i64) (result i64)
//     (i64.add (local.get 0) (local.get 1))
//   )
// )
const uint8_t i64_add_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7e, 0x7e, 0x01, 0x7e,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x7c, 0x0b
};

// (module
//   (func (param f32 f32) (result f32)
//     (f32.mul (local.get 0) (local.get 1))
//   )
// )
const uint8_t f32_mul_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7d, 0x7d, 0x01, 0x7d,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x94, 0x0b
};

// (module
//   (func (param f64 f64) (result f64)
//     (f64.sub (local.get 0) (local.get 1))
//   )
// )
const uint8_t f64_sub_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7c, 0x7c, 0x01, 0x7c,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0xa1, 0x0b
};

// (module
//   (func (result i32)
//     (i64.const 9223372036854775807)
//     (i64.const 1)
//     (i64.add)
//     (i64.const 0)
//     (i64.lt_s)
//   )
// )
const uint8_t i64_overflow_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x15, 0x01, 0x13, 0x00, // Corrected: section size 21, body size 19
    0x42, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, // i64.const MAX
    0x42, 0x01, // i64.const 1
    0x7c, // i64.add
    0x42, 0x00, // i64.const 0
    0x53, // i64.lt_s
    0x0b
};

// (module
//   (func (result f64)
//     (f64.const 1.0)
//     (f64.const 0.0)
//     (f64.div)
//   )
// )
const uint8_t f64_div_zero_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7c,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x17, 0x01, 0x15, 0x00, // Corrected: section size 23, body size 21
    0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // f64.const 1.0
    0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // f64.const 0.0
    0xa3, // f64.div
    0x0b
};

// (module
//   (func (result i64)
//     (i64.const -9223372036854775808) // INT64_MIN
//     (i64.const -1)
//     (i64.div_s)
//   )
// )
const uint8_t i64_div_overflow_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7e,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x12, 0x01, 0x10, 0x00, // Corrected: section size 18, body size 16
    0x42, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, // i64.const MIN
    0x42, 0x7f, // i64.const -1
    0x7f, // i64.div_s
    0x0b
};

// (module (func (i64.const 123) (drop)))
const uint8_t i64_const_drop_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic, version
    // Type section
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
    // Function section
    0x03, 0x02, 0x01, 0x00,
    // Code section
    0x0a, 0x07, 0x01, 0x05, 0x00, // Corrected: section size 7, body size 5
    0x42, 0x7b, // i64.const 123
    0x1a, // drop
    0x0b, // end
};


int run_test(const char* test_name, const uint8_t* wasm_binary, size_t binary_size, int (*test_func)(wah_module_t*, wah_exec_context_t*)) {
    printf("\n--- Testing %s ---\n", test_name);
    wah_module_t module;
    wah_exec_context_t ctx; // Declare context
    wah_error_t err = wah_parse_module(wasm_binary, binary_size, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "FAIL: Parsing %s (error: %d)\n", test_name, err);
        return 1;
    }
    printf("PASS: Parsing %s\n", test_name);

    // Create execution context
    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "FAIL: Creating execution context for %s (error: %d)\n", test_name, err);
        wah_free_module(&module);
        return 1;
    }

    int result = test_func(&module, &ctx); // Pass context to test_func
    wah_exec_context_destroy(&ctx); // Destroy context
    wah_free_module(&module);
    return result;
}

int test_i64_add(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    params[0].i64 = 10000000000; // 10^10
    params[1].i64 = 25000000000; // 2.5 * 10^10
    wah_error_t err = wah_call(ctx, module, 0, params, 2, &result);
    CHECK_ERR(err, WAH_OK, "i64_add wah_call");
    CHECK(result.i64 == 35000000000, "i64_add result");
    return 0;
}

int test_f32_mul(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    params[0].f32 = 12.5f;
    params[1].f32 = -4.0f;
    wah_error_t err = wah_call(ctx, module, 0, params, 2, &result);
    CHECK_ERR(err, WAH_OK, "f32_mul wah_call");
    CHECK_FLOAT(result.f32, -50.0f, 1e-6, "f32_mul result");
    return 0;
}

int test_f64_sub(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    params[0].f64 = 3.1415926535;
    params[1].f64 = 0.0000000005;
    wah_error_t err = wah_call(ctx, module, 0, params, 2, &result);
    CHECK_ERR(err, WAH_OK, "f64_sub wah_call");
    CHECK_DOUBLE(result.f64, 3.1415926530, 1e-9, "f64_sub result");
    return 0;
}

int test_i64_overflow(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t result;
    wah_error_t err = wah_call(ctx, module, 0, NULL, 0, &result);
    CHECK_ERR(err, WAH_OK, "i64_overflow wah_call");
    // INT64_MAX + 1 wraps around to INT64_MIN, which is < 0, so comparison should be true (1)
    CHECK(result.i32 == 1, "i64_overflow result");
    return 0;
}

int test_f64_div_zero(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t result;
    wah_error_t err = wah_call(ctx, module, 0, NULL, 0, &result);
    CHECK_ERR(err, WAH_OK, "f64_div_zero wah_call");
    CHECK(isinf(result.f64) && result.f64 > 0, "f64_div_zero result is +inf");
    return 0;
}

int test_i64_div_overflow(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t result;
    wah_error_t err = wah_call(ctx, module, 0, NULL, 0, &result);
    CHECK_ERR(err, WAH_ERROR_TRAP, "i64_div_overflow wah_call traps");
    return 0;
}

int test_i64_const_drop(wah_module_t* module, wah_exec_context_t* ctx) {
    // This test only needs to parse successfully.
    (void)module;
    return 0;
}

int main() {
    int failed = 0;
    failed |= run_test("i64_add", i64_add_wasm, sizeof(i64_add_wasm), test_i64_add);
    failed |= run_test("f32_mul", f32_mul_wasm, sizeof(f32_mul_wasm), test_f32_mul);
    failed |= run_test("f64_sub", f64_sub_wasm, sizeof(f64_sub_wasm), test_f64_sub);
    failed |= run_test("i64_overflow", i64_overflow_wasm, sizeof(i64_overflow_wasm), test_i64_overflow);
    failed |= run_test("f64_div_zero", f64_div_zero_wasm, sizeof(f64_div_zero_wasm), test_f64_div_zero);
    failed |= run_test("i64_div_overflow", i64_div_overflow_wasm, sizeof(i64_div_overflow_wasm), test_i64_div_overflow);
    failed |= run_test("i64_const_drop", i64_const_drop_wasm, sizeof(i64_const_drop_wasm), test_i64_const_drop);

    if (failed) {
        printf("\n--- SOME TESTS FAILED ---\n");
    } else {
        printf("\n--- ALL TESTS PASSED ---\n");
    }

    return failed;
}
