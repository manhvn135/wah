#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <string.h>
#include <math.h> // For isnan, though we'll use bit patterns for comparison
#include <stdlib.h> // For malloc, free

// Helper to convert float to uint32_t bit pattern
static uint32_t float_to_bits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(uint32_t));
    return bits;
}


// WebAssembly canonical NaN bit patterns (from wah.h)
#define TEST_WASM_F32_CANONICAL_NAN_BITS 0x7fc00000U
#define TEST_WASM_F64_CANONICAL_NAN_BITS 0x7ff8000000000000ULL

// A non-canonical NaN for testing (e.g., quiet NaN with some payload bits)
// Sign: 0, Exponent: all 1s, Mantissa: 010...0 (bit 22 is 0, bit 21 is 1)
// Canonical would be 0x7fc00000 (bit 22 is 1, rest 0)
static const uint32_t NON_CANONICAL_F32_NAN_BITS = 0x7fa00000U; // Example: qNaN with payload
static const union {
    uint32_t i;
    float f;
} non_canonical_f32_nan_union = { .i = NON_CANONICAL_F32_NAN_BITS };
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
    0x0A, // Section Size (10 bytes)
    0x02, // Number of types
        // Type 0: (func (param f32) (result f32))
        0x60, // func
        0x01, // param count
        0x7d, // f32
        0x01, // result count
        0x7d, // f32
        // Type 1: (func (result f32))
        0x60, // func
        0x00, // param count
        0x01, // result count
        0x7d, // f32

    // Function Section (3)
    0x03, // Section ID
    0x03, // Section Size (3 bytes)
    0x02, // Number of functions
    0x00, // Function 0 (test_store) uses type 0
    0x01, // Function 1 (test_const) uses type 1

    // Memory Section (5)
    0x05, // Section ID
    0x03, // Section Size (3 bytes)
    0x01, // Number of memories
    0x00, // Flags (min only)
    0x01, // Min pages (1)

    // Export Section (7)
    0x07, // Section ID
    0x21, // Section Size (33 bytes)
    0x03, // Number of exports
        // Export 0: "mem" (memory 0)
        0x03, // Length of "mem"
        'm', 'e', 'm',
        0x02, // Kind: Memory
        0x00, // Index: 0
        // Export 1: "test_store" (func 0)
        0x0a, // Length of "test_store"
        't', 'e', 's', 't', '_', 's', 't', 'o', 'r', 'e',
        0x00, // Kind: Function
        0x00, // Index: 0
        // Export 2: "test_const" (func 1)
        0x0a, // Length of "test_const"
        't', 'e', 's', 't', '_', 'c', 'o', 'n', 's', 't',
        0x00, // Kind: Function
        0x01, // Index: 1

    // Code Section (10)
    0x0A, // Section ID
    0x18, // Section Size (24 bytes)
    0x02, // Number of code bodies
        // Code Body 0: test_store
        0x0E, // Body Size (14 bytes)
        0x00, // Local count (0)
        0x41, // i32.const
        0x00, // value 0
        0x20, // local.get 0
        0x00, // index 0
        0x38, // f32.store
        0x00, 0x00, // align=0, offset=0 (LEB128 for 0, 0)
        0x41, // i32.const
        0x00, // value 0
        0x2a, // f32.load
        0x00, 0x00, // align=0, offset=0 (LEB128 for 0, 0)
        0x0b, // end
        // Code Body 1: test_const
        0x07, // Body Size (7 bytes)
        0x00, // Local count (0)
        0x43, // f32.const
        // Non-canonical NaN bits (little-endian)
        (NON_CANONICAL_F32_NAN_BITS >> 0) & 0xFF,
        (NON_CANONICAL_F32_NAN_BITS >> 8) & 0xFF,
        (NON_CANONICAL_F32_NAN_BITS >> 16) & 0xFF,
        (NON_CANONICAL_F32_NAN_BITS >> 24) & 0xFF,
        0x0b, // end
};

int main() {
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;
    wah_value_t result;

    // Test f32.store canonicalization
    printf("Testing f32.store canonicalization...\n");
    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing module: %s\n", wah_strerror(err));
        return 1;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 1;
    }

    wah_value_t param;
    param.f32 = NON_CANONICAL_F32_NAN;
    err = wah_call(&exec_ctx, &module, 0, &param, 1, &result); // Call "test_store" (func_idx 0)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_store: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return 1;
    }

    uint32_t result_bits = float_to_bits(result.f32);
    if (result_bits == TEST_WASM_F32_CANONICAL_NAN_BITS) {
        printf("f32.store test PASSED: Stored non-canonical NaN was canonicalized.\n");
    } else {
        printf("f32.store test FAILED: Stored non-canonical NaN was NOT canonicalized.\n");
        printf("  Expected canonical bits: 0x%08X\n", TEST_WASM_F32_CANONICAL_NAN_BITS);
        printf("  Actual result bits:    0x%08X\n", result_bits);
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);

    // Re-parse module for test_const (clean state)
    err = wah_parse_module(test_wasm_binary, sizeof(test_wasm_binary), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error re-parsing module for test_const: %s\n", wah_strerror(err));
        return 1;
    }

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for test_const: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 1;
    }

    printf("\nTesting f32.const canonicalization...\n");
    err = wah_call(&exec_ctx, &module, 1, NULL, 0, &result); // Call "test_const" (func_idx 1)
    if (err != WAH_OK) {
        fprintf(stderr, "Error calling test_const: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return 1;
    }

    result_bits = float_to_bits(result.f32);
    if (result_bits == TEST_WASM_F32_CANONICAL_NAN_BITS) {
        printf("f32.const test PASSED: Constant non-canonical NaN was canonicalized.\n");
    } else {
        printf("f32.const test FAILED: Constant non-canonical NaN was NOT canonicalized.\n");
        printf("  Expected canonical bits: 0x%08X\n", TEST_WASM_F32_CANONICAL_NAN_BITS);
        printf("  Actual result bits:    0x%08X\n", result_bits);
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);

    return 0;
}
