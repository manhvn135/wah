#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WAH_IMPLEMENTATION
#include "wah.h"

// This WASM module contains a function type with zero parameters and zero results.
// This is used to test that the parser correctly handles zero-count vectors,
// specifically avoiding `malloc(0)` which has implementation-defined behavior.
static const uint8_t wasm_binary_zero_params[] = {
    // Magic + Version
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

    // Type Section: 1 type, () -> ()
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // Function Section: 1 function, type 0
    0x03, 0x02, 0x01, 0x00,

    // Code Section: 1 function body
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b, // size 2, 0 locals, end
};

// This WASM module has the Memory Section before the Table Section (invalid order)
static const uint8_t wasm_binary_invalid_section_order_mem_table[] = {
    // Magic + Version
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

    // Type Section
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // Memory Section (ID 5) - Should come after Table Section (ID 4)
    0x05, 0x04, // Section ID 5, size 4
    0x01,       // 1 memory
    0x00,       // flags (fixed size)
    0x01,       // min_pages = 1

    // Table Section (ID 4)
    0x04, 0x04, // Section ID 4, size 4
    0x01,       // 1 table
    0x70,       // elem_type = funcref
    0x00,       // flags (fixed size)
    0x01,       // min_elements = 1

    // Function Section
    0x03, 0x02, 0x01, 0x00,

    // Code Section
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
};

int main(void) {
    wah_module_t module;
    wah_error_t err;

    printf("--- Running Parser Correctness Test ---\n");

    printf("Testing function type with 0 params and 0 results...\n");
    err = wah_parse_module(wasm_binary_zero_params, sizeof(wasm_binary_zero_params), &module);

    assert(err == WAH_OK);
    if (err == WAH_OK) {
        printf("  - PASSED: Module with zero-count types parsed successfully.\n");
        wah_free_module(&module);
    } else {
        printf("  - FAILED: Module parsing failed with error: %s\n", wah_strerror(err));
        return 1;
    }

    printf("Testing invalid section order (Memory before Table)...\n");
    err = wah_parse_module(wasm_binary_invalid_section_order_mem_table, sizeof(wasm_binary_invalid_section_order_mem_table), &module);
    assert(err == WAH_ERROR_VALIDATION_FAILED);
    if (err == WAH_ERROR_VALIDATION_FAILED) {
        printf("  - PASSED: Module with invalid section order correctly failed validation.\n");
    } else {
        printf("  - FAILED: Module with invalid section order did not fail as expected. Error: %s\n", wah_strerror(err));
        return 1;
    }

    printf("--- Parser Correctness Test Finished ---\n");

    return 0;
}
