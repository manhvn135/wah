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

int main() {
    test_v128_const();
    test_v128_load_store();
    return 0;
}
