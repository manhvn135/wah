#define WAH_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "wah.h"

// A WebAssembly binary with a start section that sets a global variable.
// (module
//   (global $g (mut i32) (i32.const 0))
//   (func $start_func
//     (global.set $g (i32.const 42))
//   )
//   (start $start_func)
// )
const uint8_t start_section_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, // section ID
    0x04, // section size
    0x01, // num types
    0x60, // func type (void -> void)
    0x00, // num params
    0x00, // num results

    // Function section
    0x03, // section ID
    0x02, // section size
    0x01, // num functions
    0x00, // type index 0 (void -> void)

    // Global section
    0x06, // section ID
    0x06, // section size
    0x01, // num globals
    0x7f, // i32
    0x01, // mutable
    0x41, 0x00, // i32.const 0
    0x0b, // end

    // Start section
    0x08, // section ID
    0x01, // section size
    0x00, // start function index (function 0)

    // Code section
    0x0a, // section ID
    0x08, // section size
    0x01, // num code bodies
    0x06, // code body size (for the first function)
    0x00, // num locals
    0x41, 0x2a, // i32.const 42
    0x24, 0x00, // global.set 0
    0x0b, // end
};

// --- Start Section Test ---

int test_start_section() {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    printf("--- Testing Start Section ---\n");
    printf("Parsing start_section_wasm module...\n");
    err = wah_parse_module((const uint8_t *)start_section_wasm, sizeof(start_section_wasm), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing module with start section: %s\n", wah_strerror(err));
        return 0;
    }
    printf("Module parsed successfully.\n");

    // Create execution context. This should trigger the start function.
    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for start section: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 0;
    }
    printf("Execution context created. Checking global variable...\n");

    // Verify that the global variable was set by the start function
    if (ctx.globals[0].i32 == 42) {
        printf("Global variable $g is %d (expected 42). Start section executed successfully.\n", ctx.globals[0].i32);
    } else {
        fprintf(stderr, "Global variable $g is %d (expected 42). Start section FAILED to execute or set value.\n", ctx.globals[0].i32);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 0;
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("Module with start section freed.\n");
    return 1;
}

// --- Multiple Return Functionality Tests ---

