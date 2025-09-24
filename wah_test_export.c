#define WAH_IMPLEMENTATION
#include "wah.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Assertion failed: %s\n", message); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual, message) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "Assertion failed: %s (Expected: %lld, Actual: %lld)\n", message, (long long)(expected), (long long)(actual)); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_STREQ(expected, actual, message) do { \
    if (strcmp(expected, actual) != 0) { \
        fprintf(stderr, "Assertion failed: %s (Expected: \"%s\", Actual: \"%s\")\n", message, expected, actual); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_WAH_OK(err, message) do { \
    if ((err) != WAH_OK) { \
        fprintf(stderr, "WAH Error: %s (Code: %d, Message: %s)\n", message, (int)(err), wah_strerror(err)); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_WAH_ERROR(expected_err, actual_err, message) do { \
    if ((expected_err) != (actual_err)) { \
        fprintf(stderr, "WAH Error Mismatch: %s (Expected: %d (%s), Actual: %d (%s))\n", message, (int)(expected_err), wah_strerror(expected_err), (int)(actual_err), wah_strerror(actual_err)); \
        return 1; \
    } \
} while(0)

// Helper to build a minimal WASM binary for testing
// Magic (4 bytes) + Version (4 bytes)
#define WASM_HEADER 0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00

// --- Test Cases ---

int test_basic_exports() {
    printf("Running test_basic_exports...\n");
    // (func $add (param i32 i32) (result i32) (i32.add))
    // (global $g (mut i32) (i32.const 0))
    // (memory (export "mem") 1)
    // (table (export "tbl") 1 funcref)
    // (export "add" (func $add))
    // (export "g" (global $g))

    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, // section id, size
        0x01, // count
        0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,

        // Function Section (1 function, type index 0)
        0x03, 0x02, // section id, size
        0x01, // count
        0x00, // type index 0

        // Table Section (1 table, min 1 funcref)
        0x04, 0x04, // section id, size
        0x01, // count
        0x70, // elem_type (funcref)
        0x00, // flags (no max)
        0x01, // min elements

        // Memory Section (1 memory, min 1 page)
        0x05, 0x03, // section id, size
        0x01, // count
        0x00, // flags (no max)
        0x01, // min pages

        // Global Section (1 global: i32 mutable, init 0)
        0x06, 0x06, // section id, size
        0x01, // count
        0x7F, 0x01, 0x41, 0x00, 0x0B,

        // Export Section (4 exports: "add" func, "g" global, "mem" memory, "tbl" table)
        0x07, 0x17, // section id, size
        0x04, // count

        // Export 0: "add" func 0
        0x03, 'a', 'd', 'd', 0x00, 0x00,
        // Export 1: "g" global 0
        0x01, 'g', 0x03, 0x00,
        // Export 2: "mem" memory 0
        0x03, 'm', 'e', 'm', 0x02, 0x00,
        // Export 3: "tbl" table 0
        0x03, 't', 'b', 'l', 0x01, 0x00,

        // Code Section (1 code body)
        0x0A, 0x09, // section id, size
        0x01, // count
        0x07, // code body size (0x00 for locals count + 7 bytes for instructions)
        0x00, // locals count
        0x20, 0x00, // local.get 0
        0x20, 0x01, // local.get 1
        0x6A, // i32.add
        0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_OK(err, "Failed to parse basic exports module");

    TEST_ASSERT_EQ(wah_module_num_exports(&module), 4, "Incorrect export count");

    wah_entry_t entry;

    // Test export by index
    err = wah_module_export(&module, 0, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export 0");
    TEST_ASSERT(WAH_TYPE_IS_FUNCTION(entry.type), "WAH_TYPE_IS_FUNCTION failed for Export 0");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_KIND(entry.id), WAH_ENTRY_KIND_FUNCTION, "Export 0 ID kind mismatch");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_INDEX(entry.id), 0, "Export 0 ID index mismatch");
    TEST_ASSERT_STREQ(entry.name, "add", "Export 0 name mismatch");
    TEST_ASSERT_EQ(entry.name_len, 3, "Export 0 name_len mismatch");

    err = wah_module_export(&module, 1, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export 1");
    TEST_ASSERT_EQ(entry.type, WAH_VAL_TYPE_I32, "Export 1 type mismatch");
    TEST_ASSERT(WAH_TYPE_IS_GLOBAL(entry.type), "WAH_TYPE_IS_GLOBAL failed for Export 1");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_KIND(entry.id), WAH_ENTRY_KIND_GLOBAL, "Export 1 ID kind mismatch");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_INDEX(entry.id), 0, "Export 1 ID index mismatch");
    TEST_ASSERT_STREQ(entry.name, "g", "Export 1 name mismatch");
    TEST_ASSERT_EQ(entry.name_len, 1, "Export 1 name_len mismatch");

    err = wah_module_export(&module, 2, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export 2");
    TEST_ASSERT(WAH_TYPE_IS_MEMORY(entry.type), "WAH_TYPE_IS_MEMORY failed for Export 2");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_KIND(entry.id), WAH_ENTRY_KIND_MEMORY, "Export 2 ID kind mismatch");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_INDEX(entry.id), 0, "Export 2 ID index mismatch");
    TEST_ASSERT_STREQ(entry.name, "mem", "Export 2 name mismatch");
    TEST_ASSERT_EQ(entry.name_len, 3, "Export 2 name_len mismatch");

    err = wah_module_export(&module, 3, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export 3");
    TEST_ASSERT(WAH_TYPE_IS_TABLE(entry.type), "WAH_TYPE_IS_TABLE failed for Export 3");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_KIND(entry.id), WAH_ENTRY_KIND_TABLE, "Export 3 ID kind mismatch");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_INDEX(entry.id), 0, "Export 3 ID index mismatch");
    TEST_ASSERT_STREQ(entry.name, "tbl", "Export 3 name mismatch");
    TEST_ASSERT_EQ(entry.name_len, 3, "Export 3 name_len mismatch");

    // Test export by name
    err = wah_module_export_by_name(&module, "add", &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export by name 'add'");
    TEST_ASSERT(WAH_TYPE_IS_FUNCTION(entry.type), "WAH_TYPE_IS_FUNCTION failed for Export by name 'add'");
    TEST_ASSERT_STREQ(entry.name, "add", "Export by name 'add' name mismatch");

    err = wah_module_export_by_name(&module, "g", &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export by name 'g'");
    TEST_ASSERT_EQ(entry.type, WAH_VAL_TYPE_I32, "Export by name 'g' type mismatch");
    TEST_ASSERT(WAH_TYPE_IS_GLOBAL(entry.type), "WAH_TYPE_IS_GLOBAL failed for Export by name 'g'");
    TEST_ASSERT_STREQ(entry.name, "g", "Export by name 'g' name mismatch");

    err = wah_module_export_by_name(&module, "nonexistent", &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Found nonexistent export by name");

    // Test wah_module_entry for exported function
    wah_entry_id_t func_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_FUNCTION, 0);
    err = wah_module_entry(&module, func_id, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get entry for exported function");
    TEST_ASSERT(WAH_TYPE_IS_FUNCTION(entry.type), "WAH_TYPE_IS_FUNCTION failed for exported function");
    TEST_ASSERT_EQ(entry.id, func_id, "Entry ID mismatch for exported function");
    TEST_ASSERT(entry.name == NULL, "Entry name should be NULL for non-exported lookup"); // wah_module_entry doesn't return name for non-exported

    wah_free_module(&module);
    printf("test_basic_exports passed.\n");
    return 0;
}

int test_duplicate_export_names() {
    printf("Running test_duplicate_export_names...\n");
    // Export "add" twice

    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, // section id, size
        0x01, // count
        0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,

        // Function Section (1 function, type index 0)
        0x03, 0x02, // section id, size
        0x01, // count
        0x00, // type index 0

        // Export Section (2 exports: "add" func 0, "add" func 0)
        0x07, 0x0D, // section id, size
        0x02, // count

        // Export 0: "add" func 0
        0x03, 'a', 'd', 'd', 0x00, 0x00,
        // Export 1: "add" func 0 (duplicate)
        0x03, 'a', 'd', 'd', 0x00, 0x00,

        // Code Section (1 code body)
        0x0A, 0x09, // section id, size
        0x01, // count
        0x07, // code body size
        0x00, // locals count
        0x20, 0x00, // local.get 0
        0x20, 0x01, // local.get 1
        0x6A, // i32.add
        0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_VALIDATION_FAILED, err, "Parsing module with duplicate export names should fail");

    wah_free_module(&module); // Should be safe to call even if parsing failed
    printf("test_duplicate_export_names passed.\n");
    return 0;
}

int test_invalid_export_kind_or_index() {
    printf("Running test_invalid_export_kind_or_index...\n");

    // Test 1: Invalid export kind (e.g., 0x04 which is not defined)
    uint8_t wasm_binary_invalid_kind[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,
        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,

        // Export Section (1 export: "bad" kind 0x04, index 0)
        0x07, 0x07, // section id, size
        0x01, // count
        0x03, 'b', 'a', 'd', 0x04, 0x00, // Invalid kind 0x04

        // Code Section (1 code body)
        0x0A, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
    };

    wah_module_t module_invalid_kind;
    wah_error_t err = wah_parse_module(wasm_binary_invalid_kind, sizeof(wasm_binary_invalid_kind), &module_invalid_kind);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_VALIDATION_FAILED, err, "Parsing module with invalid export kind should fail");
    wah_free_module(&module_invalid_kind);

    // Test 2: Export function with out-of-bounds index
    uint8_t wasm_binary_invalid_func_idx[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,
        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,

        // Export Section (1 export: "bad_func" func 1 (out of bounds))
        0x07, 0x0C, // section id, size
        0x01, // count
        0x08, 'b', 'a', 'd', '_', 'f', 'u', 'n', 'c', 0x00, 0x01, // Index 1, but only 1 function (index 0)

        // Code Section (1 code body)
        0x0A, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
    };

    wah_module_t module_invalid_func_idx;
    err = wah_parse_module(wasm_binary_invalid_func_idx, sizeof(wasm_binary_invalid_func_idx), &module_invalid_func_idx);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_VALIDATION_FAILED, err, "Parsing module with out-of-bounds function export index should fail");
    wah_free_module(&module_invalid_func_idx);

    printf("test_invalid_export_kind_or_index passed.\n");
    return 0;
}

