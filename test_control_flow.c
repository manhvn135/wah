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

static const uint8_t unreachable_i32_add_underflow_fail_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x06, 0x01,       // section 10, size 6, 1 body
    0x04, 0x00,             // body size 4, 0 locals

    // Function body: unreachable i32.add
    0x00,                   // unreachable
    0x6a,                   // i32.add
    0x0b                    // end function
};

// Function: (func (result i32) (unreachable i32.const 0 i32.add))
// This should pass validation because i32.add expects i32s, and i32.const pushes an i32.
static const uint8_t unreachable_i32_i32_add_pass_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x08, 0x01,       // section 10, size 8, 1 body
    0x06, 0x00,             // body size 6, 0 locals

    // Function body: unreachable i32.const 0 i32.add
    0x00,                   // unreachable
    0x41, 0x00,             // i32.const 0
    0x6a,                   // i32.add (expects i32, i32)
    0x0b                    // end function
};

// Function: (func (result i32) (block (result i32) (br 0)))
// This should fail validation because 'br 0' branches to a block that expects an i32 result,
// but the stack is empty when 'br 0' is executed.
static const uint8_t br_empty_stack_fail_wasm[] = {
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

    // Function body: block (result i32) (br 0)
    0x02, 0x7f,             // block (result i32)
    0x0c, 0x00,             // br 0
    0x0b,                   // end block
    0x0b                    // end function
};

// Function: (func (result i32) (block (result i32) (i32.const 42) (br 0)))
// This should pass validation because 'br 0' branches to a block that expects an i32 result,
// and an i32 is on the stack when 'br 0' is executed.
static const uint8_t br_correct_stack_pass_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x0b, 0x01,       // section 10, size 11, 1 body
    0x09, 0x00,             // body size 9, 0 locals

    // Function body: block (result i32) (i32.const 42) (br 0)
    0x02, 0x7f,             // block (result i32)
    0x41, 0x2a,             // i32.const 42
    0x0c, 0x00,             // br 0
    0x0b,                   // end block
    0x0b                    // end function
};

// Function: (func (result i32) (i64.const 0) (return))
// This should fail validation because the function expects an i32 result,
// but an i64 is on the stack when 'return' is executed.
static const uint8_t return_i64_fail_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x07, 0x01,       // section 10, size 7, 1 body
    0x05, 0x00,             // body size 5, 0 locals

    // Function body: i64.const 0 return
    0x42, 0x00,             // i64.const 0
    0x0f,                   // return
    0x0b                    // end function
};

// Function: (func (result i32) (i32.const 42) (return))
// This should pass validation because the function expects an i32 result,
// and an i32 is on the stack when 'return' is executed.
static const uint8_t return_i32_pass_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x05, 0x01,       // section 1, size 5, 1 type
    0x60, 0x00, 0x01, 0x7f, // func type: 0 params, 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x07, 0x01,       // section 10, size 7, 1 body
    0x05, 0x00,             // body size 5, 0 locals

    // Function body: i32.const 42 return
    0x41, 0x2a,             // i32.const 42
    0x0f,                   // return
    0x0b                    // end function
};

static void test_simple_block() {
    printf("Testing simple block...\n");
    
    wah_module_t module;
    wah_error_t err = wah_parse_module(simple_block_wasm, sizeof(simple_block_wasm), &module);
    assert(err == WAH_OK);
    printf("  - Simple block parsed successfully\n");
    
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, NULL, 0, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 42);
    printf("  - Simple block executed, result: %d\n", result.i32);
    
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}

static void test_simple_if_const() {
    printf("Testing simple if (constant)...\n");
    
    wah_module_t module;
    wah_error_t err = wah_parse_module(simple_if_wasm, sizeof(simple_if_wasm), &module);
    assert(err == WAH_OK);
    printf("  - Simple if parsed successfully\n");
    
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, NULL, 0, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 42);
    printf("  - Execution succeeded, result: %d\n", result.i32);
    
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
    printf("  - If branch works (42 -> 1)\n");
    
    // Test else branch (param != 42)
    params[0].i32 = 99;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 0);
    printf("  - Else branch works (99 -> 0)\n");
    
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
        printf("  - Loop works (sum 0..3 = 6)\n");
    }
    
    // Test empty loop
    params[0].i32 = 0;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 0);
    printf("  - Empty loop works (sum 0.. = 0)\n");
    
    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}

