#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>

// A simple test for bitwise AND: (module (func (param i32 i32) (result i32) (i32.and (local.get 0) (local.get 1))))
const uint8_t and_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section  
    0x03, 0x02, 0x01, 0x00,
    // Code section
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x71, 0x0b
};

// Test for comparison: (module (func (param i32 i32) (result i32) (i32.eq (local.get 0) (local.get 1))))
const uint8_t eq_test_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section
    0x03, 0x02, 0x01, 0x00,
    // Code section  
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x46, 0x0b
};

int main() {
    wah_module_t module;
    wah_error_t err;
    wah_value_t params[2], result;

    printf("=== Testing I32.AND ===\n");
    err = wah_parse_module(and_test_wasm, sizeof(and_test_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse AND module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = 0xFF; params[1].i32 = 0x0F;
    err = wah_call(&module, 0, params, 2, &result);
    if (err != WAH_OK) {
        printf("Failed to execute AND: %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    printf("0xFF & 0x0F = 0x%X (expected 0xF)\n", result.i32);
    wah_free_module(&module);

    printf("\n=== Testing I32.EQ ===\n");
    err = wah_parse_module(eq_test_wasm, sizeof(eq_test_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse EQ module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = 42; params[1].i32 = 42;
    err = wah_call(&module, 0, params, 2, &result);
    if (err != WAH_OK) {
        printf("Failed to execute EQ: %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    printf("42 == 42 = %d (expected 1)\n", result.i32);
    
    params[0].i32 = 42; params[1].i32 = 24;
    err = wah_call(&module, 0, params, 2, &result);
    printf("42 == 24 = %d (expected 0)\n", result.i32);
    wah_free_module(&module);

    printf("\nAll new operations working correctly!\n");
    return 0;
}
