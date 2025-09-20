#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <assert.h>

// Test just a block with no branches: block { i32.const 42 } end
static const uint8_t simple_block_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version
    
    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)
    
    // Function section  
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0
    
    // Code section
    0x0a, 0x09, 0x01,       // section 10, size 9, 1 body
    0x07, 0x00,             // body size 7, 0 locals
    
    // Function body: block { i32.const 42 } end
    0x02, 0x7f,             // block (result i32)
        0x41, 0x2a,         //   i32.const 42
    0x0b,                   // end block
    0x0b                    // end function
};

// Based on working simple_block_wasm, create a simple if
static const uint8_t simple_if_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version
    
    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)
    
    // Function section  
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0
    
    // Code section
    0x0a, 0x0e, 0x01,       // section 10, size 14, 1 body
    0x0c, 0x00,             // body size 12, 0 locals
    
    // Function body: if (1) { 42 } else { 99 }
    0x41, 0x01,             // i32.const 1
    0x04, 0x7f,             // if (result i32)
        0x41, 0x2a,         //   i32.const 42
    0x05,                   // else
        0x41, 0x63,         //   i32.const 99
    0x0b,                   // end if
    0x0b                    // end function
};

// Test a simple if-else: if (param == 42) return 1; else return 0;
static const uint8_t if_else_wasm[] = {
    // WASM header
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version 1
    
    // Type section (section 1)
    0x01,                   // section id: type
    0x06,                   // section size: 6 bytes
    0x01,                   // 1 type
    0x60,                   // func type
    0x01, 0x7f,             // 1 param: i32
    0x01, 0x7f,             // 1 result: i32
    
    // Function section (section 3)  
    0x03,                   // section id: function
    0x02,                   // section size: 2 bytes
    0x01,                   // 1 function
    0x00,                   // function 0 uses type 0
    
    // Code section (section 10)
    0x0a,                   // section id: code
    0x11,                   // section size: 17 bytes total
    0x01,                   // 1 function body
    
    // Function body
    0x0f,                   // body size: 15 bytes
    0x00,                   // 0 local declarations
    
    // Code: if (local.get 0 == 42) { return 1 } else { return 0 }
    0x20, 0x00,             // local.get 0 (get parameter)
    0x41, 0x2a,             // i32.const 42
    0x46,                   // i32.eq (compare)
    0x04, 0x7f,             // if (result type i32)
        0x41, 0x01,         //   i32.const 1
    0x05,                   // else
        0x41, 0x00,         //   i32.const 0
    0x0b,                   // end if
    0x0b                    // end function
};

// Test a simple loop: for(i=0; i<param; i++) sum+=i; return sum;
static const uint8_t loop_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version
    
    // Type section
    0x01, 0x06, 0x01,       // section 1, size 6, 1 type
    0x60, 0x01, 0x7f,       // func type: 1 param (i32)
    0x01, 0x7f,             // 1 result (i32)
    
    // Function section  
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0
    
    // Code section
    0x0a, 0x2d, 0x01,       // section 10, size 45, 1 body
    0x2b,                   // body size 43
    0x01,                   // 1 local declaration entry
    0x02, 0x7f,             // 2 locals of type i32 (sum, i)
    
    // Function body: sum=0, i=0; loop { if i>=param break; sum+=i; i++; br 0 }
    0x41, 0x00,             // i32.const 0
    0x21, 0x01,             // local.set 1 (sum = 0)
    0x41, 0x00,             // i32.const 0  
    0x21, 0x02,             // local.set 2 (i = 0)
    0x02, 0x40,             // block
        0x03, 0x40,         //   loop
            0x20, 0x02,     //     local.get 2 (i)
            0x20, 0x00,     //     local.get 0 (param)
            0x4e,           //     i32.ge_s
            0x0d, 0x01,     //     br_if 1 (break outer block)
            0x20, 0x01,     //     local.get 1 (sum)
            0x20, 0x02,     //     local.get 2 (i)
            0x6a,           //     i32.add
            0x21, 0x01,     //     local.set 1 (sum += i)
            0x20, 0x02,     //     local.get 2 (i)
            0x41, 0x01,     //     i32.const 1
            0x6a,           //     i32.add
            0x21, 0x02,     //     local.set 2 (i++)
            0x0c, 0x00,     //     br 0 (continue loop)
        0x0b,               //   end loop
    0x0b,                   // end block
    0x20, 0x01,             // local.get 1 (return sum)
    0x0b                    // end function
};


static void test_simple_block() {
    printf("Testing simple block...\n");
    
    wah_module_t module;
    wah_error_t err = wah_parse_module(simple_block_wasm, sizeof(simple_block_wasm), &module);
    assert(err == WAH_OK);
    printf("  ✓ Simple block parsed successfully\n");
    
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, NULL, 0, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 42);
    printf("  ✓ Simple block executed, result: %d\n", result.i32);
    
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}

static void test_simple_if_const() {
    printf("Testing simple if (constant)...\n");
    
    wah_module_t module;
    wah_error_t err = wah_parse_module(simple_if_wasm, sizeof(simple_if_wasm), &module);
    assert(err == WAH_OK);
    printf("  ✓ Simple if parsed successfully\n");
    
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, NULL, 0, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 42);
    printf("  ✓ Execution succeeded, result: %d\n", result.i32);
    
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}

static void test_if_else() {
    printf("Testing if-else (parameter)...\n");
    
    wah_module_t module;
    wah_error_t err = wah_parse_module(if_else_wasm, sizeof(if_else_wasm), &module);
    assert(err == WAH_OK);
    
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    
    // Test if branch (param == 42)
    wah_value_t params[1] = {{.i32 = 42}};
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 1);
    printf("  ✓ If branch works (42 -> 1)\n");
    
    // Test else branch (param != 42)
    params[0].i32 = 99;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 0);
    printf("  ✓ Else branch works (99 -> 0)\n");
    
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}

static void test_loop() {
    printf("Testing loop control flow...\n");
    
    wah_module_t module;
    wah_error_t err = wah_parse_module(loop_wasm, sizeof(loop_wasm), &module);
    if (err != WAH_OK) {
        printf("  ERROR: Failed to parse loop module: %s\n", wah_strerror(err));
        assert(false);
        return;
    }
    
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    
    // Test loop: sum of 0..4 = 0+1+2+3 = 6
    wah_value_t params[1] = {{.i32 = 4}};
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    if (err != WAH_OK) {
        printf("  ERROR: Loop function call failed: %s\n", wah_strerror(err));
        assert(false);
    } else if (result.i32 != 6) {
        printf("  ERROR: Expected 6, got %d\n", result.i32);
        assert(false);
    } else {
        printf("  ✓ Loop works (sum 0..3 = 6)\n");
    }
    
    // Test empty loop
    params[0].i32 = 0;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 0);
    printf("  ✓ Empty loop works (sum 0.. = 0)\n");
    
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}


int main() {
    printf("=== Control Flow Tests ===\n");
    test_simple_block();
    test_simple_if_const();
    test_if_else();
    test_loop();
    printf("=== Control Flow Tests Complete ===\n");
    return 0;
}
