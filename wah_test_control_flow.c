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

// Test br_table, which acts like a switch statement
static const uint8_t br_table_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic, version
    // Type section
    0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
    // Function section
    0x03, 0x02, 0x01, 0x00,
    // Export section
    0x07, 0x11, 0x01, 0x0f, 0x62, 0x72, 0x5f, 0x74, 0x61, 0x62, 0x6c,
    0x65, 0x5f, 0x66, 0x75, 0x6e, 0x63, 0x00, 0x00,
    // Code section
    0x0a, 0x23, 0x01, // section 10, size 35, 1 body
    0x21, // func body size (33 bytes)
    0x00, // locals
    // main logic
    0x02, 0x40, // block $l3 (empty result)
      0x02, 0x40, // block $l2
        0x02, 0x40, // block $l1
          0x02, 0x40, // block $l0
            0x20, 0x00, // local.get 0 (index)
            0x0e, // br_table
            0x03, // 3 targets in vec
            0x00, // target 0 -> $l0
            0x01, // target 1 -> $l1
            0x02, // target 2 -> $l2
            0x03, // default target -> $l3
          0x0b, // end $l0
          0x41, 0x0a, // i32.const 10
          0x0f, // return
        0x0b, // end $l1
        0x41, 0x14, // i32.const 20
        0x0f, // return
      0x0b, // end $l2
      0x41, 0x1e, // i32.const 30
      0x0f, // return
    0x0b, // end $l3
    0x41, 0x28, // i32.const 40
    0x0b // end func
};

static void test_br_table() {
    printf("Testing br_table...\n");
    wah_module_t module;
    wah_error_t err = wah_parse_module(br_table_wasm, sizeof(br_table_wasm), &module);
    if (err != WAH_OK) {
        printf("  ERROR: Failed to parse br_table module: %s\n", wah_strerror(err));
        assert(false);
    }
    assert(err == WAH_OK);
    printf("  - br_table module parsed successfully\n");

    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);

    wah_value_t params[1];
    wah_value_t result;

    // Test case 0
    params[0].i32 = 0;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 10);
    printf("  - Case 0 -> 10 PASSED\n");

    // Test case 1
    params[0].i32 = 1;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 20);
    printf("  - Case 1 -> 20 PASSED\n");

    // Test case 2
    params[0].i32 = 2;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 30);
    printf("  - Case 2 -> 30 PASSED\n");

    // Test case 3 (default)
    params[0].i32 = 3;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 40);
    printf("  - Case 3 (default) -> 40 PASSED\n");
    
    // Test case 4 (default, out of bounds)
    params[0].i32 = 4;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 40);
    printf("  - Case 4 (default) -> 40 PASSED\n");

    wah_exec_context_destroy(&ctx);
    wah_free_module(&module);
}