// block $a end block $b br $a end (should fail validation)
static const uint8_t br_to_outer_block_fail_wasm[] = {
    // Module header
    0x00, 0x61, 0x73, 0x6d, // Magic
    0x01, 0x00, 0x00, 0x00, // Version

    // Type section
    0x01, // Section ID
    0x04, // Section size
    0x01, // Number of types
    0x60, // Function type
    0x00, // No parameters
    0x00, // No results

    // Function section
    0x03, // Section ID
    0x02, // Section size
    0x01, // Number of functions
    0x00, // Type index 0 (void -> void)

    // Code section
    0x0a, // Section ID
    0x0c, // Section size
    0x01, // Number of code entries
    0x0a, // Code body size
    0x00, // Local count
    0x02, // block (void)
    0x40, // block type (void)
    0x0b, // end (of block $a)
    0x02, // block (void)
    0x40, // block type (void)
    0x0c, // br 1 (branch to block $a) - This should fail as block $a is not on the control stack
    0x01, // label_idx 1
    0x0b, // end (of block $b)
    0x0b, // end (of function)
};

// block $a end block $b br $b end (should pass validation)
static const uint8_t br_to_current_block_pass_wasm[] = {
    // Module header
    0x00, 0x61, 0x73, 0x6d, // Magic
    0x01, 0x00, 0x00, 0x00, // Version

    // Type section
    0x01, // Section ID
    0x04, // Section size
    0x01, // Number of types
    0x60, // Function type
    0x00, // No parameters
    0x00, // No results

    // Function section
    0x03, // Section ID
    0x02, // Section size
    0x01, // Number of functions
    0x00, // Type index 0 (void -> void)

    // Code section
    0x0a, // Section ID
    0x0c, // Section size
    0x01, // Number of code entries
    0x0a, // Code body size
    0x00, // Local count
    0x02, // block (void)
    0x40, // block type (void)
    0x0b, // end (of block $a)
    0x02, // block (void)
    0x40, // block type (void)
    0x0c, // br 0 (branch to block $b) - This should pass
    0x00, // label_idx 0
    0x0b, // end (of block $b)
    0x0b, // end (of function)
};

static const uint8_t br_if_pass_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x06, 0x01,       // section 1, size 6, 1 type
    0x60, 0x01, 0x7f,       // func type: 1 param (i32)
    0x01, 0x7f,             // 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x0f, 0x01,       // section 10, size 15, 1 body
    0x0d, 0x00,             // body size 13, 0 locals

    // Function body: block (result i32) (i32.const 10) (local.get 0) (br_if 0) (i32.const 20)
    0x02, 0x7f,             // block (result i32)
        0x41, 0x0a,         //   i32.const 10
        0x20, 0x00,         //   local.get 0 (condition)
        0x0d, 0x00,         //   br_if 0 (branch to block)
        0x41, 0x14,         //   i32.const 20 (this will be on stack if br_if doesn't branch)
    0x0b,                   // end block
    0x0b                    // end function
};

static const uint8_t br_if_fail_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section
    0x01, 0x06, 0x01,       // section 1, size 6, 1 type
    0x60, 0x01, 0x7e,       // func type: 1 param (i64)
    0x01, 0x7f,             // 1 result (i32)

    // Function section
    0x03, 0x02, 0x01, 0x00, // section 3, size 2, 1 func, type 0

    // Code section
    0x0a, 0x0f, 0x01,       // section 10, size 15, 1 body
    0x0d, 0x00,             // body size 13, 0 locals

    // Function body: block (result i32) (i32.const 10) (local.get 0) (br_if 0) (i32.const 20)
    0x02, 0x7f,             // block (result i32)
        0x41, 0x0a,         //   i32.const 10
        0x20, 0x00,         //   local.get 0 (condition - i64)
        0x0d, 0x00,         //   br_if 0 (branch to block)
        0x41, 0x14,         //   i32.const 20
    0x0b,                   // end block
    0x0b                    // end function
};

