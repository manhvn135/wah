#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <assert.h>

// Test ref.null with funcref type
static const uint8_t ref_null_funcref_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x70, // func type: 0 params, 1 result (funcref)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Export section
    0x07, 0x0d, 0x01,       // section 7, size 13, 1 export
    0x09, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x66, 0x75, 0x6e, 0x63, // "test_func"
    0x00, 0x00,             // export kind 0 (function), func index 0

    // Code section
    0x0a, 0x06, 0x01,       // section 10, size 6, 1 body
    0x04, 0x00,             // body size 4, 0 locals

    // Function body: ref.null funcref
    0xd0, 0x70,             // ref.null funcref
    0x0b                    // end function
};

// Test ref.null with externref type
static const uint8_t ref_null_externref_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x6f, // func type: 0 params, 1 result (externref)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Export section
    0x07, 0x0f, 0x01,       // section 7, size 15, 1 export
    0x0b, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x65, 0x78, 0x74, 0x65, 0x72, 0x6e, // "test_extern"
    0x00, 0x00,             // export kind 0 (function), func index 0

    // Code section
    0x0a, 0x06, 0x01,       // section 10, size 6, 1 body
    0x04, 0x00,             // body size 4, 0 locals

    // Function body: ref.null externref
    0xd0, 0x6f,             // ref.null externref
    0x0b                    // end function
};

// Test ref.func
static const uint8_t ref_func_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x70, // func type: 0 params, 1 result (funcref)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Export section
    0x07, 0x11, 0x01,       // section 7, size 17, 1 export
    0x0d, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x72, 0x65, 0x66, 0x5f, 0x66, 0x75, 0x6e, 0x63, // "test_ref_func"
    0x00, 0x00,             // export kind 0 (function), func index 0

    // Code section
    0x0a, 0x06, 0x01,       // section 10, size 6, 1 body
    0x04, 0x00,             // body size 4, 0 locals

    // Function body: ref.func 0
    0xd2, 0x00,             // ref.func 0
    0x0b                    // end function
};

// Test ref.is_null with funcref
static const uint8_t ref_is_null_funcref_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Export section
    0x07, 0x10, 0x01,       // section 7, size 16, 1 export
    0x0c, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x69, 0x73, 0x5f, 0x6e, 0x75, 0x6c, 0x6c, // "test_is_null"
    0x00, 0x00,             // export kind 0 (function), func index 0

    // Code section
    0x0a, 0x07, 0x01,       // section 10, size 7, 1 body
    0x05, 0x00,             // body size 5, 0 locals

    // Function body: ref.null funcref; ref.is_null
    0xd0, 0x70,             // ref.null funcref
    0xd1,                   // ref.is_null
    0x0b                    // end function
};

static int test_ref_null_funcref() {
    printf("Running test_ref_null_funcref...\n");

    wah_module_t module;
    wah_error_t err = wah_parse_module(ref_null_funcref_wasm, sizeof(ref_null_funcref_wasm), &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not parse module: %s\n", wah_strerror(err));
        return 1;
    }

    wah_exec_context_t exec_ctx;
    wah_value_t result;

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not create execution context: %s\n", wah_strerror(err));
        return 1;
    }

    err = wah_call(&exec_ctx, &module, 0, NULL, 0, &result);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not call function: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        return 1;
    }

    if (result.funcref != 0) {
        printf("  - FAILED: Expected funcref to be 0 (null), got %u\n", result.funcref);
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return 1;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    printf("  - PASSED: ref.null funcref returned 0\n");
    return 0;
}

static int test_ref_null_externref() {
    printf("Running test_ref_null_externref...\n");

    wah_module_t module;
    wah_error_t err = wah_parse_module(ref_null_externref_wasm, sizeof(ref_null_externref_wasm), &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not parse module: %s\n", wah_strerror(err));
        return 1;
    }

    wah_exec_context_t exec_ctx;
    wah_value_t result;

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not create execution context: %s\n", wah_strerror(err));
        return 1;
    }

    err = wah_call(&exec_ctx, &module, 0, NULL, 0, &result);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not call function: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        return 1;
    }

    if (result.externref != NULL) {
        printf("  - FAILED: Expected externref to be NULL, got %p\n", result.externref);
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return 1;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    printf("  - PASSED: ref.null externref returned NULL\n");
    return 0;
}

static int test_ref_func() {
    printf("Running test_ref_func...\n");

    wah_module_t module;
    wah_error_t err = wah_parse_module(ref_func_wasm, sizeof(ref_func_wasm), &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not parse module: %s\n", wah_strerror(err));
        return 1;
    }

    wah_exec_context_t exec_ctx;
    wah_value_t result;

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not create execution context: %s\n", wah_strerror(err));
        return 1;
    }

    err = wah_call(&exec_ctx, &module, 0, NULL, 0, &result);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not call function: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        return 1;
    }

    if (result.funcref != 0) {
        printf("  - FAILED: Expected funcref to be 0, got %u\n", result.funcref);
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return 1;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    printf("  - PASSED: ref.func 0 returned function reference 0\n");
    return 0;
}

static int test_ref_is_null() {
    printf("Running test_ref_is_null...\n");

    wah_module_t module;
    wah_error_t err = wah_parse_module(ref_is_null_funcref_wasm, sizeof(ref_is_null_funcref_wasm), &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not parse module: %s\n", wah_strerror(err));
        return 1;
    }

    wah_exec_context_t exec_ctx;
    wah_value_t result;

    err = wah_exec_context_create(&exec_ctx, &module);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not create execution context: %s\n", wah_strerror(err));
        return 1;
    }

    err = wah_call(&exec_ctx, &module, 0, NULL, 0, &result);
    if (err != WAH_OK) {
        printf("  - FAILED: Could not call function: %s\n", wah_strerror(err));
        wah_exec_context_destroy(&exec_ctx);
        return 1;
    }

    if (result.i32 != 1) {
        printf("  - FAILED: Expected ref.is_null to return 1 (true), got %d\n", result.i32);
        wah_exec_context_destroy(&exec_ctx);
        wah_free_module(&module);
        return 1;
    }

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);
    printf("  - PASSED: ref.is_null returned 1 for null reference\n");
    return 0;
}

int main() {
    printf("Testing WebAssembly Reference Types...\n\n");

    int failed = 0;

    failed |= test_ref_null_funcref();
    failed |= test_ref_null_externref();
    failed |= test_ref_func();
    failed |= test_ref_is_null();

    if (failed) {
        printf("\nSome tests failed.\n");
        return 1;
    } else {
        printf("\nAll Reference Types tests passed!\n");
        return 0;
    }
}