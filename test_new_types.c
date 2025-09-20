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

// (module
//   (memory (export "mem") 1)
//   (func (export "store_i8")     (param $addr i32) (param $val i32) local.get $addr local.get $val i32.store8   offset=0 align=1)
//   (func (export "load_i32_8s")  (param $addr i32) (result     i32) local.get $addr                i32.load8_s  offset=0 align=1)
//   (func (export "load_i32_8u")  (param $addr i32) (result     i32) local.get $addr                i32.load8_u  offset=0 align=1)
//   (func (export "store_i16")    (param $addr i32) (param $val i32) local.get $addr local.get $val i32.store16  offset=0 align=2)
//   (func (export "load_i32_16s") (param $addr i32) (result     i32) local.get $addr                i32.load16_s offset=0 align=2)
//   (func (export "load_i32_16u") (param $addr i32) (result     i32) local.get $addr                i32.load16_u offset=0 align=2)
//   (func (export "store_i32")    (param $addr i32) (param $val i32) local.get $addr local.get $val i32.store    offset=0 align=4)
//   (func (export "load_i32")     (param $addr i32) (result     i32) local.get $addr                i32.load     offset=0 align=4)
//   (func (export "store_i64")    (param $addr i32) (param $val i64) local.get $addr local.get $val i64.store    offset=0 align=8)
//   (func (export "load_i64")     (param $addr i32) (result     i64) local.get $addr                i64.load     offset=0 align=8)
//   (func (export "store_i64_8")  (param $addr i32) (param $val i64) local.get $addr local.get $val i64.store8   offset=0 align=1)
//   (func (export "load_i64_8s")  (param $addr i32) (result     i64) local.get $addr                i64.load8_s  offset=0 align=1)
//   (func (export "load_i64_8u")  (param $addr i32) (result     i64) local.get $addr                i64.load8_u  offset=0 align=1)
//   (func (export "store_i64_16") (param $addr i32) (param $val i64) local.get $addr local.get $val i64.store16  offset=0 align=2)
//   (func (export "load_i64_16s") (param $addr i32) (result     i64) local.get $addr                i64.load16_s offset=0 align=2)
//   (func (export "load_i64_16u") (param $addr i32) (result     i64) local.get $addr                i64.load16_u offset=0 align=2)
//   (func (export "store_i64_32") (param $addr i32) (param $val i64) local.get $addr local.get $val i64.store32  offset=0 align=4)
//   (func (export "load_i64_32s") (param $addr i32) (result     i64) local.get $addr                i64.load32_s offset=0 align=4)
//   (func (export "load_i64_32u") (param $addr i32) (result     i64) local.get $addr                i64.load32_u offset=0 align=4)
//   (func (export "store_f32")    (param $addr i32) (param $val f32) local.get $addr local.get $val f32.store    offset=0 align=4)
//   (func (export "load_f32")     (param $addr i32) (result     f32) local.get $addr                f32.load     offset=0 align=4)
//   (func (export "store_f64")    (param $addr i32) (param $val f64) local.get $addr local.get $val f64.store    offset=0 align=8)
//   (func (export "load_f64")     (param $addr i32) (result     f64) local.get $addr                f64.load     offset=0 align=8)
// )
const uint8_t memory_access_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x29, 0x08, 0x60, 0x02, 0x7f, 0x7f, 0x00,
    0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x02, 0x7f, 0x7e, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7e, 0x60,
    0x02, 0x7f, 0x7d, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7d, 0x60, 0x02, 0x7f, 0x7c, 0x00, 0x60, 0x01,
    0x7f, 0x01, 0x7c, 0x03, 0x18, 0x17, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x02, 0x03,
    0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07, 0x05, 0x03, 0x01,
    0x00, 0x01, 0x07, 0xb8, 0x02, 0x18, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x08, 0x73, 0x74, 0x6f,
    0x72, 0x65, 0x5f, 0x69, 0x38, 0x00, 0x00, 0x0b, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x33, 0x32,
    0x5f, 0x38, 0x73, 0x00, 0x01, 0x0b, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x33, 0x32, 0x5f, 0x38,
    0x75, 0x00, 0x02, 0x09, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x5f, 0x69, 0x31, 0x36, 0x00, 0x03, 0x0c,
    0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x33, 0x32, 0x5f, 0x31, 0x36, 0x73, 0x00, 0x04, 0x0c, 0x6c,
    0x6f, 0x61, 0x64, 0x5f, 0x69, 0x33, 0x32, 0x5f, 0x31, 0x36, 0x75, 0x00, 0x05, 0x09, 0x73, 0x74,
    0x6f, 0x72, 0x65, 0x5f, 0x69, 0x33, 0x32, 0x00, 0x06, 0x08, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69,
    0x33, 0x32, 0x00, 0x07, 0x09, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x5f, 0x69, 0x36, 0x34, 0x00, 0x08,
    0x08, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x36, 0x34, 0x00, 0x09, 0x0b, 0x73, 0x74, 0x6f, 0x72,
    0x65, 0x5f, 0x69, 0x36, 0x34, 0x5f, 0x38, 0x00, 0x0a, 0x0b, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69,
    0x36, 0x34, 0x5f, 0x38, 0x73, 0x00, 0x0b, 0x0b, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x36, 0x34,
    0x5f, 0x38, 0x75, 0x00, 0x0c, 0x0c, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x5f, 0x69, 0x36, 0x34, 0x5f,
    0x31, 0x36, 0x00, 0x0d, 0x0c, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x36, 0x34, 0x5f, 0x31, 0x36,
    0x73, 0x00, 0x0e, 0x0c, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x36, 0x34, 0x5f, 0x31, 0x36, 0x75,
    0x00, 0x0f, 0x0c, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x5f, 0x69, 0x36, 0x34, 0x5f, 0x33, 0x32, 0x00,
    0x10, 0x0c, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x36, 0x34, 0x5f, 0x33, 0x32, 0x73, 0x00, 0x11,
    0x0c, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x69, 0x36, 0x34, 0x5f, 0x33, 0x32, 0x75, 0x00, 0x12, 0x09,
    0x73, 0x74, 0x6f, 0x72, 0x65, 0x5f, 0x66, 0x33, 0x32, 0x00, 0x13, 0x08, 0x6c, 0x6f, 0x61, 0x64,
    0x5f, 0x66, 0x33, 0x32, 0x00, 0x14, 0x09, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x5f, 0x66, 0x36, 0x34,
    0x00, 0x15, 0x08, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x66, 0x36, 0x34, 0x00, 0x16, 0x0a, 0xcb, 0x01,
    0x17, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x3a, 0x00, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x2c,
    0x00, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x2d, 0x00, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20,
    0x01, 0x3b, 0x01, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x2e, 0x01, 0x00, 0x0b, 0x07, 0x00, 0x20,
    0x00, 0x2f, 0x01, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x36, 0x02, 0x00, 0x0b, 0x07,
    0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x37, 0x03, 0x00,
    0x0b, 0x07, 0x00, 0x20, 0x00, 0x29, 0x03, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x3c,
    0x00, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x30, 0x00, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x31,
    0x00, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x3d, 0x01, 0x00, 0x0b, 0x07, 0x00, 0x20,
    0x00, 0x32, 0x01, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x33, 0x01, 0x00, 0x0b, 0x09, 0x00, 0x20,
    0x00, 0x20, 0x01, 0x3e, 0x02, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x34, 0x02, 0x00, 0x0b, 0x07,
    0x00, 0x20, 0x00, 0x35, 0x02, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x38, 0x02, 0x00,
    0x0b, 0x07, 0x00, 0x20, 0x00, 0x2a, 0x02, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0x39,
    0x03, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x2b, 0x03, 0x00, 0x0b
};