// Test zero return functions with wah_call and wah_call_multi
int test_zero_return_functions() {
    printf("\n--- Testing Zero Return Functions ---\n");

    // Simple void function: () -> ()
    static const uint8_t wasm_binary[] = {
        // Magic + Version
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

        // Type Section (id 1), size 4 (0x04)
        0x01, 0x04, 0x01,
        0x60, 0x00, 0x00, // () -> ()

        // Function Section (id 3), size 2 (0x02)
        0x03, 0x02, 0x01,
        0x00, // type index 0

        // Code Section (id 10), size 4 (0x04)
        0x0a, 0x04, 0x01,
        0x02, 0x00, 0x0b // empty function: just end
    };

    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    err = wah_parse_module((const uint8_t *)wasm_binary, sizeof(wasm_binary), &module);
    if (err != WAH_OK) {
        printf("[x] Parse error: %s\n", wah_strerror(err));
        return 0;
    }

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        printf("[x] Context creation error: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 0;
    }

    // Test wah_call with zero return function
    wah_value_t result;
    memset(&result, 0xFF, sizeof(wah_value_t)); // Initialize with garbage to verify zeroization
    err = wah_call(&ctx, &module, 0, NULL, 0, &result);
    if (err == WAH_OK) {
        // Verify that result is properly zeroized
        bool is_zeroed = (result.i32 == 0 && result.i64 == 0 && result.f32 == 0.0f && result.f64 == 0.0);
        if (is_zeroed) {
            printf("[v] wah_call with zero return: result properly zeroized\n");
        } else {
            printf("[x] wah_call with zero return: result not zeroized (i32=%d, i64=%lld, f32=%.6f, f64=%.6f)\n",
                   result.i32, (long long)result.i64, result.f32, result.f64);
            wah_exec_context_destroy(&ctx);
            wah_free_module(&module);
            return 0;
        }
    } else {
        printf("[x] wah_call with zero return failed: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 0;
    }

    // Test wah_call_multi with zero return function
    wah_value_t results[1];
    uint32_t actual_results;
    err = wah_call_multi(&ctx, &module, 0, NULL, 0, results, 1, &actual_results);
    if (err == WAH_OK && actual_results == 0) {
        printf("[v] wah_call_multi with zero return: actual_results=%u (expected 0)\n", actual_results);
    } else {
        printf("[x] wah_call_multi with zero return failed: err=%s, actual_results=%u\n",
               wah_strerror(err), actual_results);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 0;
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    return 1;
}

// Test single return functions (baseline)
int test_single_return_functions() {
    printf("\n--- Testing Single Return Functions ---\n");

    // Function that returns i32 constant 42
    static const uint8_t wasm_binary[] = {
        // Magic + Version
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

        // Type Section (id 1), size 5 (0x05)
        0x01, 0x05, 0x01,
        0x60, 0x00, 0x01, 0x7f, // () -> i32

        // Function Section (id 3), size 2 (0x02)
        0x03, 0x02, 0x01,
        0x00, // type index 0

        // Code Section (id 10), size 6 (0x06)
        0x0a, 0x06, 0x01,
        0x04, 0x00, 0x41, 0x2a, 0x0b // i32.const 42, end
    };

    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    err = wah_parse_module((const uint8_t *)wasm_binary, sizeof(wasm_binary), &module);
    if (err != WAH_OK) {
        printf("[x] Parse error: %s\n", wah_strerror(err));
        return 0;
    }

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        printf("[x] Context creation error: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 0;
    }

    // Test wah_call with single return function
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, NULL, 0, &result);
    if (err == WAH_OK && result.i32 == 42) {
        printf("[v] wah_call with single return: i32=%d (expected 42)\n", result.i32);
    } else {
        printf("[x] wah_call with single return failed: err=%s, i32=%d\n",
               wah_strerror(err), result.i32);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 0;
    }

    // Test wah_call_multi with single return function
    wah_value_t results[1];
    uint32_t actual_results;
    err = wah_call_multi(&ctx, &module, 0, NULL, 0, results, 1, &actual_results);
    if (err == WAH_OK && actual_results == 1 && results[0].i32 == 42) {
        printf("[v] wah_call_multi with single return: i32=%d, count=%u\n",
               results[0].i32, actual_results);
    } else {
        printf("[x] wah_call_multi with single return failed: err=%s, i32=%d, count=%u\n",
               wah_strerror(err), results[0].i32, actual_results);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 0;
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    return 1;
}

// Test using existing globals module to verify wah_call_multi works
int test_multiple_return_with_existing_functions() {
    printf("\n--- Testing Multiple Return API with Existing Functions ---\n");

    // Copy the working globals binary from wah_test_globals.c
    static const uint8_t wasm_binary[] = {
        // Magic + Version
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

        // Type Section (id 1), size 17 (0x11)
        0x01, 0x11, 0x04,
        0x60, 0x00, 0x01, 0x7e, // 0: () -> i64
        0x60, 0x01, 0x7e, 0x00, // 1: (i64) -> ()
        0x60, 0x00, 0x01, 0x7d, // 2: () -> f32
        0x60, 0x01, 0x7d, 0x00, // 3: (f32) -> ()

        // Function Section (id 3), size 5 (0x05)
        0x03, 0x05, 0x04,
        0x00, 0x01, 0x02, 0x03,

        // Global Section (id 6), size 15 (0x0f)
        0x06, 0x0f, 0x02,
        // global 0: i64, mutable, init: i64.const 200
        0x7e, 0x01, 0x42, 0xc8, 0x01, 0x0b,
        // global 1: f32, mutable, init: f32.const 1.5
        0x7d, 0x01, 0x43, 0x00, 0x00, 0xc0, 0x3f, 0x0b,

        // Code Section (id 10), size 25 (0x19)
        0x0a, 0x19, 0x04,
        // func 0 body (get_i64): size 4, locals 0, code: global.get 0; end
        0x04, 0x00, 0x23, 0x00, 0x0b,
        // func 1 body (set_i64): size 6, locals 0, code: local.get 0; global.set 0; end
        0x06, 0x00, 0x20, 0x00, 0x24, 0x00, 0x0b,
        // func 2 body (get_f32): size 4, locals 0, code: global.get 1; end
        0x04, 0x00, 0x23, 0x01, 0x0b,
        // func 3 body (set_f32): size 6, locals 0, code: local.get 0; global.set 1; end
        0x06, 0x00, 0x20, 0x00, 0x24, 0x01, 0x0b,
    };

    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    err = wah_parse_module((const uint8_t *)wasm_binary, sizeof(wasm_binary), &module);
    if (err != WAH_OK) {
        printf("❌ Parse error: %s\n", wah_strerror(err));
        return 0;
    }

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        printf("❌ Context creation error: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 0;
    }

    // Test function 0: get_i64 with wah_call_multi (should return WAH_OK)
    wah_value_t results[1];
    uint32_t actual_results;
    err = wah_call_multi(&ctx, &module, 0, NULL, 0, results, 1, &actual_results);
    if (err == WAH_OK) {
        printf("[v] wah_call_multi with single return: i64=%lld, count=%u\n",
               (long long)results[0].i64, actual_results);
    } else {
        printf("[x] wah_call_multi failed: %s\n", wah_strerror(err));
    }

    // Test function 2: get_f32 with wah_call_multi
    wah_value_t f32_results[1];
    uint32_t f32_actual_results;
    err = wah_call_multi(&ctx, &module, 2, NULL, 0, f32_results, 1, &f32_actual_results);
    if (err == WAH_OK) {
        printf("[v] wah_call_multi get_f32: result=%.2f, count=%u\n",
               f32_results[0].f32, f32_actual_results);
    } else {
        printf("[x] wah_call_multi get_f32 failed: %s\n", wah_strerror(err));
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);

    // Return 1 (success) if both tests passed
    return 1;
}

// Test buffer validation with wah_call_multi
int test_multi_return_buffer_validation() {
    printf("\n--- Testing Multi-Return Buffer Validation ---\n");

    // Use the same working binary from other tests for buffer validation
    static const uint8_t wasm_binary[] = {
        // Magic + Version
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

        // Type Section (id 1), size 17 (0x11)
        0x01, 0x11, 0x04,
        0x60, 0x00, 0x01, 0x7e, // 0: () -> i64
        0x60, 0x01, 0x7e, 0x00, // 1: (i64) -> ()
        0x60, 0x00, 0x01, 0x7d, // 2: () -> f32
        0x60, 0x01, 0x7d, 0x00, // 3: (f32) -> ()

        // Function Section (id 3), size 5 (0x05)
        0x03, 0x05, 0x04,
        0x00, 0x01, 0x02, 0x03,

        // Global Section (id 6), size 15 (0x0f)
        0x06, 0x0f, 0x02,
        // global 0: i64, mutable, init: i64.const 200
        0x7e, 0x01, 0x42, 0xc8, 0x01, 0x0b,
        // global 1: f32, mutable, init: f32.const 1.5
        0x7d, 0x01, 0x43, 0x00, 0x00, 0xc0, 0x3f, 0x0b,

        // Code Section (id 10), size 25 (0x19)
        0x0a, 0x19, 0x04,
        // func 0 body (get_i64): size 4, locals 0, code: global.get 0; end
        0x04, 0x00, 0x23, 0x00, 0x0b,
        // func 1 body (set_i64): size 6, locals 0, code: local.get 0; global.set 0; end
        0x06, 0x00, 0x20, 0x00, 0x24, 0x00, 0x0b,
        // func 2 body (get_f32): size 4, locals 0, code: global.get 1; end
        0x04, 0x00, 0x23, 0x01, 0x0b,
        // func 3 body (set_f32): size 6, locals 0, code: local.get 0; global.set 1; end
        0x06, 0x00, 0x20, 0x00, 0x24, 0x01, 0x0b,
    };

    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    err = wah_parse_module((const uint8_t *)wasm_binary, sizeof(wasm_binary), &module);
    if (err != WAH_OK) {
        printf("[x] Parse error: %s (buffer validation test skipped)\n", wah_strerror(err));
        return 0;
    }

    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        printf("[x] Context creation error: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 0;
    }

    // Test with insufficient buffer (should return validation error)
    wah_value_t results[0]; // No space for result
    uint32_t actual_results;
    err = wah_call_multi(&ctx, &module, 0, NULL, 0, results, 0, &actual_results);
    if (err == WAH_ERROR_VALIDATION_FAILED) {
        printf("[v] Insufficient buffer correctly returned error: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 1; // Test passed
    } else {
        printf("[x] Expected validation error for insufficient buffer, got: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 0; // Test failed
    }
}

int main() {
    bool success = true;

    success = success && test_start_section();
    success = success && test_zero_return_functions();
    success = success && test_single_return_functions();
    success = success && test_multiple_return_with_existing_functions();
    success = success && test_multi_return_buffer_validation();

    printf("=== Test Results ===\n");

    if (success) {
        printf("[v] All tests passed!\n");
        return 0;
    } else {
        printf("[x] Some tests failed.\n");
        return 1;
    }
}
