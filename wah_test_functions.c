#define WAH_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
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

int main() {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;

    printf("--- Testing Start Section ---\n");
    printf("Parsing start_section_wasm module...\n");
    err = wah_parse_module((const uint8_t *)start_section_wasm, sizeof(start_section_wasm), &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error parsing module with start section: %s\n", wah_strerror(err));
        return 1;
    }
    printf("Module parsed successfully.\n");

    // Create execution context. This should trigger the start function.
    err = wah_exec_context_create(&ctx, &module);
    if (err != WAH_OK) {
        fprintf(stderr, "Error creating execution context for start section: %s\n", wah_strerror(err));
        wah_free_module(&module);
        return 1;
    }
    printf("Execution context created. Checking global variable...\n");

    // Verify that the global variable was set by the start function
    if (ctx.globals[0].i32 == 42) {
        printf("Global variable $g is %d (expected 42). Start section executed successfully.\n", ctx.globals[0].i32);
    } else {
        fprintf(stderr, "Global variable $g is %d (expected 42). Start section FAILED to execute or set value.\n", ctx.globals[0].i32);
        wah_exec_context_destroy(&ctx);
        wah_free_module(&module);
        return 1;
    }

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("Module with start section freed.\n");

    return 0;
}