#define FUNC_store_i8 0
#define FUNC_load_i32_8s 1
#define FUNC_load_i32_8u 2
#define FUNC_store_i16 3
#define FUNC_load_i32_16s 4
#define FUNC_load_i32_16u 5
#define FUNC_store_i32 6
#define FUNC_load_i32 7
#define FUNC_store_i64 8
#define FUNC_load_i64 9
#define FUNC_store_i64_8 10
#define FUNC_load_i64_8s 11
#define FUNC_load_i64_8u 12
#define FUNC_store_i64_16 13
#define FUNC_load_i64_16s 14
#define FUNC_load_i64_16u 15
#define FUNC_store_i64_32 16
#define FUNC_load_i64_32s 17
#define FUNC_load_i64_32u 18
#define FUNC_store_f32 19
#define FUNC_load_f32 20
#define FUNC_store_f64 21
#define FUNC_load_f64 22

int run_test(const char* test_name, const uint8_t* wasm_binary, size_t binary_size, int (*test_func)(wah_module_t*, wah_exec_context_t*)) {
    printf("\n--- Testing %s ---\n", test_name);
    wah_module_t module;
    wah_exec_context_t ctx; // Declare context
    wah_error_t err = wah_parse_module(wasm_binary, binary_size, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "FAIL: Parsing %s (error: %s)\n", test_name, wah_strerror(err));
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
    params[0].i64 = 10000000000LL; // 10^10
    params[1].i64 = 25000000000LL; // 2.5 * 10^10
    wah_error_t err = wah_call(ctx, module, 0, params, 2, &result);
    CHECK_ERR(err, WAH_OK, "i64_add wah_call");
    CHECK(result.i64 == 35000000000LL, "i64_add result");
    return 0;
}

