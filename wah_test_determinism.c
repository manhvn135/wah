#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <string.h>
#include <math.h> // For isnan, though we'll use bit patterns for comparison
#include <stdlib.h> // For malloc, free
#include <inttypes.h>

// Helper to convert float to uint32_t bit pattern
static uint32_t float_to_bits(float f) {
    union { uint32_t i; float f; } u = { .f = f };
    return u.i;
}

// Helper to convert double to uint64_t bit pattern
static uint64_t double_to_bits(double d) {
    union { uint64_t i; double d; } u = { .d = d };
    return u.i;
}

// WebAssembly canonical NaN bit patterns (from wah.h)
#define TEST_WASM_F32_CANONICAL_NAN_BITS 0x7fc00000U
#define TEST_WASM_F64_CANONICAL_NAN_BITS 0x7ff8000000000000ULL

static const uint64_t NON_CANONICAL_F64_NAN_BITS_1 = 0x7ff0000000000001ULL;
static const uint64_t NON_CANONICAL_F64_NAN_BITS_2 = 0x7ff0000000000002ULL;

// A non-canonical NaN for testing (e.g., quiet NaN with some payload bits)
// Sign: 0, Exponent: all 1s, Mantissa: 010...0 (bit 22 is 0, bit 21 is 1)
// Canonical would be 0x7fc00000 (bit 22 is 1, rest 0)
static const uint32_t NON_CANONICAL_F32_NAN_BITS = 0x7fa00000U; // Example: qNaN with payload
static const union { uint32_t i; float f; } non_canonical_f32_nan_union = { .i = NON_CANONICAL_F32_NAN_BITS };
#define NON_CANONICAL_F32_NAN non_canonical_f32_nan_union.f

