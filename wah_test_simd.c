#define WAH_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcmp
#include "wah.h"

// A simple WebAssembly binary for:
// (module
//   (func (result v128)
//     (v128.const i8x16 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F 0x10)
//   )
// )
const uint8_t v128_const_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section ID
    0x05, // section size
    0x01, // num types
    0x60, // func type
    0x00, // num params
    0x01, // num results
    0x7b, // v128

    // Function section
    0x03, // section ID
    0x02, // section size
    0x01, // num functions
    0x00, // type index 0

    // Code section
    0x0a, // section ID
    0x16, // section size
    0x01, // num code bodies
    0x14, // code body size (for the first function)
    0x00, // num locals
    0xfd, 0x0c, // v128.const
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 16-byte constant (little-endian)
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x0b, // end
};

// (module
//   (memory 1)
//   (func (result v128)
//     i32.const 0
//     v128.const i8x16 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88 0x99 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0x00
//     v128.store 0 0 ;; align=0, offset=0
//     i32.const 0
//     v128.load 0 0 ;; align=0, offset=0
//   )
// )
const uint8_t v128_load_store_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section ID
    0x05, // section size
    0x01, // num types
    0x60, // func type
    0x00, // num params
    0x01, // num results
    0x7b, // v128

    // Function section
    0x03, // section ID
    0x02, // section size
    0x01, // num functions
    0x00, // type index 0

    // Memory section
    0x05, // section ID
    0x03, // section size
    0x01, // num memories
    0x00, // memory type (min 1 page, max not present)
    0x01, // min pages

    // Code section
    0x0a, // section ID
    0x22, // section size
    0x01, // num code bodies
    0x20, // code body size (for the first function)
    0x00, // num locals
    0x41, 0x00, // i32.const 0
    0xfd, 0x0c, // v128.const
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
    0xfd, 0x0b, 0x00, 0x00, // v128.store align=0, offset=0
    0x41, 0x00, // i32.const 0
    0xfd, 0x00, 0x00, 0x00, // v128.load align=0, offset=0
    0x0b, // end
};

void test_v128_load_store() {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    printf("\n--- Testing v128.load and v128.store ---\n");
    printf("Parsing v128_load_store_wasm module...\n");
    err = wah_parse_module((const uint8_t *)v128_load_store_wasm, sizeof(v128_load_store_wasm), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing v128_load_store_wasm module: %s\n", wah_strerror(err));
        exit(1);
    }
    printf("Module parsed successfully.\n");

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %s\n", wah_strerror(err));
        wah_free_module(&module);
        exit(1);
    }

    uint32_t func_idx = 0;
    wah_value_t result;
    uint8_t expected_v128_val[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};

    printf("Interpreting function %u (v128.load/store)...\n", func_idx);
    err = wah_call(&ctx, &module, func_idx, NULL, 0, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        exit(1);
    }
    printf("Function interpreted successfully.\n");

    if (memcmp(&result.v128, expected_v128_val, sizeof(wah_v128_t)) == 0) {
        printf("Result v128 matches expected value.\n");
    } else {
        fprintf(stderr, "Result v128 does NOT match expected value.\n");
        fprintf(stderr, "Expected: ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", expected_v128_val[i]);
        }
        fprintf(stderr, "\nActual:   ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", result.v128.u8[i]);
        }
        fprintf(stderr, "\n");
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        exit(1);
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("v128.load/store test completed successfully.\n");
}

void test_v128_const() {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    printf("--- Testing v128.const ---\n");
    printf("Parsing v128_const_wasm module...\n");
    err = wah_parse_module((const uint8_t *)v128_const_wasm, sizeof(v128_const_wasm), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing v128_const_wasm module: %s\n", wah_strerror(err));
        exit(1);
    }
    printf("Module parsed successfully.\n");

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %s\n", wah_strerror(err));
        wah_free_module(&module);
        exit(1);
    }

    uint32_t func_idx = 0;
    wah_value_t result;
    uint8_t expected_v128_val[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    printf("Interpreting function %u (v128.const)...\n", func_idx);
    err = wah_call(&ctx, &module, func_idx, NULL, 0, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        exit(1);
    }
    printf("Function interpreted successfully.\n");

    if (memcmp(&result.v128, expected_v128_val, sizeof(wah_v128_t)) == 0) {
        printf("Result v128 matches expected value.\n");
    } else {
        fprintf(stderr, "Result v128 does NOT match expected value.\n");
        fprintf(stderr, "Expected: ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", expected_v128_val[i]);
        }
        fprintf(stderr, "\nActual:   ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", result.v128.u8[i]);
        }
        fprintf(stderr, "\n");
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        exit(1);
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("v128.const test completed successfully.\n");
}