int test_non_utf8_export_name() {
    printf("Running test_non_utf8_export_name...\n");
    // Export name with invalid UTF-8 sequence (e.g., 0xFF)

    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,
        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,

        // Export Section (1 export: "bad\xFFname" func 0)
        0x07, 0x0B, // section id, size
        0x01, // count
        0x07, 'b', 'a', 'd', 0xFF, 'n', 'a', 'm', 'e', 0x00, 0x00,

        // Code Section (1 code body)
        0x0A, 0x06, 0x01, 0x04, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_VALIDATION_FAILED, err, "Parsing module with non-UTF8 export name should fail");

    wah_free_module(&module);
    printf("test_non_utf8_export_name passed.\n");
    return 0;
}

int test_module_no_exports() {
    printf("Running test_module_no_exports...\n");
    // Module with no export section

    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,
        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,
        // Code Section (1 code body)
        0x0A, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_OK(err, "Failed to parse module with no exports");

    TEST_ASSERT_EQ(wah_module_num_exports(&module), 0, "Export count should be 0 for module with no exports");

    wah_entry_t entry;
    err = wah_module_export(&module, 0, &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Getting export by index 0 should fail for module with no exports");

    err = wah_module_export_by_name(&module, "any", &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Getting export by name should fail for module with no exports");

    wah_free_module(&module);
    printf("test_module_no_exports passed.\n");
    return 0;
}

int test_wah_module_entry_non_exported() {
    printf("Running test_wah_module_entry_non_exported...\n");
    // Test wah_module_entry for a function that is not exported

    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,
        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,
        // Code Section (1 code body)
        0x0A, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_OK(err, "Failed to parse module for non-exported entry test");

    wah_entry_t entry;
    wah_entry_id_t func_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_FUNCTION, 0);
    err = wah_module_entry(&module, func_id, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get entry for non-exported function");
    TEST_ASSERT(WAH_TYPE_IS_FUNCTION(entry.type), "WAH_TYPE_IS_FUNCTION failed for non-exported function");
    TEST_ASSERT_EQ(entry.id, func_id, "Entry ID mismatch for non-exported function");
    TEST_ASSERT(entry.name == NULL, "Entry name should be NULL for non-exported entry");
    TEST_ASSERT_EQ(entry.name_len, 0, "Entry name_len should be 0 for non-exported entry");

    wah_free_module(&module);
    printf("test_wah_module_entry_non_exported passed.\n");
    return 0;
}

int test_wah_module_entry_invalid_id() {
    printf("Running test_wah_module_entry_invalid_id...\n");
    // Test wah_module_entry with invalid entry IDs

    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func(i32, i32) -> i32)
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F,
        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,
        // Code Section (1 code body)
        0x0A, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_OK(err, "Failed to parse module for invalid entry ID test");

    wah_entry_t entry;

    // Invalid function index
    wah_entry_id_t invalid_func_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_FUNCTION, 999);
    err = wah_module_entry(&module, invalid_func_id, &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Getting entry with invalid function index should fail");

    // Invalid kind
    wah_entry_id_t invalid_kind_id = WAH_MAKE_ENTRY_ID(0xFF, 0); // 0xFF is an unknown kind
    err = wah_module_entry(&module, invalid_kind_id, &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Getting entry with invalid kind should fail");

    wah_free_module(&module);
    printf("test_wah_module_entry_invalid_id passed.\n");
    return 0;
}

int test_export_name_with_null_byte() {
    printf("Running test_export_name_with_null_byte...\n");

    // Export name: "bad\x00name" (length 8)
    uint8_t wasm_binary[] = {
        WASM_HEADER,

        // Type Section (1 type: func() -> void)
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

        // Function Section (1 function, type index 0)
        0x03, 0x02, 0x01, 0x00,

        // Export Section (1 export: "bad\x00name" func 0)
        0x07, 0x0C, // section id, size
        0x01, // count
        0x08, 'b', 'a', 'd', 0x00, 'n', 'a', 'm', 'e', 0x00, 0x00,

        // Code Section (1 code body)
        0x0A, 0x04, 0x01, 0x02, 0x00, 0x0B,
    };

    wah_module_t module;
    wah_error_t err = wah_parse_module(wasm_binary, sizeof(wasm_binary), &module);
    TEST_ASSERT_WAH_OK(err, "Failed to parse module with null byte in export name");

    TEST_ASSERT_EQ(wah_module_num_exports(&module), 1, "Incorrect export count");

    wah_entry_t entry;

    // Verify by index
    err = wah_module_export(&module, 0, &entry);
    TEST_ASSERT_WAH_OK(err, "Failed to get export 0");
    TEST_ASSERT(WAH_TYPE_IS_FUNCTION(entry.type), "Export type mismatch");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_KIND(entry.id), WAH_ENTRY_KIND_FUNCTION, "Export ID kind mismatch");
    TEST_ASSERT_EQ(WAH_GET_ENTRY_INDEX(entry.id), 0, "Export ID index mismatch");
    TEST_ASSERT_EQ(entry.name_len, 8, "Export name_len mismatch");
    TEST_ASSERT(memcmp(entry.name, "bad\0name", 8) == 0, "Export name content mismatch");

    // Attempt lookup by "bad" (shorter, stops at null)
    err = wah_module_export_by_name(&module, "bad", &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Lookup by 'bad' should fail");

    // Attempt lookup by "bad\0name" (exact length, but strlen stops at null)
    char lookup_name_with_null[] = {'b', 'a', 'd', 0x00, 'n', 'a', 'm', 'e', '\0'}; // Ensure it's null-terminated for strlen
    err = wah_module_export_by_name(&module, lookup_name_with_null, &entry);
    TEST_ASSERT_WAH_ERROR(WAH_ERROR_NOT_FOUND, err, "Lookup by 'bad\0name' should fail");

    wah_free_module(&module);
    printf("test_export_name_with_null_byte passed.\n");
    return 0;
}

int main() {
    int result = 0;

    result |= test_basic_exports();
    result |= test_duplicate_export_names();
    result |= test_invalid_export_kind_or_index();
    result |= test_non_utf8_export_name();
    result |= test_module_no_exports();
    result |= test_wah_module_entry_non_exported();
    result |= test_wah_module_entry_invalid_id();
    result |= test_export_name_with_null_byte();

    if (result == 0) {
        printf("All export tests passed!\n");
    } else {
        printf("Some export tests failed.\n");
    }

    return result;
}