// Test WASM binary for f32.store and f32.const
// (module
//   (memory (export "mem") 1)
//   (func (export "test_store") (param f32) (result f32)
//     local.get 0
//     i32.const 0
//     f32.store
//     i32.const 0
//     f32.load
//   )
//   (func (export "test_const") (result f32)
//     f32.const <NON_CANONICAL_F32_NAN>
//   )
// )
// This is a hand-assembled WASM binary.
// Magic: 0x6d736100 (WASM_MAGIC_NUMBER)
// Version: 0x01000000 (WASM_VERSION)
static const uint8_t test_wasm_binary[] = {
    0x00, 0x61, 0x73, 0x6d, // Magic
    0x01, 0x00, 0x00, 0x00, // Version

    // Type Section (1)
    0x01, // Section ID
    0x30, // Section Size (48 bytes)
    0x09, // Number of types
        // Type 0: (func (param f32) (result f32))
        0x60, 0x01, 0x7d, 0x01, 0x7d,
        // Type 1: (func (result f32))
        0x60, 0x00, 0x01, 0x7d,
        // Type 2: (func (param i32) (result i32))
        0x60, 0x01, 0x7f, 0x01, 0x7f,
        // Type 3: (func (param i64 i64) (result i64))
        0x60, 0x02, 0x7e, 0x7e, 0x01, 0x7e,
        // Type 4: (func (param f32 f32) (result f32))
        0x60, 0x02, 0x7d, 0x7d, 0x01, 0x7d,
        // Type 5: (func (param f32) (result f32)) - same as Type 0
        0x60, 0x01, 0x7d, 0x01, 0x7d,
        // Type 6: (func (param f64 f64) (result f64))
        0x60, 0x02, 0x7c, 0x7c, 0x01, 0x7c,
        // Type 7: (func (param f32) (result f64))
        0x60, 0x01, 0x7d, 0x01, 0x7c,
        // Type 8: (func (param f64) (result f32))
        0x60, 0x01, 0x7c, 0x01, 0x7d,

    // Function Section (3)
    0x03, // Section ID
    0x0a, // Section Size (10 bytes) - UPDATED
    0x09, // Number of functions
    0x00, // Function 0 (test_store) uses type 0
    0x01, // Function 1 (test_const) uses type 1
    0x02, // Function 2 (test_f32_reinterpret_nan) uses type 2
    0x03, // Function 3 (test_f64_add_nan) uses type 3
    0x04, // Function 4 (test_f32_add_nan) uses type 4
    0x05, // Function 5 (test_f32_sqrt_nan) uses type 5
    0x06, // Function 6 (test_f64_min_nan) uses type 6
    0x07, // Function 7 (test_f64_promote_f32_nan_canonicalization) uses type 7
    0x08, // Function 8 (test_f32_demote_f64_nan_canonicalization) uses type 8

    // Memory Section (5)
    0x05, // Section ID
    0x03, // Section Size (3 bytes)
    0x01, // Number of memories
    0x00, // Flags (min only)
    0x01, // Min pages (1)

    // Export Section (7)
    0x07, // Section ID
    0xe0, 0x01, // Section Size (224 bytes)
    0x0a, // Number of exports
        // Export 0: "mem" (memory 0)
        0x03, 'm', 'e', 'm', 0x02, 0x00,
        // Export 1: "test_store" (func 0)
        0x0a, 't', 'e', 's', 't', '_', 's', 't', 'o', 'r', 'e', 0x00, 0x00,
        // Export 2: "test_const" (func 1)
        0x0a, 't', 'e', 's', 't', '_', 'c', 'o', 'n', 's', 't', 0x00, 0x01,
        // Export 3: "test_f32_reinterpret_nan" (func 2)
        0x18, 't', 'e', 's', 't', '_', 'f', '3', '2', '_', 'r', 'e', 'i', 'n', 't', 'e', 'r', 'p', 'r', 'e', 't', '_', 'n', 'a', 'n', 0x00, 0x02,
        // Export 4: "test_f64_add_nan" (func 3)
        0x10, 't', 'e', 's', 't', '_', 'f', '6', '4', '_', 'a', 'd', 'd', '_', 'n', 'a', 'n', 0x00, 0x03,
        // Export 5: "test_f32_add_nan" (func 4)
        0x10, 't', 'e', 's', 't', '_', 'f', '3', '2', '_', 'a', 'd', 'd', '_', 'n', 'a', 'n', 0x00, 0x04,
        // Export 6: "test_f32_sqrt_nan" (func 5)
        0x11, 't', 'e', 's', 't', '_', 'f', '3', '2', '_', 's', 'q', 'r', 't', '_', 'n', 'a', 'n', 0x00, 0x05,
        // Export 7: "test_f64_min_nan" (func 6)
        0x10, 't', 'e', 's', 't', '_', 'f', '6', '4', '_', 'm', 'i', 'n', '_', 'n', 'a', 'n', 0x00, 0x06,
        // Export 8: "test_f64_promote_f32_nan_canonicalization" (func 7)
        0x29, 't', 'e', 's', 't', '_', 'f', '6', '4', '_', 'p', 'r', 'o', 'm', 'o', 't', 'e', '_', 'f', '3', '2', '_', 'n', 'a', 'n', '_', 'c', 'a', 'n', 'o', 'n', 'i', 'c', 'a', 'l', 'i', 'z', 'a', 't', 'i', 'o', 'n', 0x00, 0x07,
        // Export 9: "test_f32_demote_f64_nan_canonicalization" (func 8)
        0x28, 't', 'e', 's', 't', '_', 'f', '3', '2', '_', 'd', 'e', 'm', 'o', 't', 'e', '_', 'f', '6', '4', '_', 'n', 'a', 'n', '_', 'c', 'a', 'n', 'o', 'n', 'i', 'c', 'a', 'l', 'i', 'z', 'a', 't', 'i', 'o', 'n', 0x00, 0x08,

    // Code Section (10)
    0x0A, // Section ID
    0x4c, // Section Size (76 bytes)
    0x09, // Number of code bodies
        // Code Body 0: test_store
        0x0E, 0x00, 0x41, 0x00, 0x20, 0x00, 0x38, 0x00, 0x00, 0x41, 0x00, 0x2a, 0x00, 0x00, 0x0b,
        // Code Body 1: test_const
        0x07, 0x00, 0x43,
        (NON_CANONICAL_F32_NAN_BITS >> 0) & 0xFF,
        (NON_CANONICAL_F32_NAN_BITS >> 8) & 0xFF,
        (NON_CANONICAL_F32_NAN_BITS >> 16) & 0xFF,
        (NON_CANONICAL_F32_NAN_BITS >> 24) & 0xFF,
        0x0b,
        // Code Body 2: test_f32_reinterpret_nan
        0x06, // Body Size (6 bytes)
        0x00, // Local count (0)
        0x20, 0x00, // local.get 0
        0xbe, // f32.reinterpret_i32
        0xbc, // i32.reinterpret_f32
        0x0b, // end
        // Code Body 3: test_f64_add_nan
        0x0a, // Body Size (10 bytes)
        0x00, // Local count (0)
        0x20, 0x00, // local.get 0
        0xbf, // f64.reinterpret_i64
        0x20, 0x01, // local.get 1
        0xbf, // f64.reinterpret_i64
        0xa0, // f64.add
        0xbd, // i64.reinterpret_f64
        0x0b, // end
        // Code Body 4: test_f32_add_nan
        0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x92, 0x0b,
        // Code Body 5: test_f32_sqrt_nan
        0x05, 0x00, 0x20, 0x00, 0x91, 0x0b,
        // Code Body 6: test_f64_min_nan
        0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0xa4, 0x0b,
        // Code Body 7: test_f64_promote_f32_nan_canonicalization
        0x05, 0x00, 0x20, 0x00, 0xbb, 0x0b,
        // Code Body 8: test_f32_demote_f64_nan_canonicalization
        0x05, 0x00, 0x20, 0x00, 0xb6, 0x0b,
};

