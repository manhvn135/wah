#define WAH_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include "wah.h"

// A simple WebAssembly binary for:
// (module
//   (func (param i32 i32) (result i32)
//     (i32.add (local.get 0) (local.get 1))
//   )
// )
const uint8_t simple_add_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section ID
    0x07, // section size
    0x01, // num types
    0x60, // func type
    0x02, // num params
    0x7f, 0x7f, // i32, i32
    0x01, // num results
    0x7f, // i32

    // Function section
    0x03, // section ID
    0x02, // section size
    0x01, // num functions
    0x00, // type index 0

    // Code section
    0x0a, // section ID
    0x09, // section size
    0x01, // num code bodies
    0x07, // code body size (for the first function)
    0x00, // num locals
    0x20, 0x00, // local.get 0
    0x20, 0x01, // local.get 1
    0x6a, // i32.add
    0x0b, // end
};

// A semantically invalid WebAssembly binary:
// Attempts to local.get an out-of-bounds index (2) for a function with 2 parameters and 0 locals.
const uint8_t invalid_local_get_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section ID
    0x07, // section size
    0x01, // num types
    0x60, // func type
    0x02, // num params
    0x7f, 0x7f, // i32, i32
    0x01, // num results
    0x7f, // i32

    // Function section
    0x03, // section ID
    0x02, // section size
    0x01, // num functions
    0x00, // type index 0

    // Code section
    0x0a, // section ID
    0x09, // section size
    0x01, // num code bodies
    0x07, // code body size (for the first function)
    0x00, // num locals
    0x20, 0x02, // local.get 2 (INVALID: out of bounds for 2 params, 0 locals)
    0x20, 0x01, // local.get 1
    0x6a, // i32.add
    0x0b, // end
};

// A WebAssembly binary that causes stack underflow:
// Tries to i32.add with only one value on the stack
const uint8_t stack_underflow_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section ID
    0x07, // section size
    0x01, // num types
    0x60, // func type
    0x01, // num params
    0x7f, // i32
    0x01, // num results
    0x7f, // i32

    // Function section
    0x03, // section ID
    0x02, // section size
    0x01, // num functions
    0x00, // type index 0

    // Code section
    0x0a, // section ID
    0x08, // section size
    0x01, // num code bodies
    0x06, // code body size
    0x00, // num locals
    0x20, 0x00, // local.get 0 (puts one i32 on stack)
    0x6a, // i32.add (tries to pop two i32s - UNDERFLOW!)
    0x0b, // end
};

int main() {
    wah_module_t module;
    wah_exec_context_t ctx; // Add this line
    wah_error_t err;

    printf("--- Testing Valid Module (simple_add_wasm) ---\n");
    printf("Parsing simple_add_wasm module...\n");
    err = wah_parse_module((const uint8_t *)simple_add_wasm, sizeof(simple_add_wasm), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing valid module: %d\n", err);
        return 1;
    }
    printf("Module parsed successfully.\n");

    // Create execution context
    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context: %d\n", err);
        wah_free_module(&module);
        return 1;
    }

    printf("Function max stack depth: %u\n", module.code_bodies[0].max_stack_depth);

    uint32_t func_idx = 0;
    wah_value_t params[2];
    wah_value_t result;

    params[0].i32 = 10;
    params[1].i32 = 20;

    printf("Interpreting function %u with params %d and %d...\n", func_idx, params[0].i32, params[1].i32);
    err = wah_call(&ctx, &module, func_idx, params, 2, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function: %d\n", err);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 1;
    }
    printf("Function interpreted successfully.\n");
    printf("Result: %d\n", result.i32);

    params[0].i32 = 5;
    params[1].i32 = 7;
    printf("Interpreting function %u with params %d and %d...\n", func_idx, params[0].i32, params[1].i32);
    err = wah_call(&ctx, &module, func_idx, params, 2, &result);
    if (err != WAH_OK) {
        fprintf(stderr, "Error interpreting function: %d\n", err);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 1;
    }
    printf("Function interpreted successfully.\n");
    printf("Result: %d\n", result.i32);

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("Valid module freed.\n");

    printf("\n--- Testing Invalid Module (invalid_local_get_wasm) ---\n");
    printf("Parsing invalid_local_get_wasm module...\n");
    err = wah_parse_module((const uint8_t *)invalid_local_get_wasm, sizeof(invalid_local_get_wasm), &module);
    if (err == WAH_ERROR_VALIDATION_FAILED) {
        printf("Successfully detected invalid module during parsing (expected WAH_ERROR_VALIDATION_FAILED).\n");
    } else if (err != WAH_OK) {
        fprintf(stderr, "Error parsing invalid module: %d (Expected parsing to succeed)\n", err);
        return 1;
    } else {
        fprintf(stderr, "Invalid module parsed successfully (Expected WAH_ERROR_VALIDATION_FAILED)\n");
        wah_free_module(&module);
        return 1;
    }

    printf("\n--- Testing Stack Underflow Module (stack_underflow_wasm) ---\n");
    printf("Parsing stack_underflow_wasm module...\n");
    err = wah_parse_module((const uint8_t *)stack_underflow_wasm, sizeof(stack_underflow_wasm), &module);
    if (err == WAH_ERROR_VALIDATION_FAILED) {
        printf("Successfully detected stack underflow during parsing (expected WAH_ERROR_VALIDATION_FAILED).\n");
    } else if (err != WAH_OK) {
        printf("Successfully detected stack underflow during parsing (error code %d).\n", err);
    } else {
        fprintf(stderr, "Stack underflow module parsed successfully (Expected WAH_ERROR_VALIDATION_FAILED)\n");
        wah_free_module(&module);
        return 1;
    }

    return 0;
}