#define LOAD_TEST_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x05, 0x03, 0x01, 0x00, 0x01, \
    0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00, 0xfd, subopcode, 0x00, 0x00, 0x0b, \
}

const uint8_t v128_load8x8_s_wasm[] = LOAD_TEST_WASM(0x01);
const uint8_t v128_load8x8_u_wasm[] = LOAD_TEST_WASM(0x02);
const uint8_t v128_load16x4_s_wasm[] = LOAD_TEST_WASM(0x03);
const uint8_t v128_load16x4_u_wasm[] = LOAD_TEST_WASM(0x04);
const uint8_t v128_load32x2_s_wasm[] = LOAD_TEST_WASM(0x05);
const uint8_t v128_load32x2_u_wasm[] = LOAD_TEST_WASM(0x06);
const uint8_t v128_load8_splat_wasm[] = LOAD_TEST_WASM(0x07);
const uint8_t v128_load16_splat_wasm[] = LOAD_TEST_WASM(0x08);
const uint8_t v128_load32_splat_wasm[] = LOAD_TEST_WASM(0x09);
const uint8_t v128_load64_splat_wasm[] = LOAD_TEST_WASM(0x0a);
const uint8_t v128_load32_zero_wasm[] = LOAD_TEST_WASM(0x5c);
const uint8_t v128_load64_zero_wasm[] = LOAD_TEST_WASM(0x5d);

// (i32.const 0) (v128.const 0 ... 0) (i32.const 0) (v128.loadN_lane align=0 offset=0 lane_idx=0)
#define LOAD_LANE_TEST_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x05, 0x03, 0x01, 0x00, 0x01, \
    0x0a, 0x1f, 0x01, 0x1d, 0x00, \
        0x41, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x41, 0x00, \
        0xfd, subopcode, 0x00, 0x00, 0x00, \
        0x0b, \
}

const uint8_t v128_load8_lane_wasm[] = LOAD_LANE_TEST_WASM(0x54);
const uint8_t v128_load16_lane_wasm[] = LOAD_LANE_TEST_WASM(0x55);
const uint8_t v128_load32_lane_wasm[] = LOAD_LANE_TEST_WASM(0x56);
const uint8_t v128_load64_lane_wasm[] = LOAD_LANE_TEST_WASM(0x57);

// Generic WASM template for binary SIMD operations (pop 2 v128, push 1 v128)
// (module
//   (func (result v128)
//     (v128.const ...)
//     (v128.const ...)
//     (subopcode)
//   )
// )
#define SIMD_BINARY_OP_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x2b, 0x01, 0x29, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, (subopcode & 0x7f) | 0x80, subopcode >> 7, \
        0x0b, \
}

wah_error_t run_v128_load_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const uint8_t* expected_val, const uint8_t* initial_memory, size_t initial_memory_size) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    printf("\n--- Testing %s ---\n", test_name);
    printf("Parsing module...\n");
    err = wah_parse_module(wasm_binary, wasm_size, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing %s module: %s\n", test_name, wah_strerror(err));
        return err;
    }
    printf("Module parsed successfully.\n");

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for %s: %s\n", test_name, wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    if (initial_memory && initial_memory_size > 0) {
        WAH_ENSURE(initial_memory_size <= ctx.memory_size, WAH_ERROR_MISUSE);
        memcpy(ctx.memory_base, initial_memory, initial_memory_size);
    }

    uint32_t func_idx = 0;
    wah_value_t result;

    printf("Interpreting function %u...\n", func_idx);
    err = wah_call(&ctx, &module, func_idx, NULL, 0, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function for %s: %s\n", test_name, wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return err;
    }
    printf("Function interpreted successfully.\n");

    if (memcmp(&result.v128, expected_val, sizeof(wah_v128_t)) == 0) {
        printf("Result v128 matches expected value for %s.\n", test_name);
    } else {
        fprintf(stderr, "Result v128 does NOT match expected value for %s.\n", test_name);
        fprintf(stderr, "Expected: ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", expected_val[i]);
        }
        fprintf(stderr, "\nActual:   ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", result.v128.u8[i]);
        }
        fprintf(stderr, "\n");
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return WAH_ERROR_VALIDATION_FAILED; // Indicate test failure
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_v128_load_test(n,b,v,m,s) run_v128_load_test(n,b,sizeof(b),v,m,s)