static wah_error_t test_f32_store_canonicalization() {
    printf("Testing f32.store canonicalization...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing module: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t param;
    param.f32 = NON_CANONICAL_F32_NAN;
    err = wah_call(&exec_ctx, &module, 0, &param, 1, &result); // Call "test_store" (func_idx 0)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_store: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    uint32_t result_bits = float_to_bits(result.f32);
    if (result_bits == NON_CANONICAL_F32_NAN_BITS) {
        printf("f32.store test PASSED: Stored non-canonical NaN was preserved.\n");
    } else {
        printf("f32.store test FAILED: Stored non-canonical NaN was NOT preserved.\n");
        printf("  Expected non-canonical bits: 0x%08X\n", NON_CANONICAL_F32_NAN_BITS);
        printf("  Actual result bits:        0x%08X\n", result_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

static wah_error_t test_f32_const_canonicalization() {
    printf("\nTesting f32.const canonicalization...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for test_const: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for test_const: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    err = wah_call(&exec_ctx, &module, 1, NULL, 0, &result); // Call "test_const" (func_idx 1)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_const: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    uint32_t result_bits = float_to_bits(result.f32);
    if (result_bits == NON_CANONICAL_F32_NAN_BITS) {
        printf("f32.const test PASSED: Constant non-canonical NaN was preserved.\n");
    } else {
        printf("f32.const test FAILED: Constant non-canonical NaN was NOT preserved.\n");
        printf("  Expected non-canonical bits: 0x%08X\n", NON_CANONICAL_F32_NAN_BITS);
        printf("  Actual result bits:        0x%08X\n", result_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

static wah_error_t test_f32_reinterpret_nan_canonicalization() {
    printf("\nTesting f32.reinterpret_i32 and i32.reinterpret_f32 canonicalization...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for f32_reinterpret_nan: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for f32_reinterpret_nan: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t param;
    param.i32 = NON_CANONICAL_F32_NAN_BITS; // Pass non-canonical NaN bit pattern as i32
    err = wah_call(&exec_ctx, &module, 2, &param, 1, &result); // Call "test_f32_reinterpret_nan" (func_idx 2)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f32_reinterpret_nan: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    uint32_t result_i32_bits = (uint32_t)result.i32;
    if (result_i32_bits == NON_CANONICAL_F32_NAN_BITS) {
        printf("f32.reinterpret_i32/i32.reinterpret_f32 test PASSED: Reinterpreted non-canonical NaN was preserved with the absence of any interleaving operations.\n");
    } else {
        printf("f32.reinterpret_i32/i32.reinterpret_f32 test FAILED: Reinterpreted non-canonical NaN was NOT preserved.\n");
        printf("  Expected non-canonical bits: 0x%08X\n", NON_CANONICAL_F32_NAN_BITS);
        printf("  Actual result bits:        0x%08X\n", result_i32_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

static wah_error_t test_f64_add_nan_canonicalization() {
    printf("\nTesting f64.add canonicalization with non-canonical NaNs...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for f64_add_nan: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for f64_add_nan: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t params_f64[2];
    params_f64[0].i64 = (int64_t)NON_CANONICAL_F64_NAN_BITS_1;
    params_f64[1].i64 = (int64_t)NON_CANONICAL_F64_NAN_BITS_2;
    err = wah_call(&exec_ctx, &module, 3, params_f64, 2, &result); // Call "test_f64_add_nan" (func_idx 3)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f64_add_nan: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    uint64_t result_i64_bits = (uint64_t)result.i64;
    if (result_i64_bits == TEST_WASM_F64_CANONICAL_NAN_BITS) {
        printf("f64.add test PASSED: Adding non-canonical NaNs resulted in canonical NaN.\n");
    } else {
        printf("f64.add test FAILED: Adding non-canonical NaNs did NOT result in canonical NaN.\n");
        printf("  Expected canonical bits: 0x%llX\n", TEST_WASM_F64_CANONICAL_NAN_BITS);
        printf("  Actual result bits:    0x%" PRIx64 "\n", result_i64_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

static wah_error_t test_f32_add_nan_canonicalization() {
    printf("\nTesting f32.add canonicalization with non-canonical NaNs...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for f32_add_nan: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for f32_add_nan: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t params_f32[2];
    params_f32[0].f32 = NON_CANONICAL_F32_NAN;
    params_f32[1].f32 = NON_CANONICAL_F32_NAN;
    err = wah_call(&exec_ctx, &module, 4, params_f32, 2, &result); // Call "test_f32_add_nan" (func_idx 4)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f32_add_nan: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    uint32_t result_bits = float_to_bits(result.f32);
    if (result_bits == TEST_WASM_F32_CANONICAL_NAN_BITS) {
        printf("f32.add test PASSED: Adding non-canonical NaNs resulted in canonical NaN.\n");
    } else {
        printf("f32.add test FAILED: Adding non-canonical NaNs did NOT result in canonical NaN.\n");
        printf("  Expected canonical bits: 0x%08X\n", TEST_WASM_F32_CANONICAL_NAN_BITS);
        printf("  Actual result bits:    0x%08X\n", result_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

static wah_error_t test_f32_sqrt_nan_canonicalization() {
    printf("\nTesting f32.sqrt canonicalization with non-canonical NaN...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for f32_sqrt_nan: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for f32_sqrt_nan: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t param;
    param.f32 = NON_CANONICAL_F32_NAN;
    err = wah_call(&exec_ctx, &module, 5, &param, 1, &result); // Call "test_f32_sqrt_nan" (func_idx 5)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f32_sqrt_nan: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    uint32_t result_bits = float_to_bits(result.f32);
    if (result_bits == TEST_WASM_F32_CANONICAL_NAN_BITS) {
        printf("f32.sqrt test PASSED: Sqrt of non-canonical NaN resulted in canonical NaN.\n");
    } else {
        printf("f32.sqrt test FAILED: Sqrt of non-canonical NaN did NOT result in canonical NaN.\n");
        printf("  Expected canonical bits: 0x%08X\n", TEST_WASM_F32_CANONICAL_NAN_BITS);
        printf("  Actual result bits:    0x%08X\n", result_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

static wah_error_t test_f64_min_nan_canonicalization() {
    printf("\nTesting f64.min canonicalization with non-canonical NaNs...\n");
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for f64_min_nan: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for f64_min_nan: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t params_f64_min[2];
    params_f64_min[0].i64 = (int64_t)NON_CANONICAL_F64_NAN_BITS_1;
    params_f64_min[1].i64 = (int64_t)NON_CANONICAL_F64_NAN_BITS_2;
    wah_value_t result;
    err = wah_call(&exec_ctx, &module, 6, params_f64_min, 2, &result); // Call "test_f64_min_nan" (func_idx 6)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f64_min_nan: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    union { uint64_t i; double d; } u = { .d = result.f64 };
    uint64_t result_i64_bits = u.i;
    if (result_i64_bits == TEST_WASM_F64_CANONICAL_NAN_BITS) {
        printf("f64.min test PASSED: Min of non-canonical NaNs resulted in canonical NaN.\n");
    } else {
        printf("f64.min test FAILED: Min of non-canonical NaNs did NOT result in canonical NaN.\n");
        printf("  Expected canonical bits: 0x%llX\n", TEST_WASM_F64_CANONICAL_NAN_BITS);
        printf("  Actual result bits:    0x%" PRIx64 "\n", result_i64_bits);
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

// Test function for f64.promote_f32 with NaN canonicalization
static wah_error_t test_f64_promote_f32_nan_canonicalization() {
    printf("Running test_f64_promote_f32_nan_canonicalization...\n");

    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing module: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t param;
    param.i32 = (int32_t)0x7f800001; // Signaling NaN with payload 1

    wah_value_t result;
    err = wah_call(&exec_ctx, &module, 7, &param, 1, &result); // Call func_idx 7
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f64_promote_f32_nan_canonicalization: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    // Check if the result is the canonical f64 NaN
    if (double_to_bits(result.f64) == TEST_WASM_F64_CANONICAL_NAN_BITS) {
        printf("  PASS: f64.promote_f32 produced canonical NaN.\n");
    } else {
        printf("  FAIL: f64.promote_f32 did not produce canonical NaN. Expected 0x%llx, Got 0x%" PRIx64 "\n",
               TEST_WASM_F64_CANONICAL_NAN_BITS, double_to_bits(result.f64));
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

// Test function for f32.demote_f64 with NaN canonicalization
static wah_error_t test_f32_demote_f64_nan_canonicalization() {
    printf("Running test_f32_demote_f64_nan_canonicalization...\n");

    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;

    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing module: %s\n", wah_strerror(err));
        return err;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    wah_value_t param;
    param.i64 = (int64_t)0x7ff0000000000001ULL; // Signaling NaN with payload 1

    wah_value_t result;
    err = wah_call(&exec_ctx, &module, 8, &param, 1, &result); // Call func_idx 8
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_f32_demote_f64_nan_canonicalization: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return err;
    }

    // Check if the result is the canonical f32 NaN
    if (float_to_bits(result.f32) == TEST_WASM_F32_CANONICAL_NAN_BITS) {
        printf("  PASS: f32.demote_f64 produced canonical NaN.\n");
    } else {
        printf("  FAIL: f32.demote_f64 did not produce canonical NaN. Expected 0x%x, Got 0x%x\n",
               TEST_WASM_F32_CANONICAL_NAN_BITS, float_to_bits(result.f32));
        err = WAH_ERROR_VALIDATION_FAILED;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    return err;
}

int main() {
    wah_error_t err = WAH_OK;

    err = test_f32_store_canonicalization();
    if (err != WAH_OK) return 1;

    err = test_f32_const_canonicalization();
    if (err != WAH_OK) return 1;

    err = test_f32_reinterpret_nan_canonicalization();
    if (err != WAH_OK) return 1;

    err = test_f64_add_nan_canonicalization();
    if (err != WAH_OK) return 1;

    err = test_f32_add_nan_canonicalization();
    if (err != WAH_OK) return 1;

    err = test_f32_sqrt_nan_canonicalization();
    if (err != WAH_OK) return 1;

    err = test_f64_min_nan_canonicalization();
    if (err != WAH_OK) return 1;

    // NEW TESTS FOR NAN CANONICALIZATION
    err = test_f64_promote_f32_nan_canonicalization();
    if (err != WAH_OK) {
        fprintf(stderr, "test_f64_promote_f32_nan_canonicalization FAILED: %s\n", wah_strerror(err));
        return 1;
    }

    err = test_f32_demote_f64_nan_canonicalization();
    if (err != WAH_OK) {
        fprintf(stderr, "test_f32_demote_f64_nan_canonicalization FAILED: %s\n", wah_strerror(err));
        return 1;
    }

    printf("\nAll NaN canonicalization tests passed!\n");

    return 0;
}
