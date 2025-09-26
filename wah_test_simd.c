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
#define BINARY_OP_WASM(subopcode) { \
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

// Generic WASM template for ternary SIMD operations (pop 3 v128, push 1 v128)
// (module
//   (func (result v128)
//     (v128.const ...)
//     (v128.const ...)
//     (v128.const ...)
//     (subopcode)
//   )
// )
#define TERNARY_OP_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x3d, 0x01, 0x3b, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, (subopcode & 0x7f) | 0x80, subopcode >> 7, \
        0x0b, \
}

// Helper to encode a 32-bit signed integer as LEB128. Returns number of bytes written.
static size_t encode_s32_leb128(int32_t value, uint8_t* out) {
    size_t i = 0;
    while (true) {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0)) {
            out[i++] = byte;
            break;
        }
        out[i++] = byte | 0x80;
    }
    return i;
}

// Helper to encode a 64-bit signed integer as LEB128. Returns number of bytes written.
static size_t encode_s64_leb128(int64_t value, uint8_t* out) {
    size_t i = 0;
    while (true) {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0)) {
            out[i++] = byte;
            break;
        }
        out[i++] = byte | 0x80;
    }
    return i;
}

// Helper to patch a scalar constant (xxx.const) into a WASM binary.
// wasm_binary_ptr points to the location where the opcode (0x41, 0x42, 0x43, 0x44) should be written.
// It assumes 10 bytes are reserved for the opcode + value (1 byte for opcode, 9 for value).
static wah_error_t patch_scalar_const(uint8_t* wasm_binary_ptr, const wah_value_t* operand_scalar, wah_type_t scalar_type) {
    // Clear the 9-byte value placeholder with NOPs (0x01)
    memset(wasm_binary_ptr + 1, 0x01, 9);

    switch (scalar_type) {
        case WAH_TYPE_I32:
            wasm_binary_ptr[0] = 0x41; // i32.const
            encode_s32_leb128(operand_scalar->i32, wasm_binary_ptr + 1);
            break;
        case WAH_TYPE_I64:
            wasm_binary_ptr[0] = 0x42; // i64.const
            encode_s64_leb128(operand_scalar->i64, wasm_binary_ptr + 1);
            break;
        case WAH_TYPE_F32:
            wasm_binary_ptr[0] = 0x43; // f32.const
            memcpy(wasm_binary_ptr + 1, &operand_scalar->f32, 4);
            break;
        case WAH_TYPE_F64:
            wasm_binary_ptr[0] = 0x44; // f64.const
            memcpy(wasm_binary_ptr + 1, &operand_scalar->f64, 8);
            break;
        default:
            fprintf(stderr, "Unsupported scalar type for patch_scalar_const: %d\n", scalar_type);
            return WAH_ERROR_MISUSE;
    }
    return WAH_OK;
}

// Helper to compare v128 results and print byte-by-byte if they don't match.
static wah_error_t compare_and_print_v128_result(const char* test_name, const wah_v128_t* actual, const wah_v128_t* expected) {
    if (memcmp(actual, expected, sizeof(wah_v128_t)) == 0) {
        printf("Result v128 matches expected value for %s.\n", test_name);
        return WAH_OK;
    } else {
        fprintf(stderr, "Result v128 does NOT match expected value for %s.\n", test_name);
        fprintf(stderr, "Expected: ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", expected->u8[i]);
        }
        fprintf(stderr, "\nActual:   ");
        for (int i = 0; i < 16; ++i) {
            fprintf(stderr, "%02x ", actual->u8[i]);
        }
        fprintf(stderr, "\n");
        return WAH_ERROR_VALIDATION_FAILED; // Indicate test failure
    }
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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return err;
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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_binary_op_test(n,b,o1,o2,e) run_simd_binary_op_test(n,b,sizeof(b),o1,o2,e)

wah_error_t run_simd_ternary_op_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand1, const wah_v128_t* operand2, const wah_v128_t* operand3, const wah_v128_t* expected_val) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    // Set the v128.const values in the WASM binary for the operands
    uint8_t wasm_binary_copy[wasm_size];
    memcpy(wasm_binary_copy, wasm_binary, wasm_size);
    memcpy(wasm_binary_copy + 26, operand1, sizeof(wah_v128_t));
    memcpy(wasm_binary_copy + 44, operand2, sizeof(wah_v128_t));
    memcpy(wasm_binary_copy + 62, operand3, sizeof(wah_v128_t));

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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_ternary_op_test(n,b,o1,o2,o3,e) run_simd_ternary_op_test(n,b,sizeof(b),o1,o2,o3,e)

// Generic WASM template for unary SIMD operations (pop 1 v128, push 1 v128)
// (module
//   (func (result v128)
//     (v128.const ...)
//     (subopcode)
//   )
// )
#define UNARY_OP_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x19, 0x01, 0x17, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, (subopcode & 0x7f) | 0x80, subopcode >> 7, \
        0x0b, \
}

// WASM template for _extract_lane (pop 1 v128, push 1 scalar, 1 immediate byte)
#define EXTRACT_LANE_WASM(subopcode, laneidx) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x19, 0x01, 0x17, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, subopcode, laneidx, \
        0x0b, \
}

// WASM template for _replace_lane (pop 1 scalar, pop 1 v128, push 1 v128, 1 immediate byte)
#define REPLACE_LANE_WASM(subopcode, laneidx) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x23, 0x01, 0x21, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 10 NOPs (placeholder for scalar) */ \
        0xfd, subopcode, laneidx, \
        0x0b, \
}

// WASM template for _splat (pop 1 scalar, push 1 v128)
#define SPLAT_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x10, 0x01, 0x0e, 0x00, \
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, /* 10 NOPs (placeholder for scalar) */ \
        0xfd, subopcode, \
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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_unary_op_test(n,b,o,e) run_simd_unary_op_test(n,b,sizeof(b),o,e)

wah_error_t run_simd_binary_op_i32_shift_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand1, const wah_value_t* operand_scalar, wah_type_t scalar_type, const wah_v128_t* expected_val) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    // Set the v128.const and scalar.const values in the WASM binary
    uint8_t wasm_binary_copy[wasm_size];
    memcpy(wasm_binary_copy, wasm_binary, wasm_size);
    memcpy(wasm_binary_copy + 26, operand1, sizeof(wah_v128_t));
    WAH_CHECK(patch_scalar_const(wasm_binary_copy + 42, operand_scalar, scalar_type));

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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_binary_op_i32_shift_test(n,b,o1,o_scalar,s_type,e) run_simd_binary_op_i32_shift_test(n,b,sizeof(b),o1,o_scalar,s_type,e)

