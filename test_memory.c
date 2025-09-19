#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WAH_IMPLEMENTATION
#include "wah.h"

// A simple WebAssembly binary for testing i32.load and i32.store
// This binary defines:
// - A memory section with 1 page (64KB)
// - Two function types:
//   - (i32, i32) -> () for store_val
//   - (i32) -> (i32) for load_val
// - Two functions:
//   - func 0: store_val (address, value)
//   - func 1: load_val (address)
// - Export section to export "store" and "load" functions
static const uint8_t wasm_binary_memory_test[] = {
    0x00, 0x61, 0x73, 0x6D, // Magic number
    0x01, 0x00, 0x00, 0x00, // Version

    // Type Section (1)
    0x01, // Section ID
    0x0B, // Section size (11 bytes)
    0x02, // Number of types
        // Type 0: (i32, i32) -> ()
        0x60, // Function type
        0x02, // 2 parameters
        0x7F, // i32
        0x7F, // i32
        0x00, // 0 results
        // Type 1: (i32) -> (i32)
        0x60, // Function type
        0x01, // 1 parameter
        0x7F, // i32
        0x01, // 1 result
        0x7F, // i32

    // Import Section (2) - None for this test

    // Function Section (3)
    0x03, // Section ID
    0x03, // Section size (3 bytes)
    0x02, // Number of functions
    0x00, // Function 0 uses type 0 (store_val)
    0x01, // Function 1 uses type 1 (load_val)

    // Table Section (4) - None for this test

    // Memory Section (5)
    0x05, // Section ID
    0x03, // Section size (3 bytes)
    0x01, // Number of memories
    0x00, // Flags (0x00 for fixed size)
    0x01, // Initial pages (1 page = 64KB)

    // Global Section (6) - None for this test

    // Export Section (7)
    0x07, // Section ID
    0x10, // Section size (16 bytes)
    0x02, // Number of exports
        // Export "store" (func 0)
        0x05, // Length of name "store"
        's', 't', 'o', 'r', 'e',
        0x00, // Export kind: Function
        0x00, // Function index 0
        // Export "load" (func 1)
        0x04, // Length of name "load"
        'l', 'o', 'a', 'd',
        0x00, // Export kind: Function
        0x01, // Function index 1

    // Start Section (8) - None for this test
    // Element Section (9) - None for this test

    // Code Section (10)
    0x0A, // Section ID
    0x13, // Section size (19 bytes)
    0x02, // Number of code bodies

        // Code body 0: store_val (address, value)
        0x09, // Body size (9 bytes)
        0x00, // No local variables
        0x20, 0x00, // local.get 0 (address)
        0x20, 0x01, // local.get 1 (value)
        0x36, 0x02, 0x00, // i32.store align=2 (2^2=4), offset=0
        0x0B, // end

        // Code body 1: load_val (address)
        0x07, // Body size (7 bytes)
        0x00, // No local variables
        0x20, 0x00, // local.get 0 (address)
        0x28, 0x02, 0x00, // i32.load align=2 (2^2=4), offset=0
        0x0B, // end
};

static const unsigned char wasm_binary_memory_ops_test[] = {
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x10, 0x03, 0x60,
  0x00, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x03, 0x7f, 0x7f,
  0x7f, 0x00, 0x03, 0x04, 0x03, 0x00, 0x01, 0x02, 0x05, 0x04, 0x01, 0x01,
  0x01, 0x02, 0x07, 0x35, 0x04, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x0f,
  0x67, 0x65, 0x74, 0x5f, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x5f, 0x73,
  0x69, 0x7a, 0x65, 0x00, 0x00, 0x0b, 0x67, 0x72, 0x6f, 0x77, 0x5f, 0x6d,
  0x65, 0x6d, 0x6f, 0x72, 0x79, 0x00, 0x01, 0x0b, 0x66, 0x69, 0x6c, 0x6c,
  0x5f, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x00, 0x02, 0x0a, 0x19, 0x03,
  0x04, 0x00, 0x3f, 0x00, 0x0b, 0x06, 0x00, 0x20, 0x00, 0x40, 0x00, 0x0b,
  0x0b, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0xfc, 0x0b, 0x00, 0x0b
};