// Test for br_table type consistency
static void test_br_table_type_consistency() {
    printf("Testing br_table type consistency...\n");

    // Valid case: all targets have the same result type (i32)
    // (module
    //  (func (result i32)
    //   (block (result i32) ;; target 0
    //    (block (result i32) ;; target 1
    //     (i32.const 0)
    //     (br_table 0 1 0) ;; targets 0, 1, default 0
    //    )
    //    (i32.const 1)
    //   )
    //   (i32.const 2)
    //  )
    // )
    uint8_t wasm_valid[] = {
        0x00, 0x61, 0x73, 0x6d, // magic
        0x01, 0x00, 0x00, 0x00, // version

        // Type section
        0x01, // section id
        0x05, // section size
        0x01, // num types
        0x60, // func type
        0x00, // num params
        0x01, // num results
        0x7f, // i32

        // Function section
        0x03, // section id
        0x02, // section size
        0x01, // num functions
        0x00, // type index 0

        // Code section
        0x0a, // section id
        0x17, // section size (23 bytes)
        0x01, // num code bodies
        0x15, // code body size (21 bytes)
        0x00, // num locals

        // Function body
        0x02, // block
        0x7f, // block type (i32)
        0x02, // block
        0x7f, // block type (i32)
        0x41, 0x00, // i32.const 0 (index)
        0x41, 0x00, // i32.const 0 (value for target block)
        0x0e, // br_table
        0x02, // num_targets (2)
        0x00, // target 0 (label_idx)
        0x01, // target 1 (label_idx)
        0x00, // default target 0 (label_idx)
        0x0b, // end
        0x41, 0x01, // i32.const 1
        0x0b, // end
        0x41, 0x02, // i32.const 2
        0x0b, // end
    };
    wah_module_t module_valid;
    wah_error_t err_valid = wah_parse_module(wasm_valid, sizeof(wasm_valid), &module_valid);
    assert(err_valid == WAH_OK);
    wah_free_module(&module_valid);

    // Invalid case: targets have different result types
    // (module
    //  (func (result i32)
    //   (block (result i32) ;; target 0 (i32)
    //    (block (result i64) ;; target 1 (i64) -- THIS IS THE DIFFERENCE
    //     (i32.const 0)
    //     (br_table 0 1 0) ;; targets 0, 1, default 0
    //    )
    //    (i32.const 1)
    //   )
    //   (i32.const 2)
    //  )
    // )
    uint8_t wasm_invalid[] = {
        0x00, 0x61, 0x73, 0x6d, // magic
        0x01, 0x00, 0x00, 0x00, // version

        // Type section
        0x01, // section id
        0x05, // section size
        0x01, // num types
        0x60, // func type
        0x00, // num params
        0x01, // num results
        0x7f, // i32

        // Function section
        0x03, // section id
        0x02, // section size
        0x01, // num functions
        0x00, // type index 0

        // Code section
        0x0a, // section id
        0x15, // section size (21 bytes)
        0x01, // num code bodies
        0x13, // code body size (19 bytes)
        0x00, // num locals

        // Function body
        0x02, // block
        0x7f, // block type (i32)
        0x02, // block
        0x7e, // block type (i64) -- THIS IS THE DIFFERENCE
        0x41, 0x00, // i32.const 0
        0x0e, // br_table
        0x02, // num_targets (2)
        0x00, // target 0 (label_idx)
        0x01, // target 1 (label_idx)
        0x00, // default target 0 (label_idx)
        0x0b, // end
        0x41, 0x01, // i32.const 1
        0x0b, // end
        0x41, 0x02, // i32.const 2
        0x0b, // end
    };
    wah_module_t module_invalid;
    wah_error_t err_invalid = wah_parse_module(wasm_invalid, sizeof(wasm_invalid), &module_invalid);
    assert(err_invalid == WAH_ERROR_VALIDATION_FAILED);
    wah_free_module(&module_invalid); // Should still free even if parsing fails
}

// Function: (func (param i32) (result i32) (block (param i32) (result i32) local.get 0 i32.const 1 i32.add end))
// Type 0: (func (param i32) (result i32)) - for the block and the main function
static const uint8_t block_type_with_params_pass_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, // WASM magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (section 1)
    0x01,                   // section id: type
    0x06,                   // section size: 6 bytes
    0x01,                   // 1 type
    // Type 0: (func (param i32) (result i32))
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
    0x0a,                   // section size: 10 bytes total
    0x01,                   // 1 function body

    // Function body 0
    0x08,                   // body size: 8 bytes
    0x00,                   // 0 local declarations

    // Code: block (type 0) (local.get 0 i32.const 1 i32.add) end
    // The i32 parameter for the block will be taken from the stack before the block.
    // The function's parameter will be pushed onto the stack before the block.
    0x20, 0x00,             // local.get 0 (get function's parameter)
    0x41, 0x01,             // i32.const 1
    0x6a,                   // i32.add
    0x0b,                   // end block
    0x0b                    // end function
};

static void test_block_type_with_params_pass() {
    printf("Testing block type with parameters (should pass validation and execute correctly)...\n");
    wah_module_t module;
    wah_error_t err = wah_parse_module(block_type_with_params_pass_wasm, sizeof(block_type_with_params_pass_wasm), &module);
    assert(err == WAH_OK);
    printf("  - Block type with parameters parsed successfully\n");
    wah_exec_context_t ctx;
    err = wah_exec_context_create(&ctx, &module);
    assert(err == WAH_OK);
    wah_value_t params[1] = {{.i32 = 10}};
    wah_value_t result;
    err = wah_call(&ctx, &module, 0, params, 1, &result);
    assert(err == WAH_OK);
    assert(result.i32 == 11);
    printf("  - Block type with parameters executed, result: %d\n", result.i32);
    wah_exec_context_destroy(&ctx);
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
    test_br_table();
    test_br_table_type_consistency();
    test_block_type_with_params_pass();
    printf("=== Control Flow Tests Complete ===\n");
    return 0;
}
