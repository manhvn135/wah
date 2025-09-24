#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WAH_IMPLEMENTATION
#include "wah.h"

// A simple WASM binary with a table and call_indirect
// (module
//   (type $void_i32 (func (param i32)))
//   (type $i32_i32 (func (param i32) (result i32)))
//   (func $add_one (type $i32_i32) (param $p0 i32) (result i32)
//     local.get $p0
//     i32.const 1
//     i32.add)
//   (func $sub_one (type $i32_i32) (param $p0 i32) (result i32)
//     local.get $p0
//     i32.const 1
//     i32.sub)
//   (table $t0 2 funcref)
//   (elem (i32.const 0) func $add_one func $sub_one)
//   (func $call_indirect_add (type $i32_i32) (param $p0 i32) (result i32)
//     local.get $p0
//     i32.const 0 ;; function index in table
//     call_indirect (type $i32_i32))
//   (func $call_indirect_sub (type $i32_i32) (param $p0 i32) (result i32)
//     local.get $p0
//     i32.const 1 ;; function index in table
//     call_indirect (type $i32_i32))
//   (export "add_one" (func $add_one))
//   (export "sub_one" (func $sub_one))
//   (export "call_indirect_add" (func $call_indirect_add))
//   (export "call_indirect_sub" (func $call_indirect_sub))
// )
// Manually crafted WASM binary for the above WAT
const uint8_t wasm_binary_table_indirect_call[] = {
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // Type section (1)
    0x01, // section id
    0x0b, // section size
    0x02, // num types
    // Type 0: (func (param i32) (result i32))
    0x60, 0x01, 0x7f, 0x01, 0x7f,
    // Type 1: (func (param i32) (result i32)) - same as type 0 for simplicity
    0x60, 0x01, 0x7f, 0x01, 0x7f,

    // Function section (3)
    0x03, // section id
    0x05, // section size
    0x04, // num functions (add_one, sub_one, call_indirect_add, call_indirect_sub)
    0x00, // func 0 (add_one) -> type 0
    0x00, // func 1 (sub_one) -> type 0
    0x00, // func 2 (call_indirect_add) -> type 0
    0x00, // func 3 (call_indirect_sub) -> type 0

    // Table section (4)
    0x04, // section id
    0x04, // section size
    0x01, // num tables
    0x70, // element type: funcref
    0x00, // flags: 0x00 (fixed size)
    0x02, // min elements: 2
    // max elements is min_elements if flags is 0x00

    // Export section (7)
    0x07, // section id
    0x3d, // section size (61 bytes)
    0x04, // num exports
    // export "add_one" func 0
    0x07, 0x61, 0x64, 0x64, 0x5f, 0x6f, 0x6e, 0x65, 0x00, 0x00,
    // export "sub_one" func 1
    0x07, 0x73, 0x75, 0x62, 0x5f, 0x6f, 0x6e, 0x65, 0x00, 0x01,
    // export "call_indirect_add" func 2
    0x11, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x5f, 0x61, 0x64, 0x64, 0x00, 0x02,
    // export "call_indirect_sub" func 3
    0x11, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x5f, 0x73, 0x75, 0x62, 0x00, 0x03,

    // Element section (9)
    0x09, // section id
    0x08, // section size (8 bytes)
    0x01, // num elements
    0x00, // table index 0
    0x41, 0x00, 0x0b, // offset expr: i32.const 0 end
    0x02, // num_elems
    0x00, // func index 0 ($add_one)
    0x01, // func index 1 ($sub_one)

    // Code section (10)
    0x0a, // section id
    0x25, // section size (37 bytes)
    0x04, // num code bodies

    // Code body for func 0 ($add_one)
    0x07, // body size
    0x00, // num locals
    0x20, 0x00, // local.get 0
    0x41, 0x01, // i32.const 1
    0x6a,       // i32.add
    0x0b,       // end

    // Code body for func 1 ($sub_one)
    0x07, // body size
    0x00, // num locals
    0x20, 0x00, // local.get 0
    0x41, 0x01, // i32.const 1
    0x6b,       // i32.sub
    0x0b,       // end

    0x09, // body size (9 bytes)
    0x00, // num locals
    0x20, 0x00, // local.get 0 (param for indirect call)
    0x41, 0x00, // i32.const 0 (function index in table)
    0x11, 0x00, 0x00, // call_indirect (type 0, table 0)
    0x0b,       // end

    // Code body for func 3 ($call_indirect_sub)
    0x09, // body size (9 bytes)
    0x00, // num locals
    0x20, 0x00, // local.get 0 (param for indirect call)
    0x41, 0x01, // i32.const 1 (function index in table)
    0x11, 0x00, 0x00, // call_indirect (type 0, table 0)
    0x0b,       // end
};

void wah_test_table_indirect_call() {
    printf("Running wah_test_table_indirect_call...\n");

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary_table_indirect_call, sizeof(wasm_binary_table_indirect_call), &module);
    assert(err == WAH_OK);

    wah_exec_context_t exec_ctx;
    err = wah_exec_context_create(&exec_ctx, &module);
    assert(err == WAH_OK);

    // Find the exported function indices
    uint32_t add_one_func_idx = (uint32_t)-1;
    uint32_t sub_one_func_idx = (uint32_t)-1;
    uint32_t call_indirect_add_func_idx = (uint32_t)-1;
    uint32_t call_indirect_sub_func_idx = (uint32_t)-1;

    // NOTE: Export section parsing is not implemented yet, so we'll hardcode for now
    // Based on the WASM binary, func 0 is add_one, func 1 is sub_one, func 2 is call_indirect_add, func 3 is call_indirect_sub
    add_one_func_idx = 0;
    sub_one_func_idx = 1;
    call_indirect_add_func_idx = 2;
    call_indirect_sub_func_idx = 3;

    (void)add_one_func_idx; // Suppress warning
    (void)sub_one_func_idx; // Suppress warning

    // Test call_indirect_add (calls add_one indirectly)
    wah_value_t params_add[1] = {{.i32 = 10}};
    wah_value_t result_add;
    err = wah_call(&exec_ctx, &module, call_indirect_add_func_idx, params_add, 1, &result_add);
    assert(err == WAH_OK);
    assert(result_add.i32 == 11);

    // Test call_indirect_sub (calls sub_one indirectly)
    wah_value_t params_sub[1] = {{.i32 = 10}};
    wah_value_t result_sub;
    err = wah_call(&exec_ctx, &module, call_indirect_sub_func_idx, params_sub, 1, &result_sub);
    assert(err == WAH_OK);
    assert(result_sub.i32 == 9);

    wah_exec_context_destroy(&exec_ctx);
    wah_free_module(&module);

    printf("wah_test_table_indirect_call passed.\n");
}

int main() {
    wah_test_table_indirect_call();
    return 0;
}