wah_error_t run_simd_binary_op_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand1, const wah_v128_t* operand2, const wah_v128_t* expected_val) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    // Set the v128.const values in the WASM binary for the operands
    uint8_t wasm_binary_copy[wasm_size];
    memcpy(wasm_binary_copy, wasm_binary, wasm_size);
    memcpy(wasm_binary_copy + 26, operand1, sizeof(wah_v128_t));
    memcpy(wasm_binary_copy + 44, operand2, sizeof(wah_v128_t));

    printf("\n--- Testing %s ---\n", test_name);
    printf("Parsing module...\n");
    err = wah_parse_module(wasm_binary_copy, wasm_size, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing %s module: %s\n", test_name, wah_strerror(err));
        return err;
    }
    printf("Module parsed successfully.\n");

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for %s: %s\n", test_name, wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    uint32_t func_idx = 0;
    wah_value_t result;

    printf("Interpreting function %u...\n", func_idx);
    err = wah_call(&ctx, &module, func_idx, NULL, 0, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function for %s: %s\n", test_name, wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return err;
    }
    printf("Function interpreted successfully.\n");

    if (memcmp(&result.v128, expected_val, sizeof(wah_v128_t)) == 0) {
        printf("Result v128 matches expected value for %s.\n", test_name);
    } else {
        fprintf(stderr, "Result v128 does NOT match expected value for %s.\n", test_name);
        fprintf(stderr, "Expected: ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", expected_val->u8[i]);
        }
        fprintf(stderr, "\nActual:   ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", result.v128.u8[i]);
        }
        fprintf(stderr, "\n");
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return WAH_ERROR_VALIDATION_FAILED; // Indicate test failure
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_binary_op_test(n,b,o1,o2,e) run_simd_binary_op_test(n,b,sizeof(b),o1,o2,e)

// Generic WASM template for unary SIMD operations (pop 1 v128, push 1 v128)
// (module
//   (func (result v128)
//     (v128.const ...)
//     (subopcode)
//   )
// )
#define SIMD_UNARY_OP_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x19, 0x01, 0x17, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, (subopcode & 0x7f) | 0x80, subopcode >> 7, \
        0x0b, \
}

wah_error_t run_simd_unary_op_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand, const wah_v128_t* expected_val) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    // Set the v128.const value in the WASM binary for the operand
    uint8_t wasm_binary_copy[wasm_size];
    memcpy(wasm_binary_copy, wasm_binary, wasm_size);
    memcpy(wasm_binary_copy + 26, operand, sizeof(wah_v128_t));

    printf("\n--- Testing %s ---\n", test_name);
    printf("Parsing module...\n");
    err = wah_parse_module(wasm_binary_copy, wasm_size, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing %s module: %s\n", test_name, wah_strerror(err));
        return err;
    }
    printf("Module parsed successfully.\n");

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for %s: %s\n", test_name, wah_strerror(err));
        wah_free_module(&module);
        return err;
    }

    uint32_t func_idx = 0;
    wah_value_t result;

    printf("Interpreting function %u...\n", func_idx);
    err = wah_call(&ctx, &module, func_idx, NULL, 0, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function for %s: %s\n", test_name, wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return err;
    }
    printf("Function interpreted successfully.\n");

    if (memcmp(&result.v128, expected_val, sizeof(wah_v128_t)) == 0) {
        printf("Result v128 matches expected value for %s.\n", test_name);
    } else {
        fprintf(stderr, "Result v128 does NOT match expected value for %s.\n", test_name);
        fprintf(stderr, "Expected: ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", expected_val->u8[i]);
        }
        fprintf(stderr, "\nActual:   ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", result.v128.u8[i]);
        }
        fprintf(stderr, "\n");
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return WAH_ERROR_VALIDATION_FAILED; // Indicate test failure
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_unary_op_test(n,b,o,e) run_simd_unary_op_test(n,b,sizeof(b),o,e)