int test_i32_store_unaligned(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;
    int failed_checks = 0;

    // Initialize memory to zeros
    for (int i = 0; i < 8; ++i) {
        params[0].i32 = i;
        params[1].i32 = 0;
        err = wah_call(ctx, module, FUNC_store_i8, params, 2, NULL);
        CHECK(err == WAH_OK, "store_i8 0");
    }

    // Store a value at an unaligned address (e.g., address 1)
    params[0].i32 = 1; // Unaligned address
    params[1].i32 = 0xAABBCCDD; // Value
    err = wah_call(ctx, module, FUNC_store_i32, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i32 0xAABBCCDD at unaligned address 1");

    // Verify memory content by loading bytes
    // Expected: memory[0]=00, memory[1]=DD, memory[2]=CC, memory[3]=BB, memory[4]=AA, memory[5]=00
    params[0].i32 = 0;
    err = wah_call(ctx, module, FUNC_load_i32_8u, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32_8u from address 0");
    CHECK(result.i32 == 0x00, "memory[0] is 0x00");

    params[0].i32 = 1;
    err = wah_call(ctx, module, FUNC_load_i32_8u, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32_8u from address 1");
    CHECK(result.i32 == 0xDD, "memory[1] is 0xDD");

    params[0].i32 = 2;
    err = wah_call(ctx, module, FUNC_load_i32_8u, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32_8u from address 2");
    CHECK(result.i32 == 0xCC, "memory[2] is 0xCC");

    params[0].i32 = 3;
    err = wah_call(ctx, module, FUNC_load_i32_8u, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32_8u from address 3");
    CHECK(result.i32 == 0xBB, "memory[3] is 0xBB");

    params[0].i32 = 4;
    err = wah_call(ctx, module, FUNC_load_i32_8u, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32_8u from address 4");
    CHECK(result.i32 == 0xAA, "memory[4] is 0xAA");

    params[0].i32 = 5;
    err = wah_call(ctx, module, FUNC_load_i32_8u, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32_8u from address 5");
    CHECK(result.i32 == 0x00, "memory[5] is 0x00");

    return failed_checks;
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

int test_i32_load8_s_sign_extension(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;

    // Test 1: Store -1 (0xFF) and load with sign extension
    params[0].i32 = 0; // address
    params[1].i32 = -1; // value (0xFF as byte)
    err = wah_call(ctx, module, FUNC_store_i8, params, 2, NULL);
    CHECK_ERR(err, WAH_OK, "store_i8 -1");

    params[0].i32 = 0; // address
    err = wah_call(ctx, module, FUNC_load_i32_8s, params, 1, &result);
    CHECK_ERR(err, WAH_OK, "load_i32_8s -1");
    CHECK(result.i32 == -1, "i32.load8_s -1 result");

    // Test 2: Store 0x7F (127) and load with sign extension
    params[0].i32 = 1; // address
    params[1].i32 = 127; // value (0x7F as byte)
    err = wah_call(ctx, module, FUNC_store_i8, params, 2, NULL);
    CHECK_ERR(err, WAH_OK, "store_i8 127");

    params[0].i32 = 1; // address
    err = wah_call(ctx, module, FUNC_load_i32_8s, params, 1, &result);
    CHECK_ERR(err, WAH_OK, "load_i32_8s 127");
    CHECK(result.i32 == 127, "i32.load8_s 127 result");

    // Test 3: Store 0x80 (-128) and load with sign extension
    params[0].i32 = 2; // address
    params[1].i32 = -128; // value (0x80 as byte)
    err = wah_call(ctx, module, FUNC_store_i8, params, 2, NULL);
    CHECK_ERR(err, WAH_OK, "store_i8 -128");

    params[0].i32 = 2; // address
    err = wah_call(ctx, module, FUNC_load_i32_8s, params, 1, &result);
    CHECK_ERR(err, WAH_OK, "load_i32_8s -128");
    CHECK(result.i32 == -128, "i32.load8_s -128 result");

    return 0;
}

int test_i64_load8_s_sign_extension(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;
    int failed_checks = 0;

    // Test case 1: -1 (0xFF) should sign extend to -1 (0xFFFFFFFFFFFFFFFF)
    params[0].i32 = 0; // Address
    params[1].i64 = -1; // Value to store (will be truncated to 0xFF)
    err = wah_call(ctx, module, FUNC_store_i64_8, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_8 -1");

    err = wah_call(ctx, module, FUNC_load_i64_8s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_8s -1");
    CHECK(result.i64 == -1, "i64.load8_s -1 result");

    // Test case 2: 127 (0x7F) should not sign extend
    params[0].i32 = 0; // Address
    params[1].i64 = 127; // Value to store
    err = wah_call(ctx, module, FUNC_store_i64_8, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_8 127");

    err = wah_call(ctx, module, FUNC_load_i64_8s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_8s 127");
    CHECK(result.i64 == 127, "i64.load8_s 127 result");

    // Test case 3: -128 (0x80) should sign extend to -128
    params[0].i32 = 0; // Address
    params[1].i64 = -128; // Value to store
    err = wah_call(ctx, module, FUNC_store_i64_8, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_8 -128");

    err = wah_call(ctx, module, FUNC_load_i64_8s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_8s -128");
    CHECK(result.i64 == -128, "i64.load8_s -128 result");

    return failed_checks;
}

int test_i32_load16_s_sign_extension(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;

    // Test 1: Store -1 (0xFFFF) and load with sign extension
    params[0].i32 = 0; // address
    params[1].i32 = -1; // value (0xFFFF as 2 bytes)
    err = wah_call(ctx, module, FUNC_store_i16, params, 2, NULL);
    CHECK_ERR(err, WAH_OK, "store_i16 -1");

    params[0].i32 = 0; // address
    err = wah_call(ctx, module, FUNC_load_i32_16s, params, 1, &result);
    CHECK_ERR(err, WAH_OK, "load_i32_16s -1");
    CHECK(result.i32 == -1, "i32.load16_s -1 result");

    // Test 2: Store 0x7FFF (32767) and load with sign extension
    params[0].i32 = 2; // address
    params[1].i32 = 32767; // value (0x7FFF as 2 bytes)
    err = wah_call(ctx, module, FUNC_store_i16, params, 2, NULL);
    CHECK_ERR(err, WAH_OK, "store_i16 32767");

    params[0].i32 = 2; // address
    err = wah_call(ctx, module, FUNC_load_i32_16s, params, 1, &result);
    CHECK_ERR(err, WAH_OK, "load_i32_16s 32767");
    CHECK(result.i32 == 32767, "i32.load16_s 32767 result");

    // Test 3: Store 0x8000 (-32768) and load with sign extension
    params[0].i32 = 4; // address
    params[1].i32 = -32768; // value (0x8000 as 2 bytes)
    err = wah_call(ctx, module, FUNC_store_i16, params, 2, NULL);
    CHECK_ERR(err, WAH_OK, "store_i16 -32768");

    params[0].i32 = 4; // address
    err = wah_call(ctx, module, FUNC_load_i32_16s, params, 1, &result);
    CHECK_ERR(err, WAH_OK, "load_i32_16s -32768");
    CHECK(result.i32 == -32768, "i32.load16_s -32768 result");

    return 0;
}

int test_i64_load16_s_sign_extension(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;
    int failed_checks = 0;

    // Test case 1: -1 (0xFFFF) should sign extend to -1 (0xFFFFFFFFFFFFFFFF)
    params[0].i32 = 0; // Address
    params[1].i64 = -1; // Value to store (will be truncated to 0xFFFF)
    err = wah_call(ctx, module, FUNC_store_i64_16, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_16 -1");

    err = wah_call(ctx, module, FUNC_load_i64_16s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_16s -1");
    CHECK(result.i64 == -1, "i64.load16_s -1 result");

    // Test case 2: 32767 (0x7FFF) should not sign extend
    params[0].i32 = 0; // Address
    params[1].i64 = 32767; // Value to store
    err = wah_call(ctx, module, FUNC_store_i64_16, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_16 32767");

    err = wah_call(ctx, module, FUNC_load_i64_16s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_16s 32767");
    CHECK(result.i64 == 32767, "i64.load16_s 32767 result");

    // Test case 3: -32768 (0x8000) should sign extend to -32768
    params[0].i32 = 0; // Address
    params[1].i64 = -32768; // Value to store
    err = wah_call(ctx, module, FUNC_store_i64_16, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_16 -32768");

    err = wah_call(ctx, module, FUNC_load_i64_16s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_16s -32768");
    CHECK(result.i64 == -32768, "i64.load16_s -32768 result");

    return failed_checks;
}

int test_i64_load32_s_sign_extension(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;
    int failed_checks = 0;

    // Test case 1: -1 (0xFFFFFFFF) should sign extend to -1 (0xFFFFFFFFFFFFFFFF)
    params[0].i32 = 0; // Address
    params[1].i64 = -1; // Value to store (will be truncated to 0xFFFFFFFF)
    err = wah_call(ctx, module, FUNC_store_i64_32, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_32 -1");

    err = wah_call(ctx, module, FUNC_load_i64_32s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_32s -1");
    CHECK(result.i64 == -1, "i64.load32_s -1 result");

    // Test case 2: 2147483647 (0x7FFFFFFF) should not sign extend
    params[0].i32 = 0; // Address
    params[1].i64 = 2147483647LL; // Value to store
    err = wah_call(ctx, module, FUNC_store_i64_32, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_32 2147483647");

    err = wah_call(ctx, module, FUNC_load_i64_32s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_32s 2147483647");
    CHECK(result.i64 == 2147483647LL, "i64.load32_s 2147483647 result");

    // Test case 3: -2147483648 (0x80000000) should sign extend to -2147483648
    params[0].i32 = 0; // Address
    params[1].i64 = -2147483648LL; // Value to store
    err = wah_call(ctx, module, FUNC_store_i64_32, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i64_32 -2147483648");

    err = wah_call(ctx, module, FUNC_load_i64_32s, params, 1, &result);
    CHECK(err == WAH_OK, "load_i64_32s -2147483648");
    CHECK(result.i64 == -2147483648LL, "i64.load32_s -2147483648 result");

    return failed_checks;
}

int test_i32_load_unaligned(wah_module_t* module, wah_exec_context_t* ctx) {
    wah_value_t params[2];
    wah_value_t result;
    wah_error_t err;
    int failed_checks = 0;

    // Store a value at an aligned address (e.g., address 0)
    params[0].i32 = 0; // Address
    params[1].i32 = 0x12345678; // Value
    err = wah_call(ctx, module, FUNC_store_i32, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i32 0x12345678 at address 0");

    // Load from an unaligned address (e.g., address 1)
    params[0].i32 = 1; // Unaligned address
    err = wah_call(ctx, module, FUNC_load_i32, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32 from unaligned address 1");
    // Expected value: 0xXX123456 (assuming little-endian and memory[0]=78, memory[1]=56, memory[2]=34, memory[3]=12)
    // If we load from address 1, we get memory[1], memory[2], memory[3], memory[4]
    // Since memory[4] is 0, the value should be 0x00123456
    CHECK(result.i32 == 0x00123456, "i32.load from unaligned address 1 result");

    // Store another value at an aligned address (e.g., address 4)
    params[0].i32 = 4; // Address
    params[1].i32 = 0xAABBCCDD; // Value
    err = wah_call(ctx, module, FUNC_store_i32, params, 2, NULL);
    CHECK(err == WAH_OK, "store_i32 0xAABBCCDD at address 4");

    // Load from an unaligned address (e.g., address 2)
    params[0].i32 = 2; // Unaligned address
    err = wah_call(ctx, module, FUNC_load_i32, params, 1, &result);
    CHECK(err == WAH_OK, "load_i32 from unaligned address 2");
    // Expected value: 0xCCDD1234 (assuming little-endian and memory[0]=78, memory[1]=56, memory[2]=34, memory[3]=12, memory[4]=DD, memory[5]=CC, memory[6]=BB, memory[7]=AA)
    // If we load from address 2, we get memory[2], memory[3], memory[4], memory[5]
    // So, 0xCCDD1234
    CHECK(result.i32 == 0xCCDD1234, "i32.load from unaligned address 2 result");

    return failed_checks;
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
    failed |= run_test("i32_load8_s_sign_extension", memory_access_wasm, sizeof(memory_access_wasm), test_i32_load8_s_sign_extension);
    failed |= run_test("i32_load16_s_sign_extension", memory_access_wasm, sizeof(memory_access_wasm), test_i32_load16_s_sign_extension);
    failed |= run_test("i64_load8_s_sign_extension", memory_access_wasm, sizeof(memory_access_wasm), test_i64_load8_s_sign_extension);
    failed |= run_test("i64_load16_s_sign_extension", memory_access_wasm, sizeof(memory_access_wasm), test_i64_load16_s_sign_extension);
    failed |= run_test("i64_load32_s_sign_extension", memory_access_wasm, sizeof(memory_access_wasm), test_i64_load32_s_sign_extension);
    failed |= run_test("i32_load_unaligned", memory_access_wasm, sizeof(memory_access_wasm), test_i32_load_unaligned);
    failed |= run_test("i32_store_unaligned", memory_access_wasm, sizeof(memory_access_wasm), test_i32_store_unaligned);

    if (failed) {
        printf("\n--- SOME TESTS FAILED ---\n");
    } else {
        printf("\n--- ALL TESTS PASSED ---\n");
    }

    return failed;
}
