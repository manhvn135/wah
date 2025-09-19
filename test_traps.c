#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <stdint.h>

// Test for division by zero: (module (func (param i32 i32) (result i32) (i32.div_s (local.get 0) (local.get 1))))
const uint8_t div_by_zero_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section  
    0x03, 0x02, 0x01, 0x00,
    // Code section - i32.div_s opcode is 0x6D
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6d, 0x0b
};

// Test for signed overflow: (module (func (param i32 i32) (result i32) (i32.div_s (local.get 0) (local.get 1))))
// Same binary as above, will test with INT_MIN / -1
const uint8_t signed_overflow_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section  
    0x03, 0x02, 0x01, 0x00,
    // Code section - i32.div_s opcode is 0x6D
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6d, 0x0b
};

// Test for unsigned division by zero: (module (func (param i32 i32) (result i32) (i32.div_u (local.get 0) (local.get 1))))
const uint8_t div_u_by_zero_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section  
    0x03, 0x02, 0x01, 0x00,
    // Code section - i32.div_u opcode is 0x6E
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6e, 0x0b
};

// Test for remainder by zero: (module (func (param i32 i32) (result i32) (i32.rem_s (local.get 0) (local.get 1))))
const uint8_t rem_by_zero_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version
    // Type section
    0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
    // Function section  
    0x03, 0x02, 0x01, 0x00,
    // Code section - i32.rem_s opcode is 0x6F
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6f, 0x0b
};

int main() {
    wah_module_t module;
    wah_error_t err;
    wah_value_t params[2], result;

    printf("=== Testing Division by Zero Traps ===\n");

    // Test signed division by zero
    printf("\n1. Testing i32.div_s with division by zero:\n");
    err = wah_parse_module(div_by_zero_wasm, sizeof(div_by_zero_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse div_by_zero module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = 42; 
    params[1].i32 = 0; // Division by zero
    err = wah_call(&module, 0, params, 2, &result);
    if (err == WAH_ERROR_TRAP) {
        printf("✓ Correctly trapped on division by zero (error %d)\n", err);
    } else {
        printf("✗ Expected trap but got error %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    wah_free_module(&module);

    // Test signed integer overflow
    printf("\n2. Testing i32.div_s with signed integer overflow:\n");
    err = wah_parse_module(signed_overflow_wasm, sizeof(signed_overflow_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse signed_overflow module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = INT32_MIN; 
    params[1].i32 = -1; // This causes overflow: INT_MIN / -1 = +2^31 (unrepresentable)
    err = wah_call(&module, 0, params, 2, &result);
    if (err == WAH_ERROR_TRAP) {
        printf("✓ Correctly trapped on signed integer overflow (error %d)\n", err);
    } else {
        printf("✗ Expected trap but got error %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    wah_free_module(&module);

    // Test unsigned division by zero
    printf("\n3. Testing i32.div_u with division by zero:\n");
    err = wah_parse_module(div_u_by_zero_wasm, sizeof(div_u_by_zero_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse div_u_by_zero module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = 100; 
    params[1].i32 = 0; // Division by zero
    err = wah_call(&module, 0, params, 2, &result);
    if (err == WAH_ERROR_TRAP) {
        printf("✓ Correctly trapped on unsigned division by zero (error %d)\n", err);
    } else {
        printf("✗ Expected trap but got error %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    wah_free_module(&module);

    // Test remainder by zero
    printf("\n4. Testing i32.rem_s with division by zero:\n");
    err = wah_parse_module(rem_by_zero_wasm, sizeof(rem_by_zero_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse rem_by_zero module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = 7; 
    params[1].i32 = 0; // Division by zero
    err = wah_call(&module, 0, params, 2, &result);
    if (err == WAH_ERROR_TRAP) {
        printf("✓ Correctly trapped on remainder by zero (error %d)\n", err);
    } else {
        printf("✗ Expected trap but got error %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    wah_free_module(&module);

    // Test that valid operations still work
    printf("\n5. Testing that valid operations still work:\n");
    err = wah_parse_module(div_by_zero_wasm, sizeof(div_by_zero_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse module for valid test: %d\n", err);
        return 1;
    }
    
    params[0].i32 = 20; 
    params[1].i32 = 4; // Valid division: 20 / 4 = 5
    err = wah_call(&module, 0, params, 2, &result);
    if (err == WAH_OK) {
        printf("✓ Valid division works: 20 / 4 = %d\n", result.i32);
    } else {
        printf("✗ Valid division failed with error %d\n", err);
        wah_free_module(&module);
        return 1;
    }
    wah_free_module(&module);

    // Test that INT_MIN % -1 = 0 (doesn't trap per spec)
    printf("\n6. Testing i32.rem_s with INT_MIN %% -1 (should return 0, not trap):\n");
    err = wah_parse_module(rem_by_zero_wasm, sizeof(rem_by_zero_wasm), &module);
    if (err != WAH_OK) {
        printf("Failed to parse rem module: %d\n", err);
        return 1;
    }
    
    params[0].i32 = INT32_MIN; 
    params[1].i32 = -1; // This should return 0, not trap
    err = wah_call(&module, 0, params, 2, &result);
    if (err == WAH_OK && result.i32 == 0) {
        printf("✓ INT_MIN %% -1 correctly returns 0 (no trap)\n");
    } else {
        printf("✗ INT_MIN %% -1 failed: error %d, result %d\n", err, result.i32);
        wah_free_module(&module);
        return 1;
    }
    wah_free_module(&module);

    printf("\n=== All trap tests passed! ===\n");
    return 0;
}
