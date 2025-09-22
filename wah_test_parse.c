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

int test_invalid_element_segment_func_idx() {
    printf("Running test_invalid_element_segment_func_idx...\n");

    // Minimal WASM binary with an element section referencing an out-of-bounds function index
    // (module
    //   (type $0 (func))
    //   (func $f0 (type $0) nop)
    //   (table $0 1 funcref)
    //   (elem $0 (i32.const 0) $f0 $f1) ;; $f1 does not exist, func_idx 1 is out of bounds
    // )
    const uint8_t wasm_binary[] = {
        0x00, 0x61, 0x73, 0x6D, // Magic
        0x01, 0x00, 0x00, 0x00, // Version

        // Type Section (id 1)
        0x01, // Section ID
        0x04, // Section Size
        0x01, // Count of types
        0x60, // Func type
        0x00, // Param count
        0x00, // Result count

        // Function Section (id 3)
        0x03, // Section ID
        0x02, // Section Size
        0x01, // Count of functions
        0x00, // Type index 0 for function 0

        // Table Section (id 4)
        0x04, // Section ID
        0x04, // Section Size
        0x01, // Count of tables
        0x70, // funcref type
        0x00, // flags: not resizable
        0x01, // min elements: 1

        // Element Section (id 9)
        0x09, // Section ID
        0x08, // Section Size
        0x01, // Count of element segments
        0x00, // Table index 0
        0x41, // opcode i32.const
        0x00, // value 0
        0x0B, // opcode end
        0x02, // num_elems: 2
        0x00, // func_idx 0 (valid)
        0x01, // func_idx 1 (INVALID - only 1 function exists)

        // Code Section (id 10)
        0x0A, // Section ID
        0x05, // Section Size
        0x01, // Count of code bodies
        0x02, // Code body size for function 0
        0x00, // Num locals
        0x01, // Nop opcode
        0x0B, // End opcode
    };

    wah_module_t module;
    memset(&module, 0, sizeof(wah_module_t));

    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);

    // Expecting validation failure due to out-of-bounds function index
    if (err != WAH_ERROR_VALIDATION_FAILED) {
        fprintf(stderr, "Assertion failed: Expected WAH_ERROR_VALIDATION_FAILED for invalid function index in element segment, got %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 1;
    }
    printf("  - PASSED: Invalid element segment function index correctly failed validation.\n");

    wah_free_module(&module);
    return 0;
}

int main(void) {
    int result = 0;

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
    result |= test_invalid_element_segment_func_idx();

    if (result == 0) {
        printf("All parser tests passed!\n");
    } else {
        printf("Parser tests failed!\n");
    }

    return result;
}