static void test_validation_unreachable_br_return() {
    printf("Testing validation for unreachable, br, and return...\n");
    wah_module_t module;
    wah_error_t err;

    // --- Failure Case 1: unreachable i32.add (stack underflow) ---
    err = wah_parse_module(unreachable_i32_add_underflow_fail_wasm, sizeof(unreachable_i32_add_underflow_fail_wasm), &module);
    assert(err == WAH_OK);
    printf("  - Unreachable i32.add (fail) - Validation passed as expected (unreachable context)\n");
    wah_free_module(&module); // Free module even if parsing failed, to clean up any allocated parts

    // --- Success Case 1: unreachable i32.const 0 i32.add (expects i32, got i32) ---
    err = wah_parse_module(unreachable_i32_i32_add_pass_wasm, sizeof(unreachable_i32_i32_add_pass_wasm), &module);
    assert(err == WAH_OK);
    printf("  - Unreachable i32.const 0 i32.add (pass) - Validation passed as expected\n");
    wah_free_module(&module);

    // --- Failure Case 2: br 0 with empty stack for i32 result ---
    err = wah_parse_module(br_empty_stack_fail_wasm, sizeof(br_empty_stack_fail_wasm), &module);
    assert(err == WAH_ERROR_VALIDATION_FAILED);
    printf("  - br 0 with empty stack for i32 result (fail) - Validation failed as expected\n");
    wah_free_module(&module);

    // --- Success Case 2: br 0 with i32 on stack for i32 result ---
    err = wah_parse_module(br_correct_stack_pass_wasm, sizeof(br_correct_stack_pass_wasm), &module);
    assert(err == WAH_OK);
    printf("  - br 0 with i32 on stack for i32 result (pass) - Validation passed as expected\n");
    wah_free_module(&module);

    // --- Failure Case 3: return with i64 on stack for i32 result ---
    err = wah_parse_module(return_i64_fail_wasm, sizeof(return_i64_fail_wasm), &module);
    assert(err == WAH_ERROR_VALIDATION_FAILED);
    printf("  - return with i64 on stack for i32 result (fail) - Validation failed as expected\n");
    wah_free_module(&module);

    // --- Success Case 3: return with i32 on stack for i32 result ---
    err = wah_parse_module(return_i32_pass_wasm, sizeof(return_i32_pass_wasm), &module);
    assert(err == WAH_OK);
    printf("  - return with i32 on stack for i32 result (pass) - Validation passed as expected\n");
    wah_free_module(&module);

    // --- Failure Case 4: br to an outer block that is no longer on the control stack ---
    err = wah_parse_module(br_to_outer_block_fail_wasm, sizeof(br_to_outer_block_fail_wasm), &module);
    assert(err == WAH_ERROR_VALIDATION_FAILED);
    printf("  - br to outer block (fail) - Validation failed as expected\n");
    wah_free_module(&module);

    // --- Success Case 4: br to current block ---
    err = wah_parse_module(br_to_current_block_pass_wasm, sizeof(br_to_current_block_pass_wasm), &module);
    assert(err == WAH_OK);
    printf("  - br to current block (pass) - Validation passed as expected\n");
    wah_free_module(&module);
}

static void test_br_if_validation() {
    printf("Testing br_if validation...\n");
    wah_module_t module;
    wah_error_t err;

    // --- Success Case: br_if with correct i32 condition and i32 result ---
    err = wah_parse_module(br_if_pass_wasm, sizeof(br_if_pass_wasm), &module);
    if (err != WAH_OK) {
        printf("ERROR: br_if_pass_wasm validation failed with error: %s (%d)\n", wah_strerror(err), err);
    }
    assert(err == WAH_OK);
    printf("  - br_if with i32 condition and i32 result (pass) - Validation passed as expected\n");
    wah_free_module(&module);

    // --- Failure Case: br_if with i64 condition (expects i32) ---
    err = wah_parse_module(br_if_fail_wasm, sizeof(br_if_fail_wasm), &module);
    assert(err == WAH_ERROR_VALIDATION_FAILED);
    printf("  - br_if with i64 condition (fail) - Validation failed as expected\n");
    wah_free_module(&module);
}


int main() {
    printf("=== Control Flow Tests ===\n");
    test_simple_block();
    test_simple_if_const();
    test_if_else();
    test_loop();
    test_validation_unreachable_br_return();
    test_br_if_validation();
    printf("=== Control Flow Tests Complete ===\n");
    return 0;
}
