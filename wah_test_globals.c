#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h> // For fabsf

#define WAH_IMPLEMENTATION
#include "wah.h"

// This WASM module was crafted by hand to test globals.
// It defines two mutable globals: an i64 and an f32.
// It uses function indices: 0:get_i64, 1:set_i64, 2:get_f32, 3:set_f32
//
// WAT equivalent:
// (module
//   (global $g_i64 (mut i64) (i64.const 200))
//   (global $g_f32 (mut f32) (f32.const 1.5))
//
//   (func $get_i64 (result i64) global.get $g_i64)
//   (func $set_i64 (param i64) local.get 0 global.set $g_i64)
//
//   (func $get_f32 (result f32) global.get $g_f32)
//   (func $set_f32 (param f32) local.get 0 global.set $g_f32)
// )
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

static const uint8_t wasm_binary_type_mismatch[] = {
    // Magic + Version
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

    // Type Section (empty for this test, as we only care about global init)
    0x01, 0x01, 0x00, // Section ID, Size, Number of Types (0)

    // Global Section (id 6), size 10 (0x0a)
    0x06, 0x09, 0x01, // Section ID, Size, Number of Globals
    // global 0: i64, mutable, init: f32.const 1.5 (TYPE MISMATCH)
    0x7e, // Value Type (i64) - Declared as i64
    0x01, // Mutable
    0x43, // Opcode for f32.const - Initialized with f32.const
    0x00, 0x00, 0xc0, 0x3f, // IEEE 754 for 1.5f
    0x0b, // Opcode for end
};

void test_i64_global(wah_exec_context_t* exec_ctx, wah_module_t* module) {
    wah_value_t result;
    wah_error_t err;

    printf("Testing i64 global...\n");
    // Get initial value
    err = wah_call(exec_ctx, module, 0, NULL, 0, &result);
    assert(err == WAH_OK);
    printf("Initial i64 value: %lld\n", (long long)result.i64);
    assert(result.i64 == 200);

    // Set new value
    wah_value_t params_i64[1];
    params_i64[0].i64 = -5000;
    err = wah_call(exec_ctx, module, 1, params_i64, 1, NULL);
    assert(err == WAH_OK);

    // Get new value
    err = wah_call(exec_ctx, module, 0, NULL, 0, &result);
    assert(err == WAH_OK);
    printf("New i64 value: %lld\n", (long long)result.i64);
    assert(result.i64 == -5000);
}

void test_f32_global(wah_exec_context_t* exec_ctx, wah_module_t* module) {
    wah_value_t result;
    wah_error_t err;

    printf("Testing f32 global...\n");
    // Get initial value
    err = wah_call(exec_ctx, module, 2, NULL, 0, &result);
    assert(err == WAH_OK);
    printf("Initial f32 value: %f\n", result.f32);
    assert(fabsf(result.f32 - 1.5f) < 1e-6);

    // Set new value
    wah_value_t params_f32[1];
    params_f32[0].f32 = 9.99f;
    err = wah_call(exec_ctx, module, 3, params_f32, 1, NULL);
    assert(err == WAH_OK);

    // Get new value
    err = wah_call(exec_ctx, module, 2, NULL, 0, &result);
    assert(err == WAH_OK);
    printf("New f32 value: %f\n", result.f32);
    assert(fabsf(result.f32 - 9.99f) < 1e-6);
}

void test_global_type_mismatch(void) {
    wah_module_t module_mismatch;
    wah_error_t err_mismatch;

    printf("\n--- Running Global Type Mismatch Test ---\n");

    // Parse the module - this should fail with WAH_ERROR_TYPE_MISMATCH
    err_mismatch = wah_parse_module(wasm_binary_type_mismatch, sizeof(wasm_binary_type_mismatch), &module_mismatch);

    printf("Expected error: %s, Actual error: %s\n", wah_strerror(WAH_ERROR_VALIDATION_FAILED), wah_strerror(err_mismatch));
    assert(err_mismatch == WAH_ERROR_VALIDATION_FAILED);

    printf("--- Global Type Mismatch Test Passed (as expected failure) ---\n");
}

int main(void) {
    wah_module_t module;
    wah_exec_context_t exec_ctx;
    wah_error_t err;

    printf("--- Running Globals Test ---\n");

    // Parse the module
    err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    if (err != WAH_OK) {
        printf("Failed to parse module: %s\n", wah_strerror(err));
        assert(err == WAH_OK);
    }

    // Create execution context
    err = wah_exec_context_create(&exec_ctx, &module);
    assert(err == WAH_OK);

    test_i64_global(&exec_ctx, &module);
    test_f32_global(&exec_ctx, &module);

    // Cleanup for regular globals test
    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);

    printf("--- Globals Test Passed ---\n");

    test_global_type_mismatch();

    return 0;
}
