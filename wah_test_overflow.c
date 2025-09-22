#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WAH_IMPLEMENTATION
#include "wah.h"

// --- Test Case 1: ULEB128 value overflow ---
static const uint8_t wasm_binary_u32_overflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00, // Type: () -> ()
    0x03, 0x02, 0x01, 0x00,             // Func: type 0
    0x0a, 0x08, 0x01,                   // Code Section size 8, 1 func
    0x80, 0x80, 0x80, 0x80, 0x10,       // Body size: ULEB128 for UINT32_MAX + 1
    0x00, 0x0b,                         // Body content
};

// --- Test Case 2: SLEB128 value overflow ---
static const uint8_t wasm_binary_s32_overflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00, // Type: () -> ()
    0x03, 0x02, 0x01, 0x00,             // Func: type 0
    0x0a, 0x09, 0x01,                   // Code Section size 9, 1 func
    0x07, 0x00,                         // body size 7, 0 locals
    0x41,                               // i32.const
    0x80, 0x80, 0x80, 0x80, 0x08,       // Value > INT32_MAX
    0x0b,                               // end
};

// --- Test Case 3: SLEB128 value underflow ---
static const uint8_t wasm_binary_s32_underflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00, // Type: () -> ()
    0x03, 0x02, 0x01, 0x00,             // Func: type 0
    0x0a, 0x09, 0x01,                   // Code Section size 9, 1 func
    0x07, 0x00,                         // body size 7, 0 locals
    0x41,                               // i32.const
    0xff, 0xff, 0xff, 0xff, 0x77,       // Value < INT32_MIN
    0x0b,                               // end
};

// --- Test Case 4: Element segment address overflow ---
// This tests if (offset + num_elems) overflows the table bounds.
// The offset is a valid sleb128, but the resulting address is out of bounds.
static const uint8_t wasm_binary_elem_overflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,             // Type: () -> ()
    0x03, 0x02, 0x01, 0x00,                         // Func: type 0
    // Table: 1 table, min size 1000
    0x04, 0x05, 0x01, 0x70, 0x00, 0xe8, 0x07,
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,             // Code: 1 func, size 2, 0 locals, end
    // Element Section
    0x09, 0x1b, 0x01,
    0x00,                                           // table index 0
    // offset expression: i32.const 990
    0x41, 0xf6, 0x07, 0x0b,
    // vector of 20 function indices
    0x14, // 20 elements
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

// --- Test Case 5: Local count overflow in wah_parse_code_section ---
// This tests if current_local_count += local_type_count overflows uint32_t
// leading to an incorrect local_count and subsequent allocation failure or
// out-of-bounds access.
static const uint8_t wasm_binary_local_count_overflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // Magic + Version
    // Type Section
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,             // Type: () -> ()
    // Function Section
    0x03, 0x02, 0x01, 0x00,                         // Func: type 0
    // Code Section
    0x0a, 0x0c, 0x01,                               // Code Section ID, size 12, 1 func
        0x0a,                                       // Code Body 0 size (10 bytes)
            0x02,                                   // num_local_entries = 2
            // Entry 1: local_type_count = 0xFFFFFFFF (5 bytes ULEB128)
            0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x7F,      // i32 type
            // Entry 2: local_type_count = 1 (1 byte ULEB128)
            0x01, 0x7F,                             // i32 type
        0x0B                                        // Function Body: END opcode
};

int main(void) {
    wah_module_t module;
    wah_error_t err;

    printf("--- Running Overflow Tests ---\n");

    printf("1. Testing ULEB128 value overflow...\n");
    err = wah_parse_module(wasm_binary_u32_overflow, sizeof(wasm_binary_u32_overflow), &module);
    assert(err == WAH_ERROR_TOO_LARGE);
    printf("  - PASSED\n");

    printf("2. Testing SLEB128 value overflow...\n");
    err = wah_parse_module(wasm_binary_s32_overflow, sizeof(wasm_binary_s32_overflow), &module);
    assert(err == WAH_ERROR_TOO_LARGE);
    printf("  - PASSED\n");

    printf("3. Testing SLEB128 value underflow...\n");
    err = wah_parse_module(wasm_binary_s32_underflow, sizeof(wasm_binary_s32_underflow), &module);
    assert(err == WAH_ERROR_TOO_LARGE);
    printf("  - PASSED\n");

    printf("4. Testing element segment address overflow...\n");
    err = wah_parse_module(wasm_binary_elem_overflow, sizeof(wasm_binary_elem_overflow), &module);
    assert(err == WAH_ERROR_VALIDATION_FAILED);
    if (err == WAH_OK) {
        printf("  - FAILED: Module parsing succeeded unexpectedly!\n");
        wah_free_module(&module);
        return 1;
    } else {
        printf("  - PASSED: Module parsing failed as expected with error: %s\n", wah_strerror(err));
    }

    printf("5. Testing local count overflow...\n");
    err = wah_parse_module(wasm_binary_local_count_overflow, sizeof(wasm_binary_local_count_overflow), &module);
    assert(err == WAH_ERROR_TOO_LARGE); // Expecting TOO_LARGE due to overflow check
    printf("  - PASSED\n");

    printf("--- All Overflow Tests Passed ---\n");

    return 0;
}