wah_error_t run_simd_shuffle_swizzle_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* expected_val) {
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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_shuffle_swizzle_test(n,b,e) run_simd_shuffle_swizzle_test(n,b,sizeof(b),e)

wah_error_t run_simd_extract_lane_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand, const wah_value_t* expected_val, wah_type_t expected_type) {
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

    bool match = false;
    switch (expected_type) {
        case WAH_TYPE_I32: match = (result.i32 == expected_val->i32); break;
        case WAH_TYPE_I64: match = (result.i64 == expected_val->i64); break;
        case WAH_TYPE_F32: match = (result.f32 == expected_val->f32); break;
        case WAH_TYPE_F64: match = (result.f64 == expected_val->f64); break;
        default: match = false; break;
    }

    if (match) {
        printf("Result scalar matches expected value for %s.\n", test_name);
    } else {
        fprintf(stderr, "Result scalar does NOT match expected value for %s.\n", test_name);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return WAH_ERROR_VALIDATION_FAILED; // Indicate test failure
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_extract_lane_test(n,b,o,e,t) run_simd_extract_lane_test(n,b,sizeof(b),o,e,t)

wah_error_t run_simd_replace_lane_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand_v128, const wah_value_t* operand_scalar, wah_type_t scalar_type, const wah_v128_t* expected_val) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    // Set the v128.const and scalar.const values in the WASM binary
    uint8_t wasm_binary_copy[wasm_size];
    memcpy(wasm_binary_copy, wasm_binary, wasm_size);
    memcpy(wasm_binary_copy + 26, operand_v128, sizeof(wah_v128_t));
    WAH_CHECK(patch_scalar_const(wasm_binary_copy + 42, operand_scalar, scalar_type));

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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_replace_lane_test(n,b,o_v128,o_scalar,s_type,e) run_simd_replace_lane_test(n,b,sizeof(b),o_v128,o_scalar,s_type,e)

wah_error_t run_simd_splat_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_value_t* operand_scalar, wah_type_t scalar_type, const wah_v128_t* expected_val) {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err = WAH_OK;

    // Set the scalar.const value in the WASM binary
    uint8_t wasm_binary_copy[wasm_size];
    memcpy(wasm_binary_copy, wasm_binary, wasm_size);
    WAH_CHECK(patch_scalar_const(wasm_binary_copy + 24, operand_scalar, scalar_type));

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

    err = compare_and_print_v128_result(test_name, &result.v128, (const wah_v128_t*)expected_val);
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_splat_test(n,b,o_scalar,s_type,e) run_simd_splat_test(n,b,sizeof(b),o_scalar,s_type,e)

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
const uint8_t v128_not_wasm[] = UNARY_OP_WASM(0x4d);
void test_v128_not() {
    wah_v128_t operand = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t expected = {{0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0, 0xEF}};
    if (run_simd_unary_op_test("v128.not", v128_not_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

#define V128_UNARY_OP_WASM(subopcode) { \
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, \
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, \
    0x03, 0x02, 0x01, 0x00, \
    0x0a, 0x19, 0x01, 0x17, 0x00, \
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0xfd, (subopcode & 0x7f) | 0x80, subopcode >> 7, \
        0x0b, \
};

#define V128_BINARY_OP_WASM(subopcode) { \
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
};

// Test cases for v128.bitselect
const uint8_t v128_bitselect_wasm[] = TERNARY_OP_WASM(0x52);
void test_v128_bitselect() {
    wah_v128_t v1 = {{0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0}};
    wah_v128_t v2 = {{0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F}};
    wah_v128_t v3 = {{0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}};
    wah_v128_t expected = {{0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A}};
    if (run_simd_ternary_op_test("v128.bitselect", v128_bitselect_wasm, &v1, &v2, &v3, &expected) != WAH_OK) { exit(1); }
}

wah_error_t run_simd_any_true_test(const char* test_name, const uint8_t* wasm_binary, size_t wasm_size, const wah_v128_t* operand, int32_t expected_val) {
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

    if (result.i32 == expected_val) {
        printf("Result scalar matches expected value for %s.\n", test_name);
    } else {
        fprintf(stderr, "Result scalar does NOT match expected value for %s.\n", test_name);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return WAH_ERROR_VALIDATION_FAILED; // Indicate test failure
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("%s test completed successfully.\n", test_name);
    return WAH_OK;
}
#define run_simd_any_true_test(n,b,o,e) run_simd_any_true_test(n,b,sizeof(b),o,e)

const uint8_t v128_any_true_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x18, 0x01, 0x16, 0x00,
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xfd, 0x53,
        0x0b,
};
void test_v128_any_true() {
    wah_v128_t operand_true = {{0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand_false = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_any_true_test("v128.any_true (true)", v128_any_true_wasm, &operand_true, 1) != WAH_OK) { exit(1); }
    if (run_simd_any_true_test("v128.any_true (false)", v128_any_true_wasm, &operand_false, 0) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.add
const uint8_t i8x16_add_wasm[] = BINARY_OP_WASM(0x6e);
void test_i8x16_add() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t operand2 = {{0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}};
    wah_v128_t expected = {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};
    if (run_simd_binary_op_test("i8x16.add", i8x16_add_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.add
const uint8_t f32x4_add_wasm[] = BINARY_OP_WASM(0xe4);
void test_f32x4_add() {
    wah_v128_t operand1 = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} }, operand2 = { .f32 = {5.0, 6.0, 7.0, 8.0} };
    wah_v128_t expected = { .f32 = {6.0f, 8.0f, 10.0f, 12.0f} };
    if (run_simd_binary_op_test("f32x4.add", f32x4_add_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.and
const uint8_t v128_and_wasm[] = BINARY_OP_WASM(0x4E);
void test_v128_and() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0xF0, 0x00, 0x0F, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00, 0x40, 0x00}};
    if (run_simd_binary_op_test("v128.and", v128_and_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.andnot
const uint8_t v128_andnot_wasm[] = BINARY_OP_WASM(0x4F);
void test_v128_andnot() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0x0F, 0x00, 0xF0, 0x00, 0x55, 0x00, 0x55, 0x00, 0xFE, 0x00, 0xFB, 0x00, 0xEF, 0x00, 0xBF, 0x00}};
    if (run_simd_binary_op_test("v128.andnot", v128_andnot_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.or
const uint8_t v128_or_wasm[] = BINARY_OP_WASM(0x50);
void test_v128_or() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0xFF, 0xF0, 0xFF, 0x0F, 0xFF, 0x55, 0xFF, 0x55, 0xFF, 0x02, 0xFF, 0x08, 0xFF, 0x20, 0xFF, 0x80}};
    if (run_simd_binary_op_test("v128.or", v128_or_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for v128.xor
const uint8_t v128_xor_wasm[] = BINARY_OP_WASM(0x51);
void test_v128_xor() {
    wah_v128_t operand1 = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    wah_v128_t operand2 = {{0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0x55, 0xAA, 0x55, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0x0F, 0xF0, 0xF0, 0x0F, 0x55, 0x55, 0x55, 0x55, 0xFE, 0x02, 0xFB, 0x08, 0xEF, 0x20, 0xBF, 0x80}};
    if (run_simd_binary_op_test("v128.xor", v128_xor_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.add_sat_s
const uint8_t i8x16_add_sat_s_wasm[] = BINARY_OP_WASM(0x6F);
void test_i8x16_add_sat_s() {
    wah_v128_t operand1 = {{0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x02, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.add_sat_s", i8x16_add_sat_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.add_sat_u
const uint8_t i8x16_add_sat_u_wasm[] = BINARY_OP_WASM(0x70);
void test_i8x16_add_sat_u() {
    wah_v128_t operand1 = {{0x01, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x02, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.add_sat_u", i8x16_add_sat_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.sub
const uint8_t i8x16_sub_wasm[] = BINARY_OP_WASM(0x71);
void test_i8x16_sub() {
    wah_v128_t operand1 = {{0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.sub", i8x16_sub_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.sub_sat_s
const uint8_t i8x16_sub_sat_s_wasm[] = BINARY_OP_WASM(0x72);
void test_i8x16_sub_sat_s() {
    wah_v128_t operand1 = {{0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.sub_sat_s", i8x16_sub_sat_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.sub_sat_u
const uint8_t i8x16_sub_sat_u_wasm[] = BINARY_OP_WASM(0x73);
void test_i8x16_sub_sat_u() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.sub_sat_u", i8x16_sub_sat_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.sub
const uint8_t i16x8_sub_wasm[] = BINARY_OP_WASM(0x91);
void test_i16x8_sub() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 0x0001, 0x0005
    wah_v128_t operand2 = {{0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 0x0002, 0x0003
    wah_v128_t expected = {{0xFF, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 0xFFFF (-1), 0x0002
    if (run_simd_binary_op_test("i16x8.sub", i16x8_sub_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.mul
const uint8_t i32x4_mul_wasm[] = BINARY_OP_WASM(0xb5);
void test_i32x4_mul() {
    wah_v128_t operand1 = {{0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 3
    wah_v128_t operand2 = {{0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 3, 4
    wah_v128_t expected = {{0x06, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 6, 12
    if (run_simd_binary_op_test("i8x16.ge_u", i32x4_mul_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.eq
const uint8_t i16x8_eq_wasm[] = BINARY_OP_WASM(0x2D);
void test_i16x8_eq() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00}};
    if (run_simd_binary_op_test("i16x8.eq", i16x8_eq_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.ne
const uint8_t i16x8_ne_wasm[] = BINARY_OP_WASM(0x2E);
void test_i16x8_ne() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00}};
    wah_v128_t operand2 = {{0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF}};
    if (run_simd_binary_op_test("i16x8.ne", i16x8_ne_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.lt_s
const uint8_t i16x8_lt_s_wasm[] = BINARY_OP_WASM(0x2F);
void test_i16x8_lt_s() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -32768, 32767
    wah_v128_t operand2 = {{0x02, 0x00, 0x01, 0x00, 0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 32767, -32768
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1<2, 2<1(F), -32768<32767, 32767<-32768(F)
    if (run_simd_binary_op_test("i16x8.lt_s", i16x8_lt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.lt_u
const uint8_t i16x8_lt_u_wasm[] = BINARY_OP_WASM(0x30);
void test_i16x8_lt_u() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 32768, 32767
    wah_v128_t operand2 = {{0x02, 0x00, 0x01, 0x00, 0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 32767, 32768
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1<2, 2<1(F), 32768<32767(F), 32767<32768
    if (run_simd_binary_op_test("i16x8.lt_u", i16x8_lt_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.gt_s
const uint8_t i16x8_gt_s_wasm[] = BINARY_OP_WASM(0x31);
void test_i16x8_gt_s() {
    wah_v128_t operand1 = {{0x02, 0x00, 0x01, 0x00, 0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 32767, -32768
    wah_v128_t operand2 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -32768, 32767
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2>1, 1>2(F), 32767>-32768, -32768>32767(F)
    if (run_simd_binary_op_test("i16x8.gt_s", i16x8_gt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.gt_u
const uint8_t i16x8_gt_u_wasm[] = BINARY_OP_WASM(0x32);
void test_i16x8_gt_u() {
    wah_v128_t operand1 = {{0x02, 0x00, 0x01, 0x00, 0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 32767, 32768
    wah_v128_t operand2 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 32768, 32767
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2>1, 1>2(F), 32767>32768(F), 32768>32767
    if (run_simd_binary_op_test("i16x8.gt_u", i16x8_gt_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.le_s
const uint8_t i16x8_le_s_wasm[] = BINARY_OP_WASM(0x33);
void test_i16x8_le_s() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -32768, 32767
    wah_v128_t operand2 = {{0x01, 0x00, 0x01, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, -32768, 32767
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1<=1, 2<=1(F), -32768<=-32768, 32767<=32767, 0<=0 (remaining 4 lanes)
    if (run_simd_binary_op_test("i16x8.le_s", i16x8_le_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.le_u
const uint8_t i16x8_le_u_wasm[] = BINARY_OP_WASM(0x34);
void test_i16x8_le_u() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 32768, 32767
    wah_v128_t operand2 = {{0x01, 0x00, 0x01, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, 32768, 32767
    wah_v128_t expected = {{0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1<=1, 2<=1(F), 32768<=32768, 32767<=32767, 0<=0 (remaining 4 lanes)
    if (run_simd_binary_op_test("i16x8.le_u", i16x8_le_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.ge_s
const uint8_t i16x8_ge_s_wasm[] = BINARY_OP_WASM(0x35);
void test_i16x8_ge_s() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -32768, 32767
    wah_v128_t operand2 = {{0x01, 0x00, 0x01, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, -32768, 32767
    wah_v128_t expected = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1>=1, 2>=1, -32768>=-32768, 32767>=32767, 0>=0 (remaining 4 lanes)
    if (run_simd_binary_op_test("i16x8.ge_s", i16x8_ge_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.ge_u
const uint8_t i16x8_ge_u_wasm[] = BINARY_OP_WASM(0x36);
void test_i16x8_ge_u() {
    wah_v128_t operand1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 32768, 32767
    wah_v128_t operand2 = {{0x01, 0x00, 0x01, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, 32768, 32767
    wah_v128_t expected = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1>=1, 2>=1, 32768>=32768, 32767>=32767, 0>=0 (remaining 4 lanes)
    if (run_simd_binary_op_test("i16x8.ge_u", i16x8_ge_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.eq
const uint8_t i8x16_eq_wasm[] = BINARY_OP_WASM(0x23);
void test_i8x16_eq() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t operand2 = {{0x01, 0x00, 0x03, 0x00, 0x05, 0x00, 0x07, 0x00, 0x09, 0x00, 0x0B, 0x00, 0x0D, 0x00, 0x0F, 0x00}};
    wah_v128_t expected = {{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00}};
    if (run_simd_binary_op_test("i8x16.eq", i8x16_eq_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.ne
const uint8_t i8x16_ne_wasm[] = BINARY_OP_WASM(0x24);
void test_i8x16_ne() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t operand2 = {{0x01, 0x00, 0x03, 0x00, 0x05, 0x00, 0x07, 0x00, 0x09, 0x00, 0x0B, 0x00, 0x0D, 0x00, 0x0F, 0x00}};
    wah_v128_t expected = {{0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF}};
    if (run_simd_binary_op_test("i8x16.ne", i8x16_ne_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.lt_s
const uint8_t i8x16_lt_s_wasm[] = BINARY_OP_WASM(0x25);
void test_i8x16_lt_s() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -128, 127
    wah_v128_t operand2 = {{0x02, 0x01, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 127, -128
    wah_v128_t expected = {{0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1<2, 2<1(F), -128<127, 127<-128(F)
    if (run_simd_binary_op_test("i8x16.lt_s", i8x16_lt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.lt_u
const uint8_t i8x16_lt_u_wasm[] = BINARY_OP_WASM(0x26);
void test_i8x16_lt_u() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 128, 127
    wah_v128_t operand2 = {{0x02, 0x01, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 127, 128
    wah_v128_t expected = {{0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1<2, 2<1(F), 128<127(F), 127<128
    if (run_simd_binary_op_test("i8x16.lt_u", i8x16_lt_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.gt_s
const uint8_t i8x16_gt_s_wasm[] = BINARY_OP_WASM(0x27);
void test_i8x16_gt_s() {
    wah_v128_t operand1 = {{0x02, 0x01, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 127, -128
    wah_v128_t operand2 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -128, 127
    wah_v128_t expected = {{0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2>1, 1>2(F), 127>-128, -128>127(F)
    if (run_simd_binary_op_test("i8x16.gt_s", i8x16_gt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.gt_u
const uint8_t i8x16_gt_u_wasm[] = BINARY_OP_WASM(0x28);
void test_i8x16_gt_u() {
    wah_v128_t operand1 = {{0x02, 0x01, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2, 1, 127, 128
    wah_v128_t operand2 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 128, 127
    wah_v128_t expected = {{0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 2>1, 1>2(F), 127>128(F), 128>127
    if (run_simd_binary_op_test("i8x16.gt_u", i8x16_gt_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.le_s
const uint8_t i8x16_le_s_wasm[] = BINARY_OP_WASM(0x29);
void test_i8x16_le_s() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -128, 127
    wah_v128_t operand2 = {{0x01, 0x01, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, -128, 127
    wah_v128_t expected = {{0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1<=1, 2<=1(F), -128<=-128, 127<=127, 0<=0 (remaining 12 bytes)
    if (run_simd_binary_op_test("i8x16.le_s", i8x16_le_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.le_u
const uint8_t i8x16_le_u_wasm[] = BINARY_OP_WASM(0x2A);
void test_i8x16_le_u() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 128, 127
    wah_v128_t operand2 = {{0x01, 0x01, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, 128, 127
    wah_v128_t expected = {{0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1<=1, 2<=1(F), 128<=128, 127<=127, 0<=0 (remaining 12 bytes)
    if (run_simd_binary_op_test("i8x16.le_u", i8x16_le_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.ge_s
const uint8_t i8x16_ge_s_wasm[] = BINARY_OP_WASM(0x2B);
void test_i8x16_ge_s() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, -128, 127
    wah_v128_t operand2 = {{0x01, 0x01, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, -128, 127
    wah_v128_t expected = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1>=1, 2>=1, -128>=-128, 127>=127, 0>=0 (remaining 12 bytes)
    if (run_simd_binary_op_test("i8x16.ge_s", i8x16_ge_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.ge_u
const uint8_t i8x16_ge_u_wasm[] = BINARY_OP_WASM(0x2C);
void test_i8x16_ge_u() {
    wah_v128_t operand1 = {{0x01, 0x02, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 2, 128, 127
    wah_v128_t operand2 = {{0x01, 0x01, 0x80, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // 1, 1, 128, 127
    wah_v128_t expected = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // 1>=1, 2>=1, 128>=128, 127>=127, 0>=0 (remaining 12 bytes)
    if (run_simd_binary_op_test("i8x16.ge_u", i8x16_ge_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.eq
const uint8_t i32x4_eq_wasm[] = BINARY_OP_WASM(0x37);
void test_i32x4_eq() {
    wah_v128_t operand1 = { .i32 = {1, 2, 3, 4} }, operand2 = { .i32 = {1, 0, 3, 0} }, expected = { .u32 = {~0U, 0, ~0U, 0} };
    if (run_simd_binary_op_test("i32x4.eq", i32x4_eq_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.ne
const uint8_t i32x4_ne_wasm[] = BINARY_OP_WASM(0x38);
void test_i32x4_ne() {
    wah_v128_t operand1 = { .i32 = {1, 2, 3, 4} }, operand2 = { .i32 = {1, 0, 3, 0} }, expected = { .u32 = {0, ~0U, 0, ~0U} };
    if (run_simd_binary_op_test("i32x4.ne", i32x4_ne_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.lt_s
const uint8_t i32x4_lt_s_wasm[] = BINARY_OP_WASM(0x39);
void test_i32x4_lt_s() {
    wah_v128_t operand1 = { .i32 = {1, 2, -2147483648, 2147483647} }, operand2 = { .i32 = {2, 1, 2147483647, -2147483648} };
    wah_v128_t expected = { .u32 = {~0U, 0, ~0U, 0} }; // 1<2, 2<1(F), -2147483648<2147483647, 2147483647<-2147483648(F)
    if (run_simd_binary_op_test("i32x4.lt_s", i32x4_lt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.lt_u
const uint8_t i32x4_lt_u_wasm[] = BINARY_OP_WASM(0x3A);
void test_i32x4_lt_u() {
    wah_v128_t operand1 = { .u32 = {1, 2, 2147483648U, 2147483647U} }, operand2 = { .u32 = {2, 1, 2147483647U, 2147483648U} };
    wah_v128_t expected = { .u32 = {~0U, 0, 0, ~0U} }; // 1<2, 2<1(F), 2147483648<2147483647(F), 2147483647<2147483648
    if (run_simd_binary_op_test("i32x4.lt_u", i32x4_lt_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.gt_s
const uint8_t i32x4_gt_s_wasm[] = BINARY_OP_WASM(0x3B);
void test_i32x4_gt_s() {
    wah_v128_t operand1 = { .i32 = {2, 1, 2147483647, -2147483648} }, operand2 = { .i32 = {1, 2, -2147483648, 2147483647} };
    wah_v128_t expected = { .u32 = {~0U, 0, ~0U, 0} }; // 2>1, 1>2(F), 2147483647>-2147483648, -2147483648>2147483647(F)
    if (run_simd_binary_op_test("i32x4.gt_s", i32x4_gt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.gt_u
const uint8_t i32x4_gt_u_wasm[] = BINARY_OP_WASM(0x3C);
void test_i32x4_gt_u() {
    wah_v128_t operand1 = { .u32 = {2, 1, 2147483647U, 2147483648U} }, operand2 = { .u32 = {1, 2, 2147483648U, 2147483647U} };
    wah_v128_t expected = { .u32 = {~0U, 0, 0, ~0U} }; // 2>1, 1>2(F), 2147483647>2147483648(F), 2147483648>2147483647
    if (run_simd_binary_op_test("i32x4.gt_u", i32x4_gt_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.le_s
const uint8_t i32x4_le_s_wasm[] = BINARY_OP_WASM(0x3D);
void test_i32x4_le_s() {
    wah_v128_t operand1 = { .i32 = {1, 2, -2147483648, 2147483647} }, operand2 = { .i32 = {1, 1, -2147483648, 2147483647} };
    wah_v128_t expected = { .u32 = {~0U, 0, ~0U, ~0U} }; // 1<=1, 2<=1(F), -2147483648<=-2147483648, 2147483647<=2147483647
    if (run_simd_binary_op_test("i32x4.le_s", i32x4_le_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.le_u
const uint8_t i32x4_le_u_wasm[] = BINARY_OP_WASM(0x3E);
void test_i32x4_le_u() {
    wah_v128_t operand1 = { .u32 = {1, 2, 2147483648U, 2147483647U} }, operand2 = { .u32 = {1, 1, 2147483648U, 2147483647U} };
    wah_v128_t expected = { .u32 = {~0U, 0, ~0U, ~0U} }; // 1<=1, 2<=1(F), 2147483648<=2147483648, 2147483647<=2147483647
    if (run_simd_binary_op_test("i32x4.le_u", i32x4_le_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.ge_s
const uint8_t i32x4_ge_s_wasm[] = BINARY_OP_WASM(0x3F);
void test_i32x4_ge_s() {
    wah_v128_t operand1 = { .i32 = {1, 2, -2147483648, 2147483647} }, operand2 = { .i32 = {1, 1, -2147483648, 2147483647} };
    wah_v128_t expected = { .u32 = {~0U, ~0U, ~0U, ~0U} }; // 1>=1, 2>=1, -2147483648>=-2147483648, 2147483647>=2147483647
    if (run_simd_binary_op_test("i32x4.ge_s", i32x4_ge_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.ge_u
const uint8_t i32x4_ge_u_wasm[] = BINARY_OP_WASM(0x40);
void test_i32x4_ge_u() {
    wah_v128_t operand1 = { .u32 = {1, 2, 2147483648U, 2147483647U} }, operand2 = { .u32 = {1, 1, 2147483648U, 2147483647U} };
    wah_v128_t expected = { .u32 = {~0U, ~0U, ~0U, ~0U} }; // 1>=1, 2>=1, 2147483648>=2147483648, 2147483647>=2147483647
    if (run_simd_binary_op_test("i32x4.ge_u", i32x4_ge_u_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.eq
const uint8_t i64x2_eq_wasm[] = BINARY_OP_WASM(0xD6);
void test_i64x2_eq() {
    wah_v128_t operand1 = { .i64 = {1, 2} }, operand2 = { .i64 = {1, 0} }, expected = { .u64 = {~0ULL, 0} };
    if (run_simd_binary_op_test("i64x2.eq", i64x2_eq_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.ne
const uint8_t i64x2_ne_wasm[] = BINARY_OP_WASM(0xD7);
void test_i64x2_ne() {
    wah_v128_t operand1 = { .i64 = {1, 2} }, operand2 = { .i64 = {1, 0} }, expected = { .u64 = {0, ~0ULL} };
    if (run_simd_binary_op_test("i64x2.ne", i64x2_ne_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.lt_s
const uint8_t i64x2_lt_s_wasm[] = BINARY_OP_WASM(0xD8);
void test_i64x2_lt_s() {
    wah_v128_t operand1 = { .i64 = {1, 2} }, operand2 = { .i64 = {2, 1} }, expected = { .u64 = {~0ULL, 0} }; // 1<2, 2<1(F)
    if (run_simd_binary_op_test("i64x2.lt_s", i64x2_lt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.gt_s
const uint8_t i64x2_gt_s_wasm[] = BINARY_OP_WASM(0xD9);
void test_i64x2_gt_s() {
    wah_v128_t operand1 = { .i64 = {2, 1} }, operand2 = { .i64 = {1, 2} }, expected = { .u64 = {~0ULL, 0} }; // 2>1, 1>2(F)
    if (run_simd_binary_op_test("i64x2.gt_s", i64x2_gt_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.le_s
const uint8_t i64x2_le_s_wasm[] = BINARY_OP_WASM(0xDA);
void test_i64x2_le_s() {
    wah_v128_t operand1 = { .i64 = {1, 2} }, operand2 = { .i64 = {1, 2} }, expected = { .u64 = {~0ULL, ~0ULL} }; // 1<=1, 2<=2
    if (run_simd_binary_op_test("i64x2.le_s", i64x2_le_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.ge_s
const uint8_t i64x2_ge_s_wasm[] = BINARY_OP_WASM(0xDB);
void test_i64x2_ge_s() {
    wah_v128_t operand1 = { .i64 = {1, 2} }, operand2 = { .i64 = {1, 2} }, expected = { .u64 = {~0ULL, ~0ULL} }; // 1>=1, 2>=2
    if (run_simd_binary_op_test("i64x2.ge_s", i64x2_ge_s_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.eq
const uint8_t f32x4_eq_wasm[] = BINARY_OP_WASM(0x41);
void test_f32x4_eq() {
    wah_v128_t operand1 = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} }, operand2 = { .f32 = {1.0f, 0.0f, 3.0f, 0.0f} };
    wah_v128_t expected = { .u32 = {~0U, 0, ~0U, 0} };
    if (run_simd_binary_op_test("f32x4.eq", f32x4_eq_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.ne
const uint8_t f32x4_ne_wasm[] = BINARY_OP_WASM(0x42);
void test_f32x4_ne() {
    wah_v128_t operand1 = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} }, operand2 = { .f32 = {1.0f, 0.0f, 3.0f, 0.0f} };
    wah_v128_t expected = { .u32 = {0, ~0U, 0, ~0U} };
    if (run_simd_binary_op_test("f32x4.ne", f32x4_ne_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.lt
const uint8_t f32x4_lt_wasm[] = BINARY_OP_WASM(0x43);
void test_f32x4_lt() {
    wah_v128_t operand1 = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} }, operand2 = { .f32 = {2.0f, 1.0f, 3.0f, 5.0f} };
    wah_v128_t expected = { .u32 = {~0U, 0, 0, ~0U} }; // 1<2, 2<1(F), 3<3(F), 4<5
    if (run_simd_binary_op_test("f32x4.lt", f32x4_lt_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.gt
const uint8_t f32x4_gt_wasm[] = BINARY_OP_WASM(0x44);
void test_f32x4_gt() {
    wah_v128_t operand1 = { .f32 = {2.0f, 1.0f, 3.0f, 5.0f} }, operand2 = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} };
    wah_v128_t expected = { .u32 = {~0U, 0, 0, ~0U} }; // 2>1, 1>2(F), 3>3(F), 5>4
    if (run_simd_binary_op_test("f32x4.gt", f32x4_gt_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.le
const uint8_t f32x4_le_wasm[] = BINARY_OP_WASM(0x45);
void test_f32x4_le() {
    wah_v128_t operand1 = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} }, operand2 = { .f32 = {1.0f, 1.0f, 3.0f, 5.0f} };
    wah_v128_t expected = { .u32 = {~0U, 0, ~0U, ~0U} }; // 1<=1, 2<=1(F), 3<=3, 4<=5
    if (run_simd_binary_op_test("f32x4.le", f32x4_le_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.ge
const uint8_t f32x4_ge_wasm[] = BINARY_OP_WASM(0x46);
void test_f32x4_ge() {
    wah_v128_t operand1 = { .f32 = {1.0f, 2.0f, 3.0f, 5.0f} }, operand2 = { .f32 = {1.0f, 1.0f, 3.0f, 4.0f} };
    wah_v128_t expected = { .u32 = {~0U, ~0U, ~0U, ~0U} }; // 1>=1, 2>=1, 3>=3, 5>=4
    if (run_simd_binary_op_test("f32x4.ge", f32x4_ge_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.eq
const uint8_t f64x2_eq_wasm[] = BINARY_OP_WASM(0x47);
void test_f64x2_eq() {
    wah_v128_t operand1 = { .f64 = {1.0, 2.0} }, operand2 = { .f64 = {1.0, 0.0} }, expected = { .u64 = {~0ULL, 0} };
    if (run_simd_binary_op_test("f64x2.eq", f64x2_eq_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.ne
const uint8_t f64x2_ne_wasm[] = BINARY_OP_WASM(0x48);
void test_f64x2_ne() {
    wah_v128_t operand1 = { .f64 = {1.0, 2.0} }, operand2 = { .f64 = {1.0, 0.0} }, expected = { .u64 = {0, ~0ULL} };
    if (run_simd_binary_op_test("f64x2.ne", f64x2_ne_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.lt
const uint8_t f64x2_lt_wasm[] = BINARY_OP_WASM(0x49);
void test_f64x2_lt() {
    wah_v128_t operand1 = { .f64 = {1.0, 2.0} }, operand2 = { .f64 = {2.0, 1.0} }, expected = { .u64 = {~0ULL, 0} }; // 1<2, 2<1(F)
    if (run_simd_binary_op_test("f64x2.lt", f64x2_lt_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.gt
const uint8_t f64x2_gt_wasm[] = BINARY_OP_WASM(0x4A);
void test_f64x2_gt() {
    wah_v128_t operand1 = { .f64 = {2.0, 1.0} }, operand2 = { .f64 = {1.0, 2.0} }, expected = { .u64 = {~0ULL, 0} }; // 2>1, 1>2(F)
    if (run_simd_binary_op_test("f64x2.gt", f64x2_gt_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.le
const uint8_t f64x2_le_wasm[] = BINARY_OP_WASM(0x4B);
void test_f64x2_le() {
    wah_v128_t operand1 = { .f64 = {1.0, 2.0} }, operand2 = { .f64 = {1.0, 1.0} }, expected = { .u64 = {~0ULL, 0} }; // 1<=1, 2<=1(F)
    if (run_simd_binary_op_test("f64x2.le", f64x2_le_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.ge
const uint8_t f64x2_ge_wasm[] = BINARY_OP_WASM(0x4C);
void test_f64x2_ge() {
    wah_v128_t operand1 = { .f64 = {1.0, 2.0} }, operand2 = { .f64 = {1.0, 1.0} }, expected = { .u64 = {~0ULL, ~0ULL} }; // 1>=1, 2>=1
    if (run_simd_binary_op_test("f64x2.ge", f64x2_ge_wasm, &operand1, &operand2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.shuffle
const uint8_t i8x16_shuffle_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x3a, 0x01, 0x38, 0x00,
        0xfd, 0x0c, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // v128.const operand1...
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0xfd, 0x0c, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, // v128.const operand2...
                    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0xfd, 0x0d, 0x00, 0x01, 0x10, 0x11, 0x02, 0x03, 0x12, 0x13, // i8x16.shuffle mask...
                    0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x16, 0x17,
        0x0b,
};
void test_i8x16_shuffle() {
    // mask: 0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23
    // result: vec1[0], vec1[1], vec2[0], vec2[1], vec1[2], vec1[3], vec2[2], vec2[3], ...
    wah_v128_t expected = {{0x00, 0x01, 0x10, 0x11, 0x02, 0x03, 0x12, 0x13, 0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x16, 0x17}};
    if (run_simd_shuffle_swizzle_test("i8x16.shuffle", i8x16_shuffle_wasm, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.extract_lane_s
const uint8_t i8x16_extract_lane_s_wasm[] = EXTRACT_LANE_WASM(0x15, 0x01); // lane 1
void test_i8x16_extract_lane_s() {
    wah_v128_t operand = {{0x00, 0x81, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}};
    wah_value_t expected = {.i32 = -127}; // operand.i8[1] is 0x81 = -127
    if (run_simd_extract_lane_test("i8x16.extract_lane_s", i8x16_extract_lane_s_wasm, &operand, &expected, WAH_TYPE_I32) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.replace_lane
const uint8_t i8x16_replace_lane_wasm[] = REPLACE_LANE_WASM(0x17, 0x01); // lane 1
void test_i8x16_replace_lane() {
    wah_v128_t operand_v128 = {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}};
    wah_value_t operand_scalar = {.i32 = 0xAA}; // Replace with 0xAA
    wah_v128_t expected = {{0x00, 0xAA, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}};
    if (run_simd_replace_lane_test("i8x16.replace_lane", i8x16_replace_lane_wasm, &operand_v128, &operand_scalar, WAH_TYPE_I32, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.swizzle
const uint8_t i8x16_swizzle_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x2a, 0x01, 0x28, 0x00,
        0xfd, 0x0c, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, // v128.const data...
                    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xfd, 0x0c, 0x00, 0x02, 0x10, 0x12, 0x01, 0x03, 0x11, 0x13, // v128.const mask...
                    0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x16, 0x17,
        0xfd, 0x0e, // i8x16.swizzle
        0x0b,
};
void test_i8x16_swizzle() {
    // mask: 0, 2, 16, 18, 1, 3, 17, 19, 4, 5, 20, 21, 6, 7, 22, 23
    // result: data[0], data[2], 0, 0, data[1], data[3], 0, 0, ...
    wah_v128_t expected = {{0xA0, 0xA2, 0x00, 0x00, 0xA1, 0xA3, 0x00, 0x00, 0xA4, 0xA5, 0x00, 0x00, 0xA6, 0xA7, 0x00, 0x00}};
    if (run_simd_shuffle_swizzle_test("i8x16.swizzle", i8x16_swizzle_wasm, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i8x16.splat
const uint8_t i8x16_splat_wasm[] = SPLAT_WASM(0x0F);
void test_i8x16_splat() {
    wah_value_t operand_scalar = {.i32 = 0xBE};
    wah_v128_t expected = {{0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE}};
    if (run_simd_splat_test("i8x16.splat", i8x16_splat_wasm, &operand_scalar, WAH_TYPE_I32, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i16x8.splat
const uint8_t i16x8_splat_wasm[] = SPLAT_WASM(0x10);
void test_i16x8_splat() {
    wah_value_t operand_scalar = {.i32 = 0xBEAF};
    wah_v128_t expected = {{0xAF, 0xBE, 0xAF, 0xBE, 0xAF, 0xBE, 0xAF, 0xBE, 0xAF, 0xBE, 0xAF, 0xBE, 0xAF, 0xBE, 0xAF, 0xBE}};
    if (run_simd_splat_test("i16x8.splat", i16x8_splat_wasm, &operand_scalar, WAH_TYPE_I32, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.splat
const uint8_t i32x4_splat_wasm[] = SPLAT_WASM(0x11);
void test_i32x4_splat() {
    wah_value_t operand_scalar = {.i32 = 0xDEADBEEF};
    wah_v128_t expected = {{0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE}};
    if (run_simd_splat_test("i32x4.splat", i32x4_splat_wasm, &operand_scalar, WAH_TYPE_I32, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i64x2.splat
const uint8_t i64x2_splat_wasm[] = SPLAT_WASM(0x12);
void test_i64x2_splat() {
    wah_value_t operand_scalar = {.i64 = 0xDEADBEEFCAFEBABEULL};
    wah_v128_t expected = {{0xBE, 0xBA, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE, 0xBE, 0xBA, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE}};
    if (run_simd_splat_test("i64x2.splat", i64x2_splat_wasm, &operand_scalar, WAH_TYPE_I64, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.splat
const uint8_t f32x4_splat_wasm[] = SPLAT_WASM(0x13);
void test_f32x4_splat() {
    wah_value_t operand_scalar = {.f32 = 123.456f};
    wah_v128_t expected;
    for (int i = 0; i < 4; ++i) expected.f32[i] = 123.456f;
    if (run_simd_splat_test("f32x4.splat", f32x4_splat_wasm, &operand_scalar, WAH_TYPE_F32, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.splat
const uint8_t f64x2_splat_wasm[] = SPLAT_WASM(0x14);
void test_f64x2_splat() {
    wah_value_t operand_scalar = {.f64 = 123.456789};
    wah_v128_t expected;
    for (int i = 0; i < 2; ++i) expected.f64[i] = 123.456789;
    if (run_simd_splat_test("f64x2.splat", f64x2_splat_wasm, &operand_scalar, WAH_TYPE_F64, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.trunc_sat_f32x4_s
const uint8_t i32x4_trunc_sat_f32x4_s_wasm[] = UNARY_OP_WASM(0xF8);
void test_i32x4_trunc_sat_f32x4_s() {
    wah_v128_t operand = { .f32 = {1.5f, -2.5f, 2147483647.0f, -2147483648.0f} };
    wah_v128_t expected = { .i32 = {1, -2, 2147483647, -2147483648} };
    if (run_simd_unary_op_test("i32x4.trunc_sat_f32x4_s", i32x4_trunc_sat_f32x4_s_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.trunc_sat_f32x4_u
const uint8_t i32x4_trunc_sat_f32x4_u_wasm[] = UNARY_OP_WASM(0xF9);
void test_i32x4_trunc_sat_f32x4_u() {
    wah_v128_t operand = { .f32 = {1.5f, -2.5f, 4294967295.0f, 0.0f} };
    wah_v128_t expected = { .u32 = {1, 0, 4294967295U, 0} };
    if (run_simd_unary_op_test("i32x4.trunc_sat_f32x4_u", i32x4_trunc_sat_f32x4_u_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.convert_i32x4_s
const uint8_t f32x4_convert_i32x4_s_wasm[] = UNARY_OP_WASM(0xFA);
void test_f32x4_convert_i32x4_s() {
    wah_v128_t operand = { .i32 = {1, -2, 3, -4} };
    wah_v128_t expected = { .f32 = {1.0f, -2.0f, 3.0f, -4.0f} };
    if (run_simd_unary_op_test("f32x4.convert_i32x4_s", f32x4_convert_i32x4_s_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.convert_i32x4_u
const uint8_t f32x4_convert_i32x4_u_wasm[] = UNARY_OP_WASM(0xFB);
void test_f32x4_convert_i32x4_u() {
    wah_v128_t operand = { .u32 = {1, 2, 3, 4} };
    wah_v128_t expected = { .f32 = {1.0f, 2.0f, 3.0f, 4.0f} };
    if (run_simd_unary_op_test("f32x4.convert_i32x4_u", f32x4_convert_i32x4_u_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.trunc_sat_f64x2_s_zero
const uint8_t i32x4_trunc_sat_f64x2_s_zero_wasm[] = UNARY_OP_WASM(0xFC);
void test_i32x4_trunc_sat_f64x2_s_zero() {
    wah_v128_t operand = { .f64 = {1.5, -2.5} };
    wah_v128_t expected = { .i32 = {1, -2, 0, 0} };
    if (run_simd_unary_op_test("i32x4.trunc_sat_f64x2_s_zero", i32x4_trunc_sat_f64x2_s_zero_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for i32x4.trunc_sat_f64x2_u_zero
const uint8_t i32x4_trunc_sat_f64x2_u_zero_wasm[] = UNARY_OP_WASM(0xFD);
void test_i32x4_trunc_sat_f64x2_u_zero() {
    wah_v128_t operand = { .f64 = {1.5, -2.5} };
    wah_v128_t expected = { .u32 = {1, 0, 0, 0} };
    if (run_simd_unary_op_test("i32x4.trunc_sat_f64x2_u_zero", i32x4_trunc_sat_f64x2_u_zero_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.convert_low_i32x4_s
const uint8_t f64x2_convert_low_i32x4_s_wasm[] = UNARY_OP_WASM(0xFE);
void test_f64x2_convert_low_i32x4_s() {
    wah_v128_t operand = { .i32 = {1, -2, 3, -4} };
    wah_v128_t expected = { .f64 = {1.0, -2.0} };
    if (run_simd_unary_op_test("f64x2.convert_low_i32x4_s", f64x2_convert_low_i32x4_s_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.convert_low_i32x4_u
const uint8_t f64x2_convert_low_i32x4_u_wasm[] = UNARY_OP_WASM(0xFF);
void test_f64x2_convert_low_i32x4_u() {
    wah_v128_t operand = { .u32 = {1, 2, 3, 4} };
    wah_v128_t expected = { .f64 = {1.0, 2.0} };
    if (run_simd_unary_op_test("f64x2.convert_low_i32x4_u", f64x2_convert_low_i32x4_u_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f32x4.demote_f64x2_zero
const uint8_t f32x4_demote_f64x2_zero_wasm[] = UNARY_OP_WASM(0x5E);
void test_f32x4_demote_f64x2_zero() {
    wah_v128_t operand = { .f64 = {1.5, 2.5} };
    wah_v128_t expected = { .f32 = {1.5f, 2.5f, 0.0f, 0.0f} };
    if (run_simd_unary_op_test("f32x4.demote_f64x2_zero", f32x4_demote_f64x2_zero_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for f64x2.promote_low_f32x4
const uint8_t f64x2_promote_low_f32x4_wasm[] = UNARY_OP_WASM(0x5F);
void test_f64x2_promote_low_f32x4() {
    wah_v128_t operand = { .f32 = {1.5f, 2.5f, 3.5f, 4.5f} };
    wah_v128_t expected = { .f64 = {1.5, 2.5} };
    if (run_simd_unary_op_test("f64x2.promote_low_f32x4", f64x2_promote_low_f32x4_wasm, &operand, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I8X16_ABS
const uint8_t i8x16_abs_wasm[] = V128_UNARY_OP_WASM(0x60);
void test_i8x16_abs() {
    wah_v128_t v = {{0x01, 0xFF, 0x00, 0x7F, 0x80, 0x0A, 0xF6, 0x00, 0x01, 0xFF, 0x00, 0x7F, 0x80, 0x0A, 0xF6, 0x00}};
    wah_v128_t expected = {{0x01, 0x01, 0x00, 0x7F, 0x80, 0x0A, 0x0A, 0x00, 0x01, 0x01, 0x00, 0x7F, 0x80, 0x0A, 0x0A, 0x00}};
    if (run_simd_unary_op_test("i8x16.abs", i8x16_abs_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I8X16_SHL
const uint8_t i8x16_shl_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x22, 0x01, 0x20, 0x00,
        0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // v128.const ...
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, // (scalar)
        0xfd, 0x6b, // i8x16.shl
        0x0b,
};
void test_i8x16_shl() {
    wah_v128_t v = {{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}};
    wah_v128_t expected = {{0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00}};
    wah_value_t shift_amount = {.i32 = 1};
    if (run_simd_binary_op_i32_shift_test("i8x16.shl", i8x16_shl_wasm, &v, &shift_amount, WAH_TYPE_I32, &expected) != WAH_OK) { exit(1); }

    v = (wah_v128_t){{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}};
    expected = (wah_v128_t){{0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}};
    shift_amount = (wah_value_t){.i32 = 2};
    if (run_simd_binary_op_i32_shift_test("i8x16.shl", i8x16_shl_wasm, &v, &shift_amount, WAH_TYPE_I32, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I8X16_MIN_S
const uint8_t i8x16_min_s_wasm[] = V128_BINARY_OP_WASM(0x76);
void test_i8x16_min_s() {
    wah_v128_t v1 = {{0x01, 0x05, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t v2 = {{0x02, 0x03, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x01, 0x03, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00}};
    if (run_simd_binary_op_test("i8x16.min_s", i8x16_min_s_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I8X16_AVGR_U
const uint8_t i8x16_avgr_u_wasm[] = V128_BINARY_OP_WASM(0x7B);
void test_i8x16_avgr_u() {
    wah_v128_t v1 = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t v2 = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    wah_v128_t expected = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    if (run_simd_binary_op_test("i8x16.avgr_u", i8x16_avgr_u_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }

    v1 = (wah_v128_t){{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    v2 = (wah_v128_t){{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}};
    expected = (wah_v128_t){{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}};
    if (run_simd_binary_op_test("i8x16.avgr_u", i8x16_avgr_u_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I16X8_EXTEND_LOW_I8X16_S
const uint8_t i16x8_extend_low_i8x16_s_wasm[] = V128_UNARY_OP_WASM(0x87);
void test_i16x8_extend_low_i8x16_s() {
    wah_v128_t v = {{0x01, 0xFF, 0x00, 0x7F, 0x80, 0x0A, 0xF6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    wah_v128_t expected = {{0x01, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x7F, 0x00, 0x80, 0xFF, 0x0A, 0x00, 0xF6, 0xFF, 0x00, 0x00}}; // i16 values
    if (run_simd_unary_op_test("i16x8.extend_low_i8x16_s", i16x8_extend_low_i8x16_s_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I16X8_NARROW_I32X4_S
const uint8_t i16x8_narrow_i32x4_s_wasm[] = V128_BINARY_OP_WASM(0x85);
void test_i16x8_narrow_i32x4_s() {
    wah_v128_t v1 = {{0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // i32: 1, -1
    wah_v128_t v2 = {{0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // i32: -32768, 65535 (saturates to 32767)
    wah_v128_t expected = {{0x01, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00}}; // i16: 1, -1, 0, 0, -32768, 32767, 0, 0
    if (run_simd_binary_op_test("i16x8.narrow_i32x4_s", i16x8_narrow_i32x4_s_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I16X8_Q15MULR_SAT_S
const uint8_t i16x8_q15mulr_sat_s_wasm[] = V128_BINARY_OP_WASM(0x82);
void test_i16x8_q15mulr_sat_s() {
    wah_v128_t v1 = {{0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40}}; // 0.5 in Q15
    wah_v128_t v2 = {{0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40}}; // 0.5 in Q15
    wah_v128_t expected = {{0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20}}; // 0.25 in Q15
    if (run_simd_binary_op_test("i16x8.q15mulr_sat_s", i16x8_q15mulr_sat_s_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }

    v1 = (wah_v128_t){{0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F}}; // 1.0 in Q15
    v2 = (wah_v128_t){{0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F}}; // 1.0 in Q15
    expected = (wah_v128_t){{0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F}}; // 32766 in Q15 (saturated)
    if (run_simd_binary_op_test("i16x8.q15mulr_sat_s", i16x8_q15mulr_sat_s_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I32X4_DOT_I16X8_S
const uint8_t i32x4_dot_i16x8_s_wasm[] = V128_BINARY_OP_WASM(0xBA);
void test_i32x4_dot_i16x8_s() {
    wah_v128_t v1 = {{0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00}}; // i16: 1, 2, 3, 4, 5, 6, 7, 8
    wah_v128_t v2 = {{0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x00}}; // i16: 2, 3, 4, 5, 6, 7, 8, 9
    wah_v128_t expected = {{0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00}}; // i32: 8, 32, 72, 128
    if (run_simd_binary_op_test("i32x4.dot_i16x8_s", i32x4_dot_i16x8_s_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I32X4_EXTMUL_LOW_I16X8_S
const uint8_t i32x4_extmul_low_i16x8_s_wasm[] = V128_BINARY_OP_WASM(0xBC);
void test_i32x4_extmul_low_i16x8_s() {
    wah_v128_t v1 = {{0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // i16: 1, 2
    wah_v128_t v2 = {{0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // i16: 2, 3
    wah_v128_t expected = {{0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // i32: 2, 6
    if (run_simd_binary_op_test("i32x4.extmul_low_i16x8_s", i32x4_extmul_low_i16x8_s_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I64X2_ABS
const uint8_t i64x2_abs_wasm[] = V128_UNARY_OP_WASM(0xC0);
void test_i64x2_abs() {
    wah_v128_t v = {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // i64: 1, -1
    wah_v128_t expected = {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // i64: 1, 1
    if (run_simd_unary_op_test("i64x2.abs", i64x2_abs_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for I64X2_EXTEND_HIGH_I32X4_U
const uint8_t i64x2_extend_high_i32x4_u_wasm[] = V128_UNARY_OP_WASM(0xCA);
void test_i64x2_extend_high_i32x4_u() {
    wah_v128_t v = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00}}; // u32: ?, ?, 0xFFFFFFFF, 0
    wah_v128_t expected = {{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // u64: 0xFFFFFFFF, 0
    if (run_simd_unary_op_test("i64x2.extend_high_i32x4_u", i64x2_extend_high_i32x4_u_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for F32X4_ABS
const uint8_t f32x4_abs_wasm[] = V128_UNARY_OP_WASM(0xE0);
void test_f32x4_abs() {
    wah_v128_t v = {{0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // f32: 1.0, -1.0, 0.0
    wah_v128_t expected = {{0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // f32: 1.0, 1.0, 0.0
    if (run_simd_unary_op_test("f32x4.abs", f32x4_abs_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for F32X4_CEIL
const uint8_t f32x4_ceil_wasm[] = V128_UNARY_OP_WASM(0x67);
void test_f32x4_ceil() {
    wah_v128_t v = {{0x00, 0x00, 0x20, 0x40, 0x00, 0x00, 0x20, 0xC0, 0xCD, 0xCC, 0xCC, 0x3F, 0x00, 0x00, 0x00, 0x00}}; // f32: 2.5, -2.5, 1.6
    wah_v128_t expected = {{0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00}}; // f32: 3.0, -2.0, 2.0
    if (run_simd_unary_op_test("f32x4.ceil", f32x4_ceil_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for F32X4_MIN
const uint8_t f32x4_min_wasm[] = V128_BINARY_OP_WASM(0xE8);
void test_f32x4_min() {
    wah_v128_t v1 = {{0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00}}; // f32: 1.0, 0.0, 1.0
    wah_v128_t v2 = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // f32: 0.0, 1.0, 0.0
    wah_v128_t expected = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // f32: 0.0, 0.0, 0.0
    if (run_simd_binary_op_test("f32x4.min", f32x4_min_wasm, &v1, &v2, &expected) != WAH_OK) { exit(1); }
}

// Test cases for F64X2_NEG
const uint8_t f64x2_neg_wasm[] = V128_UNARY_OP_WASM(0xED);
void test_f64x2_neg() {
    wah_v128_t v = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xBF}}; // f64: 1.0, -1.0
    wah_v128_t expected = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F}}; // f64: -1.0, 1.0
    if (run_simd_unary_op_test("f64x2.neg", f64x2_neg_wasm, &v, &expected) != WAH_OK) { exit(1); }
}

// Test cases for F64X2_SQRT
const uint8_t f64x2_sqrt_wasm[] = V128_UNARY_OP_WASM(0xEF);
void test_f64x2_sqrt() {
    wah_v128_t v = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // f64: 4.0, 0.0
    wah_v128_t expected = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // f64: 2.0, 0.0
    if (run_simd_unary_op_test("f64x2.sqrt", f64x2_sqrt_wasm, &v, &expected) != WAH_OK) { exit(1); }
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
    test_i16x8_eq();
    test_i16x8_ne();
    test_i16x8_lt_s();
    test_i16x8_lt_u();
    test_i16x8_gt_s();
    test_i16x8_gt_u();
    test_i16x8_le_s();
    test_i16x8_le_u();
    test_i16x8_ge_s();
    test_i16x8_ge_u();
    test_i8x16_eq();
    test_i8x16_ne();
    test_i8x16_lt_s();
    test_i8x16_lt_u();
    test_i8x16_gt_s();
    test_i8x16_gt_u();
    test_i8x16_le_s();
    test_i8x16_le_u();
    test_i8x16_ge_s();
    test_i8x16_ge_u();
    test_i32x4_eq();
    test_i32x4_ne();
    test_i32x4_lt_s();
    test_i32x4_lt_u();
    test_i32x4_gt_s();
    test_i32x4_gt_u();
    test_i32x4_le_s();
    test_i32x4_le_u();
    test_i32x4_ge_s();
    test_i32x4_ge_u();
    test_i64x2_eq();
    test_i64x2_ne();
    test_i64x2_lt_s();
    test_i64x2_gt_s();
    test_i64x2_le_s();
    test_i64x2_ge_s();
    test_f32x4_eq();
    test_f32x4_ne();
    test_f32x4_lt();
    test_f32x4_gt();
    test_f32x4_le();
    test_f32x4_ge();
    test_f64x2_eq();
    test_f64x2_ne();
    test_f64x2_lt();
    test_f64x2_gt();
    test_f64x2_le();
    test_f64x2_ge();

    test_i8x16_shuffle();
    test_i8x16_extract_lane_s();
    test_i8x16_replace_lane();
    test_i8x16_swizzle();
    test_i8x16_splat();
    test_i16x8_splat();
    test_i32x4_splat();
    test_i64x2_splat();
    test_f32x4_splat();
    test_f64x2_splat();

    test_v128_bitselect();
    test_v128_any_true();
    test_i32x4_trunc_sat_f32x4_s();
    test_i32x4_trunc_sat_f32x4_u();
    test_f32x4_convert_i32x4_s();
    test_f32x4_convert_i32x4_u();
    test_i32x4_trunc_sat_f64x2_s_zero();
    test_i32x4_trunc_sat_f64x2_u_zero();
    test_f64x2_convert_low_i32x4_s();
    test_f64x2_convert_low_i32x4_u();
    test_f32x4_demote_f64x2_zero();
    test_f64x2_promote_low_f32x4();

    test_i8x16_abs();
    test_i8x16_shl();
    test_i8x16_min_s();
    test_i8x16_avgr_u();
    test_i16x8_extend_low_i8x16_s();
    test_i16x8_narrow_i32x4_s();
    test_i16x8_q15mulr_sat_s();
    test_i32x4_dot_i16x8_s();
    test_i32x4_extmul_low_i16x8_s();
    test_i64x2_abs();
    test_i64x2_extend_high_i32x4_u();
    test_f32x4_abs();
    test_f32x4_ceil();
    test_f32x4_min();
    test_f64x2_neg();
    test_f64x2_sqrt();

    return 0;
}