void test_all_v128_loads() {
    wah_error_t err;
    // Initial memory for all load tests
    uint8_t initial_mem[16] = {0x01, 0x82, 0x03, 0x84, 0x05, 0x86, 0x07, 0x88, 0x09, 0x8A, 0x0B, 0x8C, 0x0D, 0x8E, 0x0F, 0x90};

    // v128.load8x8_s
    uint8_t expected_8x8_s[16] = {
        0x01, 0x00, 0x82, 0xFF, 0x03, 0x00, 0x84, 0xFF, 0x05, 0x00, 0x86, 0xFF, 0x07, 0x00, 0x88, 0xFF
    }; // 0x01, 0x82(-126), 0x03, 0x84(-124), 0x05, 0x86(-122), 0x07, 0x88(-120) sign-extended to 16-bit
    err = run_v128_load_test("v128.load8x8_s", v128_load8x8_s_wasm, expected_8x8_s, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load8x8_u
    uint8_t expected_8x8_u[16] = {
        0x01, 0x00, 0x82, 0x00, 0x03, 0x00, 0x84, 0x00, 0x05, 0x00, 0x86, 0x00, 0x07, 0x00, 0x88, 0x00
    }; // 0x01, 0x82, 0x03, 0x84, 0x05, 0x86, 0x07, 0x88
    err = run_v128_load_test("v128.load8x8_u", v128_load8x8_u_wasm, expected_8x8_u, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load16x4_s
    uint8_t expected_16x4_s[16] = {
        0x01, 0x82, 0xFF, 0xFF, 0x03, 0x84, 0xFF, 0xFF, 0x05, 0x86, 0xFF, 0xFF, 0x07, 0x88, 0xFF, 0xFF
    }; // 0x8201, 0x8403, 0x8605, 0x8807 sign-extended to 32-bit
    err = run_v128_load_test("v128.load16x4_s", v128_load16x4_s_wasm, expected_16x4_s, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load16x4_u
    uint8_t expected_16x4_u[16] = {
        0x01, 0x82, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x05, 0x86, 0x00, 0x00, 0x07, 0x88, 0x00, 0x00
    }; // 0x8201, 0x8403, 0x8605, 0x8807 zero-extended to 32-bit
    err = run_v128_load_test("v128.load16x4_u", v128_load16x4_u_wasm, expected_16x4_u, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load32x2_s
    uint8_t expected_32x2_s[16] = {
        0x01, 0x82, 0x03, 0x84, 0xFF, 0xFF, 0xFF, 0xFF, 0x05, 0x86, 0x07, 0x88, 0xFF, 0xFF, 0xFF, 0xFF
    }; // 0x84038201, 0x88078605 sign-extended to 64-bit
    err = run_v128_load_test("v128.load32x2_s", v128_load32x2_s_wasm, expected_32x2_s, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load32x2_u
    uint8_t expected_32x2_u[16] = {
        0x01, 0x82, 0x03, 0x84, 0x00, 0x00, 0x00, 0x00, 0x05, 0x86, 0x07, 0x88, 0x00, 0x00, 0x00, 0x00
    }; // 0x84038201, 0x88078605 zero-extended to 64-bit
    err = run_v128_load_test("v128.load32x2_u", v128_load32x2_u_wasm, expected_32x2_u, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load8_splat
    uint8_t expected_8_splat[16] = {
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
    }; // 0x01 splatted
    err = run_v128_load_test("v128.load8_splat", v128_load8_splat_wasm, expected_8_splat, initial_mem, 1);
    if (err != WAH_OK) { exit(1); }

    // v128.load16_splat
    uint8_t expected_16_splat[16] = {
        0x01, 0x82, 0x01, 0x82, 0x01, 0x82, 0x01, 0x82, 0x01, 0x82, 0x01, 0x82, 0x01, 0x82, 0x01, 0x82
    }; // 0x8201 splatted
    err = run_v128_load_test("v128.load16_splat", v128_load16_splat_wasm, expected_16_splat, initial_mem, 2);
    if (err != WAH_OK) { exit(1); }

    // v128.load32_splat
    uint8_t expected_32_splat[16] = {
        0x01, 0x82, 0x03, 0x84, 0x01, 0x82, 0x03, 0x84, 0x01, 0x82, 0x03, 0x84, 0x01, 0x82, 0x03, 0x84
    }; // 0x84038201 splatted
    err = run_v128_load_test("v128.load32_splat", v128_load32_splat_wasm, expected_32_splat, initial_mem, 4);
    if (err != WAH_OK) { exit(1); }

    // v128.load64_splat
    uint8_t expected_64_splat[16] = {
        0x01, 0x82, 0x03, 0x84, 0x05, 0x86, 0x07, 0x88, 0x01, 0x82, 0x03, 0x84, 0x05, 0x86, 0x07, 0x88
    }; // 0x8807860584038201 splatted
    err = run_v128_load_test("v128.load64_splat", v128_load64_splat_wasm, expected_64_splat, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load32_zero
    uint8_t expected_32_zero[16] = {
        0x01, 0x82, 0x03, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }; // 0x84038201, rest zero
    err = run_v128_load_test("v128.load32_zero", v128_load32_zero_wasm, expected_32_zero, initial_mem, 4);
    if (err != WAH_OK) { exit(1); }

    // v128.load64_zero
    uint8_t expected_64_zero[16] = {
        0x01, 0x82, 0x03, 0x84, 0x05, 0x86, 0x07, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }; // 0x8807860584038201, rest zero
    err = run_v128_load_test("v128.load64_zero", v128_load64_zero_wasm, expected_64_zero, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }

    // v128.load8_lane
    uint8_t expected_8_lane[16] = {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }; // initial all zeros, lane 0 gets 0x01
    err = run_v128_load_test("v128.load8_lane", v128_load8_lane_wasm, expected_8_lane, initial_mem, 1);
    if (err != WAH_OK) { exit(1); }

    // v128.load16_lane
    uint8_t expected_16_lane[16] = {
        0x01, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }; // initial all zeros, lane 0 gets 0x8201
    err = run_v128_load_test("v128.load16_lane", v128_load16_lane_wasm, expected_16_lane, initial_mem, 2);
    if (err != WAH_OK) { exit(1); }

    // v128.load32_lane
    uint8_t expected_32_lane[16] = {
        0x01, 0x82, 0x03, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }; // initial all zeros, lane 0 gets 0x84038201
    err = run_v128_load_test("v128.load32_lane", v128_load32_lane_wasm, expected_32_lane, initial_mem, 4);
    if (err != WAH_OK) { exit(1); }

    // v128.load64_lane
    uint8_t expected_64_lane[16] = {
        0x01, 0x82, 0x03, 0x84, 0x05, 0x86, 0x07, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }; // initial all zeros, lane 0 gets 0x8807860584038201
    err = run_v128_load_test("v128.load64_lane", v128_load64_lane_wasm, expected_64_lane, initial_mem, 8);
    if (err != WAH_OK) { exit(1); }
}

// Test cases for v128.not
const uint8_t v128_not_wasm[] = SIMD_UNARY_OP_WASM(0x4d);
void test_v128_not() {
    wah_v128_t operand = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t expected = {{0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0, 0xEF}};
    if (run_simd_unary_op_test("v128.not", v128_not_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.add
const uint8_t i8x16_add_wasm[] = SIMD_BINARY_OP_WASM(0x6e);
void test_i8x16_add() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t operand2 = {{0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}};
    wah_v128_t expected = {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};
    if (run_simd_binary_op_test("i8x16.add", i8x16_add_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.add
const uint8_t f32x4_add_wasm[] = SIMD_BINARY_OP_WASM(0xe4);
void test_f32x4_add() {
    // Using float values directly for clarity, will be converted to byte arrays
    union { float f[4]; uint8_t u8[16]; } op1_union, op2_union, expected_union;

    op1_union.f[0] = 1.0f; op1_union.f[1] = 2.0f; op1_union.f[2] = 3.0f; op1_union.f[3] = 4.0f;
    op2_union.f[0] = 5.0f; op2_union.f[1] = 6.0f; op2_union.f[2] = 7.0f; op2_union.f[3] = 8.0f;
    expected_union.f[0] = 6.0f; expected_union.f[1] = 8.0f; expected_union.f[2] = 10.0f; expected_union.f[3] = 12.0f;

    wah_v128_t operand1, operand2, expected;
    memcpy(operand1.u8, op1_union.u8, 16);
    memcpy(operand2.u8, op2_union.u8, 16);
    memcpy(expected.u8, expected_union.u8, 16);

    if (run_simd_binary_op_test("f32x4.add", f32x4_add_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.and
const uint8_t v128_and_wasm[] = SIMD_BINARY_OP_WASM(0x4E);
void test_v128_and() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0xF0, 0x00, 0x0F, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00, 0x40, 0x00}};
    if (run_simd_binary_op_test("v128.and", v128_and_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.andnot
const uint8_t v128_andnot_wasm[] = SIMD_BINARY_OP_WASM(0x4F);
void test_v128_andnot() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0x0F, 0x00, 0xF0, 0x00, 0x55, 0x00, 0x55, 0x00, 0xFE, 0x00, 0xFB, 0x00, 0xEF, 0x00, 0xBF, 0x00}};
    if (run_simd_binary_op_test("v128.andnot", v128_andnot_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.or
const uint8_t v128_or_wasm[] = SIMD_BINARY_OP_WASM(0x50);
void test_v128_or() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0xFF, 0xF0, 0xFF, 0x0F, 0xFF, 0x55, 0xFF, 0x55, 0xFF, 0x02, 0xFF, 0x08, 0xFF, 0x20, 0xFF, 0x80}};
    if (run_simd_binary_op_test("v128.or", v128_or_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.xor
const uint8_t v128_xor_wasm[] = SIMD_BINARY_OP_WASM(0x51);
void test_v128_xor() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0x0F, 0xF0, 0xF0, 0x0F, 0x55, 0x55, 0x55, 0x55, 0xFE, 0x02, 0xFB, 0x08, 0xEF, 0x20, 0xBF, 0x80}};
    if (run_simd_binary_op_test("v128.xor", v128_xor_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.add_sat_s
const uint8_t i8x16_add_sat_s_wasm[] = SIMD_BINARY_OP_WASM(0x6F);
void test_i8x16_add_sat_s() {
    wah_v128_t operand1 = {{0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x02, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.add_sat_s", i8x16_add_sat_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.add_sat_u
const uint8_t i8x16_add_sat_u_wasm[] = SIMD_BINARY_OP_WASM(0x70);
void test_i8x16_add_sat_u() {
    wah_v128_t operand1 = {{0x01, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x02, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.add_sat_u", i8x16_add_sat_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.sub
const uint8_t i8x16_sub_wasm[] = SIMD_BINARY_OP_WASM(0x71);
void test_i8x16_sub() {
    wah_v128_t operand1 = {{0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.sub", i8x16_sub_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.sub_sat_s
const uint8_t i8x16_sub_sat_s_wasm[] = SIMD_BINARY_OP_WASM(0x72);
void test_i8x16_sub_sat_s() {
    wah_v128_t operand1 = {{0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.sub_sat_s", i8x16_sub_sat_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.sub_sat_u
const uint8_t i8x16_sub_sat_u_wasm[] = SIMD_BINARY_OP_WASM(0x73);
void test_i8x16_sub_sat_u() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.sub_sat_u", i8x16_sub_sat_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.sub
const uint8_t i16x8_sub_wasm[] = SIMD_BINARY_OP_WASM(0x91);
void test_i16x8_sub() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 0x0001, 0x0005
    wah_v128_t operand2 = {{0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 0x0002, 0x0003
    wah_v128_t expected = {{0xFF, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 0xFFFF (-1), 0x0002
    if (run_simd_binary_op_test("i16x8.sub", i16x8_sub_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.mul
const uint8_t i32x4_mul_wasm[] = SIMD_BINARY_OP_WASM(0xb5);
void test_i32x4_mul() {
    wah_v128_t operand1 = {{0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 3
    wah_v128_t operand2 = {{0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 3, 4
    wah_v128_t expected = {{0x06, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 6, 12
    if (run_simd_binary_op_test("i32x4.mul", i32x4_mul_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

int main() {
    test_v128_const();
    test_v128_load_store();
    test_all_v128_loads();
    test_v128_not();
    test_i8x16_add();
    test_f32x4_add();
    test_v128_and();
    test_v128_andnot();
    test_v128_or();
    test_v128_xor();
    test_i8x16_add_sat_s();
    test_i8x16_add_sat_u();
    test_i8x16_sub();
    test_i8x16_sub_sat_s();
    test_i8x16_sub_sat_u();
    test_i16x8_sub();
    test_i32x4_mul();
    return 0;
}
