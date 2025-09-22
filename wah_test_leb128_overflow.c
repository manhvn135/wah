#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WAH_IMPLEMENTATION
#include "wah.h"

// This WASM module is crafted to fail parsing due to a ULEB128 overflow.
// It contains a code section with a function body size that is a valid
// 5-byte ULEB128 encoding, but the decoded value (UINT32_MAX + 1)
// is too large to fit in a uint32_t.
static const uint8_t wasm_binary_u32_overflow[] = {
    // Magic + Version
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    // Type Section (id 1)
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00, // () -> ()
    // Function Section (id 3)
    0x03, 0x02, 0x01, 0x00,
    // Code Section (id 10)
    0x0a, 0x08, 0x01, // Section size 8, 1 function
    // Body size: ULEB128 for 4294967296 (UINT32_MAX + 1)
    0x80, 0x80, 0x80, 0x80, 0x10,
    // Body content (not reached)
    0x00, // 0 locals
    0x0b, // end
};

// This tests signed 32-bit overflow.
// The value is 2147483648 (INT32_MAX + 1), encoded as 80 80 80 80 08
static const uint8_t wasm_binary_s32_overflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00,
    // Code Section with an i32.const that overflows
    0x0a, 0x0a, 0x01, // Section size 10, 1 function
    0x08, 0x00, // body size 8, 0 locals
    0x41, // i32.const
    // Value > INT32_MAX
    0x80, 0x80, 0x80, 0x80, 0x08,
    0x0b, // end
};

// This tests signed 32-bit underflow.
// The value is -2147483649 (INT32_MIN - 1), encoded as ff ff ff ff 77
static const uint8_t wasm_binary_s32_underflow[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00,
    // Code Section with an i32.const that underflows
    0x0a, 0x0a, 0x01, // Section size 10, 1 function
    0x08, 0x00, // body size 8, 0 locals
    0x41, // i32.const
    // Value < INT32_MIN
    0xff, 0xff, 0xff, 0xff, 0x77,
    0x0b, // end
};


int main(void) {
    wah_module_t module;
    wah_error_t err;

    printf("--- Running LEB128 Overflow Test ---\n");

    printf("Testing ULEB128 overflow...\n");
    err = wah_parse_module(wasm_binary_u32_overflow, sizeof(wasm_binary_u32_overflow), &module);
    // Before the fix, this might be WAH_ERROR_UNEXPECTED_EOF or another validation error
    // After the fix, it MUST be WAH_ERROR_TOO_LARGE
    assert(err == WAH_ERROR_TOO_LARGE);
    printf("  - PASSED: Correctly failed with WAH_ERROR_TOO_LARGE\n");

    printf("Testing SLEB128 overflow...\n");
    err = wah_parse_module(wasm_binary_s32_overflow, sizeof(wasm_binary_s32_overflow), &module);
    assert(err == WAH_ERROR_TOO_LARGE);
    printf("  - PASSED: Correctly failed with WAH_ERROR_TOO_LARGE\n");

    printf("Testing SLEB128 underflow...\n");
    err = wah_parse_module(wasm_binary_s32_underflow, sizeof(wasm_binary_s32_underflow), &module);
    assert(err == WAH_ERROR_TOO_LARGE);
    printf("  - PASSED: Correctly failed with WAH_ERROR_TOO_LARGE\n");

    printf("--- LEB128 Overflow Test Passed ---\n");

    return 0;
}