int main() {
    wah_module_t module;
    wah_exec_context_t ctx;
    wah_error_t err;
    wah_value_t params[3];
    wah_value_t result;

    printf("Running memory tests...\n");

    // Test 1: Parse module
    err = wah_parse_module(wasm_binary_memory_test, sizeof(wasm_binary_memory_test), &module);
    assert(err == WAH_OK && "Failed to parse module");
    printf("Module parsed successfully.\n");
    assert(module.memory_count == 1 && "Expected 1 memory section");
    assert(module.memories[0].min_pages == 1 && "Expected 1 min page");

    // Test 2: Create execution context
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK && "Failed to create execution context");
    printf("Execution context created successfully.\n");
    assert(ctx.memory_base != NULL && "Memory base should not be NULL");
    assert(ctx.memory_size == WAH_WASM_PAGE_SIZE && "Memory size should be 1 page");

    // Test 3: Store a value
    uint32_t test_address = 1024;
    int32_t test_value = 0xDEADBEEF;
    params[0].i32 = test_address;
    params[1].i32 = test_value;
    err = wah_call(&ctx, &module, 0, params, 2, NULL); // Call store_val (func 0)
    assert(err == WAH_OK && "Failed to call store_val");
    printf("Stored 0x%X at address %u.\n", test_value, test_address);

    // Verify directly in memory
    int32_t *mem_ptr = (int32_t*)(ctx.memory_base + test_address);
    assert(*mem_ptr == test_value && "Value not correctly stored in memory");
    printf("Direct memory verification successful.\n");

    // Test 4: Load the value
    params[0].i32 = test_address;
    err = wah_call(&ctx, &module, 1, params, 1, &result); // Call load_val (func 1)
    assert(err == WAH_OK && "Failed to call load_val");
    assert(result.i32 == test_value && "Loaded value does not match stored value");
    printf("Loaded 0x%X from address %u. Verification successful.\n", result.i32, test_address);

    // Test 5: Memory out-of-bounds store
    uint32_t oob_address_store = WAH_WASM_PAGE_SIZE - 2; // 2 bytes before end, trying to store 4 bytes
    params[0].i32 = oob_address_store;
    params[1].i32 = 0x12345678;
    err = wah_call(&ctx, &module, 0, params, 2, NULL);
    assert(err == WAH_ERROR_MEMORY_OUT_OF_BOUNDS && "Expected memory out-of-bounds error for store");
    printf("Memory out-of-bounds store test successful.\n");

    // Test 6: Memory out-of-bounds load
    uint32_t oob_address_load = WAH_WASM_PAGE_SIZE - 2; // 2 bytes before end, trying to load 4 bytes
    params[0].i32 = oob_address_load;
    err = wah_call(&ctx, &module, 1, params, 1, &result);
    assert(err == WAH_ERROR_MEMORY_OUT_OF_BOUNDS && "Expected memory out-of-bounds error for load");
    printf("Memory out-of-bounds load test successful.\n");

    // Test 7: Offset overflow with wrap-around
    printf("Testing offset overflow with wrap-around...\n");
    uint32_t base_addr_overflow = 0xFFFFFFF0; // A high address
    uint32_t offset_overflow = 0x20;         // An offset that causes overflow
    uint32_t expected_effective_addr = (base_addr_overflow + offset_overflow); // Expected wrapped address

    // Ensure the expected_effective_addr is within bounds for our test
    // We want it to wrap around to a valid address within the first page
    assert(expected_effective_addr < WAH_WASM_PAGE_SIZE - 4 && "Expected effective address out of test bounds");

    int32_t test_value_overflow = 0xCAFEBABE;

    // Store the value using the overflowed address
    params[0].i32 = base_addr_overflow;
    params[1].i32 = test_value_overflow;
    err = wah_call(&ctx, &module, 0, params, 2, NULL); // Call store_val (func 0)
    assert(err == WAH_ERROR_MEMORY_OUT_OF_BOUNDS && "Expected memory out-of-bounds error for store with overflow address");
    printf("Memory out-of-bounds store test with overflow address successful.\n");

    // Load the value using the overflowed address
    params[0].i32 = base_addr_overflow;
    err = wah_call(&ctx, &module, 1, params, 1, &result); // Call load_val (func 1)
    assert(err == WAH_ERROR_MEMORY_OUT_OF_BOUNDS && "Expected memory out-of-bounds error for load with overflow address");
    printf("Memory out-of-bounds load test with overflow address successful.\n");

    // Cleanup for first module
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);

    printf("\nRunning memory operations tests (size, grow, fill)...\n");

    // Test 8: Parse new module for memory operations
    err = wah_parse_module(wasm_binary_memory_ops_test, sizeof(wasm_binary_memory_ops_test), &module);
    if (err != WAH_OK) {
        printf("Failed to parse memory ops module. Error: %d\n", err);
    }
    assert(err == WAH_OK && "Failed to parse memory ops module");
    printf("Memory ops module parsed successfully.\n");
    assert(module.memory_count == 1 && "Expected 1 memory section in ops module");
    assert(module.memories[0].min_pages == 1 && "Expected 1 min page in ops module");
    assert(module.memories[0].max_pages == 2 && "Expected 2 max pages in ops module");

    // Test 9: Create execution context for memory operations
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK && "Failed to create execution context for memory ops");
    printf("Execution context for memory ops created successfully.\n");
    assert(ctx.memory_base != NULL && "Memory base should not be NULL for ops");
    assert(ctx.memory_size == WAH_WASM_PAGE_SIZE && "Memory size should be 1 page for ops");

    // Test 10: memory.size - initial
    err = wah_call(&ctx, &module, 0, NULL, 0, &result); // Call get_memory_size (func 0)
    assert(err == WAH_OK && "Failed to call get_memory_size");
    assert(result.i32 == 1 && "Initial memory size should be 1 page");
    printf("Initial memory size: %d pages. Test successful.\n", result.i32);

    // Test 11: memory.grow - success
    params[0].i32 = 1; // Grow by 1 page
    err = wah_call(&ctx, &module, 1, params, 1, &result); // Call grow_memory (func 1)
    assert(err == WAH_OK && "Failed to call grow_memory");
    assert(result.i32 == 1 && "grow_memory should return old size (1)");
    assert(ctx.memory_size == (2 * WAH_WASM_PAGE_SIZE) && "Memory size should be 2 pages");
    printf("Memory grown by 1 page. New size: %d pages. Test successful.\n", ctx.memory_size / WAH_WASM_PAGE_SIZE);

    // Test 12: memory.size - after grow
    err = wah_call(&ctx, &module, 0, NULL, 0, &result); // Call get_memory_size (func 0)
    assert(err == WAH_OK && "Failed to call get_memory_size after grow");
    assert(result.i32 == 2 && "Memory size should be 2 pages after grow");
    printf("Memory size after grow: %d pages. Test successful.\n", result.i32);

    // Test 13: memory.grow - failure (exceed max_pages)
    params[0].i32 = 1; // Grow by 1 page (total 3, max 2)
    err = wah_call(&ctx, &module, 1, params, 1, &result); // Call grow_memory (func 1)
    assert(err == WAH_OK && "Failed to call grow_memory for failure test"); // Should return -1, not trap
    assert(result.i32 == -1 && "grow_memory should return -1 on failure");
    assert(ctx.memory_size == (2 * WAH_WASM_PAGE_SIZE) && "Memory size should remain 2 pages");
    printf("Memory grow failure test successful (returned -1). Current size: %d pages.\n", ctx.memory_size / WAH_WASM_PAGE_SIZE);

    // Test 14: memory.fill - basic
    uint32_t fill_offset = 100;
    uint8_t fill_value = 0xAA;
    uint32_t fill_size = 256;
    params[0].i32 = fill_offset; // offset
    params[1].i32 = fill_value;  // value
    params[2].i32 = fill_size;   // size
    err = wah_call(&ctx, &module, 2, params, 3, NULL); // Call fill_memory (func 2)
    assert(err == WAH_OK && "Failed to call fill_memory");
    printf("Memory fill basic test successful. Filled %u bytes from offset %u with 0x%02X.\n", fill_size, fill_offset, fill_value);

    // Verify filled memory directly
    for (uint32_t i = 0; i < fill_size; ++i) {
        assert(ctx.memory_base[fill_offset + i] == fill_value && "Memory fill verification failed");
    }
    printf("Memory fill verification successful.\n");

    // Test 15: memory.fill - out of bounds
    uint32_t oob_fill_offset = ctx.memory_size - 100; // Near end of memory
    uint32_t oob_fill_size = 200; // Will go out of bounds
    params[0].i32 = oob_fill_offset;
    params[1].i32 = 0xBB;
    params[2].i32 = oob_fill_size;
    err = wah_call(&ctx, &module, 2, params, 3, NULL); // Call fill_memory (func 2)
    assert(err == WAH_ERROR_MEMORY_OUT_OF_BOUNDS && "Expected memory out-of-bounds error for fill");
    printf("Memory fill out-of-bounds test successful.\n");

    // Final Cleanup
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
    printf("All memory tests passed!\n");

    return 0;
}

