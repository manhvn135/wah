// WebAssembly interpreter in a Header file (WAH)

#ifndef WAH_H
#define WAH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef WAH_DEBUG
#include <stdio.h>
#define WAH_LOG(fmt, ...) printf("(%d) " fmt "\n", __LINE__, ##__VA_ARGS__)
#else
#define WAH_LOG(fmt, ...) (void)(0)
#endif

// --- Error Codes ---
typedef enum {
    WAH_OK = 0,
    WAH_ERROR_INVALID_MAGIC_NUMBER,
    WAH_ERROR_INVALID_VERSION,
    WAH_ERROR_UNEXPECTED_EOF,
    WAH_ERROR_UNKNOWN_SECTION,
    WAH_ERROR_TOO_LARGE, // exceeding implementation limits (including numeric overflow)
    WAH_ERROR_OUT_OF_MEMORY,
    WAH_ERROR_VALIDATION_FAILED,
    WAH_ERROR_TRAP,  // Runtime trap (division by zero, integer overflow, etc.)
    WAH_ERROR_CALL_STACK_OVERFLOW,
    WAH_ERROR_MEMORY_OUT_OF_BOUNDS, // Memory access out of bounds
    // Add more specific error codes as needed
} wah_error_t;

// Convert error code to human-readable string
const char *wah_strerror(wah_error_t err);

// --- Memory Structure ---
#define WAH_WASM_PAGE_SIZE 65536 // 64 KB

typedef struct {
    uint32_t min_pages;
    uint32_t max_pages; // For future extension, currently ignored for fixed size
} wah_memory_type_t;

// --- LEB128 Decoding ---
// Helper function to decode an unsigned LEB128 integer
static inline wah_error_t wah_decode_uleb128(const uint8_t **ptr, const uint8_t *end, uint32_t *result) {
    uint64_t val = 0;
    uint32_t shift = 0;
    uint8_t byte;

    for (int i = 0; i < 5; ++i) { // Max 5 bytes for a 32-bit value
        if (*ptr >= end) return WAH_ERROR_UNEXPECTED_EOF;
        byte = *(*ptr)++;
        val |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            if (val > UINT32_MAX) return WAH_ERROR_TOO_LARGE;
            *result = (uint32_t)val;
            return WAH_OK;
        }
        shift += 7;
    }
    // If we get here, it means the 5th byte had the continuation bit set.
    return WAH_ERROR_TOO_LARGE;
}

// Helper function to decode a signed LEB128 integer (32-bit)
static inline wah_error_t wah_decode_sleb128_32(const uint8_t **ptr, const uint8_t *end, int32_t *result) {
    uint64_t val = 0;
    uint32_t shift = 0;
    uint8_t byte;

    for (int i = 0; i < 5; ++i) { // Max 5 bytes for a 32-bit value
        if (*ptr >= end) return WAH_ERROR_UNEXPECTED_EOF;
        byte = *(*ptr)++;
        val |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            // Sign extend
            if (shift < 64) {
                uint64_t sign_bit = 1ULL << (shift - 1);
                if ((val & sign_bit) != 0) {
                    val |= ~0ULL << shift;
                }
            }
            if ((int64_t)val < INT32_MIN || (int64_t)val > INT32_MAX) return WAH_ERROR_TOO_LARGE;
            *result = (int32_t)val;
            return WAH_OK;
        }
    }
    // If we get here, it means the 5th byte had the continuation bit set.
    return WAH_ERROR_TOO_LARGE;
}

// Helper function to decode an unsigned LEB128 integer (64-bit)
static inline wah_error_t wah_decode_uleb128_64(const uint8_t **ptr, const uint8_t *end, uint64_t *result) {
    *result = 0;
    uint64_t shift = 0;
    uint8_t byte;

    do {
        if (*ptr >= end) {
            return WAH_ERROR_UNEXPECTED_EOF;
        }
        if (shift >= 64) {
            return WAH_ERROR_TOO_LARGE;
        }
        byte = *(*ptr)++;
        *result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    return WAH_OK;
}

// Helper function to decode a signed LEB128 integer (64-bit)
static inline wah_error_t wah_decode_sleb128_64(const uint8_t **ptr, const uint8_t *end, int64_t *result) {
    uint64_t val = 0;
    uint32_t shift = 0;
    uint8_t byte;

    do {
        if (*ptr >= end) return WAH_ERROR_UNEXPECTED_EOF;
        if (shift >= 64) return WAH_ERROR_TOO_LARGE;
        byte = *(*ptr)++;
        val |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    if (shift < 64) {
        uint64_t sign_bit = 1ULL << (shift - 1);
        if ((val & sign_bit) != 0) {
            val |= ~0ULL << shift;
        }
    }
    *result = (int64_t)val;
    return WAH_OK;
}

// Helper to read a uint8_t from a byte array in little-endian format
static inline uint8_t wah_read_u8_le(const uint8_t *ptr) {
    return ptr[0];
}

// Helper to write a uint8_t to a byte array in little-endian format
static inline void wah_write_u8_le(uint8_t *ptr, uint8_t val) {
    ptr[0] = val;
}

// Helper to read a uint16_t from a byte array in little-endian format
static inline uint16_t wah_read_u16_le(const uint8_t *ptr) {
    return ((uint16_t)ptr[0] << 0) |
           ((uint16_t)ptr[1] << 8);
}

// Helper to write a uint16_t to a byte array in little-endian format
static inline void wah_write_u16_le(uint8_t *ptr, uint16_t val) {
    ptr[0] = (uint8_t)(val >> 0);
    ptr[1] = (uint8_t)(val >> 8);
}

// Helper to read a uint32_t from a byte array in little-endian format
static inline uint32_t wah_read_u32_le(const uint8_t *ptr) {
    return ((uint32_t)ptr[0] << 0) |
           ((uint32_t)ptr[1] << 8) |
           ((uint32_t)ptr[2] << 16) |
           ((uint32_t)ptr[3] << 24);
}

// Helper to read a uint64_t from a byte array in little-endian format
static inline uint64_t wah_read_u64_le(const uint8_t *ptr) {
    return ((uint64_t)ptr[0] << 0) |
           ((uint64_t)ptr[1] << 8) |
           ((uint64_t)ptr[2] << 16) |
           ((uint64_t)ptr[3] << 24) |
           ((uint64_t)ptr[4] << 32) |
           ((uint64_t)ptr[5] << 40) |
           ((uint64_t)ptr[6] << 48) |
           ((uint64_t)ptr[7] << 56);
}

// Helper to write a uint32_t to a byte array in little-endian format
static inline void wah_write_u32_le(uint8_t *ptr, uint32_t val) {
    ptr[0] = (uint8_t)(val >> 0);
    ptr[1] = (uint8_t)(val >> 8);
    ptr[2] = (uint8_t)(val >> 16);
    ptr[3] = (uint8_t)(val >> 24);
}

// Helper to write a uint64_t to a byte array in little-endian format
static inline void wah_write_u64_le(uint8_t *ptr, uint64_t val) {
    ptr[0] = (uint8_t)(val >> 0);
    ptr[1] = (uint8_t)(val >> 8);
    ptr[2] = (uint8_t)(val >> 16);
    ptr[3] = (uint8_t)(val >> 24);
    ptr[4] = (uint8_t)(val >> 32);
    ptr[5] = (uint8_t)(val >> 40);
    ptr[6] = (uint8_t)(val >> 48);
    ptr[7] = (uint8_t)(val >> 56);
}

// Helper to read a float from a byte array in little-endian format
static inline float wah_read_f32_le(const uint8_t *ptr) {
    union { uint32_t i; float f; } u = { .i = wah_read_u32_le(ptr) };
    return u.f;
}

// Helper to read a double from a byte array in little-endian format
static inline double wah_read_f64_le(const uint8_t *ptr) {
    union { uint64_t i; double d; } u = { .i = wah_read_u64_le(ptr) };
    return u.d;
}

// Helper to write a float to a byte array in little-endian format
static inline void wah_write_f32_le(uint8_t *ptr, float val) {
    union { uint32_t i; float f; } u = { .f = val };
    wah_write_u32_le(ptr, u.i);
}

// Helper to write a double to a byte array in little-endian format
static inline void wah_write_f64_le(uint8_t *ptr, double val) {
    union { uint64_t i; double f; } u = { .f = val };
    wah_write_u64_le(ptr, u.i);
}


// --- WebAssembly Types ---
typedef enum {
    WAH_VAL_TYPE_I32 = 0x7F,
    WAH_VAL_TYPE_I64 = 0x7E,
    WAH_VAL_TYPE_F32 = 0x7D,
    WAH_VAL_TYPE_F64 = 0x7C,
    WAH_VAL_TYPE_FUNCREF = 0x70, // For table elements (function references)
    WAH_VAL_TYPE_FUNC = 0x60,    // For function types
    WAH_VAL_TYPE_BLOCK_TYPE = 0x40, // Special type for blocks
    WAH_VAL_TYPE_ANY = -5 // Represents any type for stack-polymorphic validation
} wah_val_type_t;

// --- WebAssembly Opcodes (subset) ---
typedef enum {
    // Control Flow Operators
    WAH_OP_UNREACHABLE = 0x00, WAH_OP_NOP = 0x01, WAH_OP_BLOCK = 0x02, WAH_OP_LOOP = 0x03,
    WAH_OP_IF = 0x04, WAH_OP_ELSE = 0x05,
    WAH_OP_END = 0x0B, WAH_OP_BR = 0x0C, WAH_OP_BR_IF = 0x0D, WAH_OP_BR_TABLE = 0x0E,
    WAH_OP_RETURN = 0x0F, WAH_OP_CALL = 0x10, WAH_OP_CALL_INDIRECT = 0x11,

    // Parametric Operators
    WAH_OP_DROP = 0x1A,
    WAH_OP_SELECT = 0x1B,

    // Variable Access
    WAH_OP_LOCAL_GET = 0x20, WAH_OP_LOCAL_SET = 0x21, WAH_OP_LOCAL_TEE = 0x22,
    WAH_OP_GLOBAL_GET = 0x23, WAH_OP_GLOBAL_SET = 0x24,

    // Memory Operators
    WAH_OP_I32_LOAD = 0x28, WAH_OP_I64_LOAD = 0x29, WAH_OP_F32_LOAD = 0x2A, WAH_OP_F64_LOAD = 0x2B,
    WAH_OP_I32_LOAD8_S = 0x2C, WAH_OP_I32_LOAD8_U = 0x2D, WAH_OP_I32_LOAD16_S = 0x2E, WAH_OP_I32_LOAD16_U = 0x2F,
    WAH_OP_I64_LOAD8_S = 0x30, WAH_OP_I64_LOAD8_U = 0x31, WAH_OP_I64_LOAD16_S = 0x32, WAH_OP_I64_LOAD16_U = 0x33,
    WAH_OP_I64_LOAD32_S = 0x34, WAH_OP_I64_LOAD32_U = 0x35,
    WAH_OP_I32_STORE = 0x36, WAH_OP_I64_STORE = 0x37, WAH_OP_F32_STORE = 0x38, WAH_OP_F64_STORE = 0x39,
    WAH_OP_I32_STORE8 = 0x3A, WAH_OP_I32_STORE16 = 0x3B,
    WAH_OP_I64_STORE8 = 0x3C, WAH_OP_I64_STORE16 = 0x3D, WAH_OP_I64_STORE32 = 0x3E,
    WAH_OP_MEMORY_SIZE = 0x3F,
    WAH_OP_MEMORY_GROW = 0x40,
    WAH_OP_MEMORY_FILL = 0xC00B,

    // Constants
    WAH_OP_I32_CONST = 0x41, WAH_OP_I64_CONST = 0x42, WAH_OP_F32_CONST = 0x43, WAH_OP_F64_CONST = 0x44,

    // Comparison Operators
    WAH_OP_I32_EQZ = 0x45, WAH_OP_I32_EQ = 0x46, WAH_OP_I32_NE = 0x47,
    WAH_OP_I32_LT_S = 0x48, WAH_OP_I32_LT_U = 0x49, WAH_OP_I32_GT_S = 0x4A, WAH_OP_I32_GT_U = 0x4B,
    WAH_OP_I32_LE_S = 0x4C, WAH_OP_I32_LE_U = 0x4D, WAH_OP_I32_GE_S = 0x4E, WAH_OP_I32_GE_U = 0x4F,
    WAH_OP_I64_EQZ = 0x50, WAH_OP_I64_EQ = 0x51, WAH_OP_I64_NE = 0x52,
    WAH_OP_I64_LT_S = 0x53, WAH_OP_I64_LT_U = 0x54, WAH_OP_I64_GT_S = 0x55, WAH_OP_I64_GT_U = 0x56,
    WAH_OP_I64_LE_S = 0x57, WAH_OP_I64_LE_U = 0x58, WAH_OP_I64_GE_S = 0x59, WAH_OP_I64_GE_U = 0x5A,
    WAH_OP_F32_EQ = 0x5B, WAH_OP_F32_NE = 0x5C,
    WAH_OP_F32_LT = 0x5D, WAH_OP_F32_GT = 0x5E, WAH_OP_F32_LE = 0x5F, WAH_OP_F32_GE = 0x60,
    WAH_OP_F64_EQ = 0x61, WAH_OP_F64_NE = 0x62,
    WAH_OP_F64_LT = 0x63, WAH_OP_F64_GT = 0x64, WAH_OP_F64_LE = 0x65, WAH_OP_F64_GE = 0x66,

    // Numeric Operators
    WAH_OP_I32_CLZ = 0x67, WAH_OP_I32_CTZ = 0x68, WAH_OP_I32_POPCNT = 0x69,
    WAH_OP_I32_ADD = 0x6A, WAH_OP_I32_SUB = 0x6B, WAH_OP_I32_MUL = 0x6C,
    WAH_OP_I32_DIV_S = 0x6D, WAH_OP_I32_DIV_U = 0x6E, WAH_OP_I32_REM_S = 0x6F, WAH_OP_I32_REM_U = 0x70,
    WAH_OP_I32_AND = 0x71, WAH_OP_I32_OR = 0x72, WAH_OP_I32_XOR = 0x73,
    WAH_OP_I32_SHL = 0x74, WAH_OP_I32_SHR_S = 0x75, WAH_OP_I32_SHR_U = 0x76,
    WAH_OP_I32_ROTL = 0x77, WAH_OP_I32_ROTR = 0x78,
    WAH_OP_I64_CLZ = 0x79, WAH_OP_I64_CTZ = 0x7A, WAH_OP_I64_POPCNT = 0x7B,
    WAH_OP_I64_ADD = 0x7C, WAH_OP_I64_SUB = 0x7D, WAH_OP_I64_MUL = 0x7E,
    WAH_OP_I64_DIV_S = 0x7F, WAH_OP_I64_DIV_U = 0x80, WAH_OP_I64_REM_S = 0x81, WAH_OP_I64_REM_U = 0x82,
    WAH_OP_I64_AND = 0x83, WAH_OP_I64_OR = 0x84, WAH_OP_I64_XOR = 0x85,
    WAH_OP_I64_SHL = 0x86, WAH_OP_I64_SHR_S = 0x87, WAH_OP_I64_SHR_U = 0x88,
    WAH_OP_I64_ROTL = 0x89, WAH_OP_I64_ROTR = 0x8A,
    WAH_OP_F32_ABS = 0x8B, WAH_OP_F32_NEG = 0x8C, WAH_OP_F32_CEIL = 0x8D, WAH_OP_F32_FLOOR = 0x8E,
    WAH_OP_F32_TRUNC = 0x8F, WAH_OP_F32_NEAREST = 0x90, WAH_OP_F32_SQRT = 0x91,
    WAH_OP_F32_ADD = 0x92, WAH_OP_F32_SUB = 0x93, WAH_OP_F32_MUL = 0x94, WAH_OP_F32_DIV = 0x95,
    WAH_OP_F32_MIN = 0x96, WAH_OP_F32_MAX = 0x97, WAH_OP_F32_COPYSIGN = 0x98,
    WAH_OP_F64_ABS = 0x99, WAH_OP_F64_NEG = 0x9A, WAH_OP_F64_CEIL = 0x9B, WAH_OP_F64_FLOOR = 0x9C,
    WAH_OP_F64_TRUNC = 0x9D, WAH_OP_F64_NEAREST = 0x9E, WAH_OP_F64_SQRT = 0x9F,
    WAH_OP_F64_ADD = 0xA0, WAH_OP_F64_SUB = 0xA1, WAH_OP_F64_MUL = 0xA2, WAH_OP_F64_DIV = 0xA3,
    WAH_OP_F64_MIN = 0xA4, WAH_OP_F64_MAX = 0xA5, WAH_OP_F64_COPYSIGN = 0xA6,

    // Conversion Operators
    WAH_OP_I32_WRAP_I64 = 0xA7,
    WAH_OP_I32_TRUNC_F32_S = 0xA8, WAH_OP_I32_TRUNC_F32_U = 0xA9,
    WAH_OP_I32_TRUNC_F64_S = 0xAA, WAH_OP_I32_TRUNC_F64_U = 0xAB,
    WAH_OP_I64_EXTEND_I32_S = 0xAC, WAH_OP_I64_EXTEND_I32_U = 0xAD,
    WAH_OP_I64_TRUNC_F32_S = 0xAE, WAH_OP_I64_TRUNC_F32_U = 0xAF,
    WAH_OP_I64_TRUNC_F64_S = 0xB0, WAH_OP_I64_TRUNC_F64_U = 0xB1,
    WAH_OP_F32_CONVERT_I32_S = 0xB2, WAH_OP_F32_CONVERT_I32_U = 0xB3,
    WAH_OP_F32_CONVERT_I64_S = 0xB4, WAH_OP_F32_CONVERT_I64_U = 0xB5,
    WAH_OP_F32_DEMOTE_F64 = 0xB6,
    WAH_OP_F64_CONVERT_I32_S = 0xB7, WAH_OP_F64_CONVERT_I32_U = 0xB8,
    WAH_OP_F64_CONVERT_I64_S = 0xB9, WAH_OP_F64_CONVERT_I64_U = 0xBA,
    WAH_OP_F64_PROMOTE_F32 = 0xBB,
    WAH_OP_I32_REINTERPRET_F32 = 0xBC, WAH_OP_I64_REINTERPRET_F64 = 0xBD,
    WAH_OP_F32_REINTERPRET_I32 = 0xBE, WAH_OP_F64_REINTERPRET_I64 = 0xBF,
} wah_opcode_t;

typedef union {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
} wah_value_t;

// --- WebAssembly Table Structures ---
typedef struct {
    wah_val_type_t elem_type;
    uint32_t min_elements;
    uint32_t max_elements; // 0 if no maximum
} wah_table_type_t;

// --- WebAssembly Element Segment Structure ---
typedef struct {
    uint32_t table_idx;
    uint32_t offset; // Result of the offset_expr
    uint32_t num_elems;
    uint32_t *func_indices; // Array of function indices
} wah_element_segment_t;

// --- Operand Stack ---
typedef struct {
    wah_value_t *data; // Dynamically allocated based on function requirements
    uint32_t sp; // Stack pointer
    uint32_t capacity; // Allocated capacity
} wah_stack_t;

// --- Type Stack for Validation ---

#define WAH_MAX_TYPE_STACK_SIZE 1024 // Maximum size of the type stack for validation
typedef struct {
    wah_val_type_t data[WAH_MAX_TYPE_STACK_SIZE];
    uint32_t sp; // Stack pointer
} wah_type_stack_t;

// --- Execution Context (New Design) ---

// Forward declarations for structures used in the execution context.
struct wah_code_body_s;
struct wah_module_s;

// Represents a single function call's state on the call stack.
typedef struct {
    const uint8_t *bytecode_ip;  // Instruction pointer into the parsed bytecode
    const struct wah_code_body_s *code; // The function body being executed
    uint32_t locals_offset;      // Offset into the shared value_stack for this frame's locals
    uint32_t func_idx;           // Index of the function being executed
} wah_call_frame_t;

// The main context for the entire WebAssembly interpretation.
#define WAH_DEFAULT_MAX_CALL_DEPTH 1024
#define WAH_DEFAULT_VALUE_STACK_SIZE (64 * 1024)

typedef struct wah_exec_context_s {
    wah_value_t *value_stack;       // A single, large stack for operands and locals
    uint32_t sp;                    // Stack pointer for the value_stack (points to next free slot)
    uint32_t value_stack_capacity;

    wah_call_frame_t *call_stack;   // The call frame stack
    uint32_t call_depth;            // Current call depth (top of the call_stack)
    uint32_t call_stack_capacity;

    uint32_t max_call_depth;        // Configurable max call depth

    wah_value_t *globals;           // Mutable global values
    uint32_t global_count;

    const struct wah_module_s *module;

    // Memory
    uint8_t *memory_base; // Pointer to the allocated memory
    uint32_t memory_size; // Current size of the memory in bytes

    wah_value_t **tables;
    uint32_t table_count;
} wah_exec_context_t;

// --- Section IDs ---
typedef enum {
    WAH_SECTION_CUSTOM = 0,
    WAH_SECTION_TYPE = 1,
    WAH_SECTION_IMPORT = 2,
    WAH_SECTION_FUNCTION = 3,
    WAH_SECTION_TABLE = 4,
    WAH_SECTION_MEMORY = 5,
    WAH_SECTION_GLOBAL = 6,
    WAH_SECTION_EXPORT = 7,
    WAH_SECTION_START = 8,
    WAH_SECTION_ELEMENT = 9,
    WAH_SECTION_CODE = 10,
    WAH_SECTION_DATA = 11,
    WAH_SECTION_DATACOUNT = 12,
} wah_section_id_t;

// --- Function Type ---
typedef struct {
    uint32_t param_count;
    wah_val_type_t *param_types;
    uint32_t result_count;
    wah_val_type_t *result_types;
} wah_func_type_t;

// --- Pre-parsed Opcode Structure for Optimized Execution ---
typedef struct {
    uint8_t *bytecode;           // Combined array of opcodes and arguments
    uint32_t bytecode_size;      // Total size of the bytecode array
} wah_parsed_code_t;

// --- Code Body Structure ---
typedef struct wah_code_body_s {
    uint32_t local_count;
    wah_val_type_t *local_types; // Array of types for local variables
    uint32_t code_size;
    const uint8_t *code; // Pointer to the raw instruction bytes within the WASM binary
    uint32_t max_stack_depth; // Maximum operand stack depth required
    wah_parsed_code_t parsed_code; // Pre-parsed opcodes and arguments for optimized execution
} wah_code_body_t;

// --- Global Variable Structure ---
typedef struct {
    wah_val_type_t type;
    bool is_mutable;
    wah_value_t initial_value; // Stored after parsing the init_expr
} wah_global_t;

// --- Module Structure ---
typedef struct wah_module_s {
    uint32_t type_count;
    uint32_t function_count;
    uint32_t code_count;
    uint32_t global_count;
    uint32_t memory_count;
    uint32_t table_count;
    uint32_t element_segment_count;

    wah_func_type_t *types;
    uint32_t *function_type_indices; // Index into the types array
    wah_code_body_t *code_bodies;
    wah_global_t *globals;
    wah_memory_type_t *memories;
    wah_table_type_t *tables;
    wah_element_segment_t *element_segments;

    // Add other sections as they are implemented
} wah_module_t;

// --- Validation Context ---
#define WAH_MAX_CONTROL_DEPTH 256
typedef struct {
    wah_opcode_t opcode;
    uint32_t type_stack_sp; // Type stack pointer at the start of the block
    wah_func_type_t block_type; // For if/block/loop
    bool else_found; // For if blocks
    bool is_unreachable; // True if this control frame is currently unreachable
    uint32_t stack_height; // Stack height at the beginning of the block
} wah_validation_control_frame_t;

typedef struct {
    wah_type_stack_t type_stack;
    const wah_func_type_t *func_type; // Type of the function being validated
    const wah_module_t *module; // Reference to the module for global/function lookups
    uint32_t total_locals; // Total number of locals (params + declared locals)
    uint32_t current_stack_depth; // Current stack depth during validation
    uint32_t max_stack_depth; // Maximum stack depth seen during validation
    bool is_unreachable; // True if the current code path is unreachable
    
    // Control flow validation stack
    wah_validation_control_frame_t control_stack[WAH_MAX_CONTROL_DEPTH];
    uint32_t control_sp;
} wah_validation_context_t;

// --- Parser Functions ---
static wah_error_t wah_decode_opcode(const uint8_t **ptr, const uint8_t *end, uint16_t *opcode_val);

wah_error_t wah_parse_module(const uint8_t *wasm_binary, size_t binary_size, wah_module_t *module);

// Internal section parsing functions
static wah_error_t wah_parse_type_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_import_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_function_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_table_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_memory_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_global_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_export_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_start_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_element_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_code_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_data_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_datacount_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_custom_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);

// Global array of section handlers, indexed by wah_section_id_t
static const struct wah_section_handler_s {
    int8_t order; // Expected order of the section (0 for custom, 1 for Type, etc.)
    wah_error_t (*parser_func)(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
} wah_section_handlers[] = {
    [WAH_SECTION_CUSTOM]    = { .order = 0,  .parser_func = wah_parse_custom_section },
    [WAH_SECTION_TYPE]      = { .order = 1,  .parser_func = wah_parse_type_section },
    [WAH_SECTION_IMPORT]    = { .order = 2,  .parser_func = wah_parse_import_section },
    [WAH_SECTION_FUNCTION]  = { .order = 3,  .parser_func = wah_parse_function_section },
    [WAH_SECTION_TABLE]     = { .order = 4,  .parser_func = wah_parse_table_section },
    [WAH_SECTION_MEMORY]    = { .order = 5,  .parser_func = wah_parse_memory_section },
    [WAH_SECTION_GLOBAL]    = { .order = 6,  .parser_func = wah_parse_global_section },
    [WAH_SECTION_EXPORT]    = { .order = 7,  .parser_func = wah_parse_export_section },
    [WAH_SECTION_START]     = { .order = 8,  .parser_func = wah_parse_start_section },
    [WAH_SECTION_ELEMENT]   = { .order = 9,  .parser_func = wah_parse_element_section },
    [WAH_SECTION_DATACOUNT] = { .order = 10, .parser_func = wah_parse_datacount_section },
    [WAH_SECTION_CODE]      = { .order = 11, .parser_func = wah_parse_code_section },
    [WAH_SECTION_DATA]      = { .order = 12, .parser_func = wah_parse_data_section },
};

// Creates and initializes an execution context.
wah_error_t wah_exec_context_create(wah_exec_context_t *exec_ctx, const wah_module_t *module);

// Destroys and frees resources of an execution context.
void wah_exec_context_destroy(wah_exec_context_t *exec_ctx);

// The main entry point to call a WebAssembly function.
wah_error_t wah_call(wah_exec_context_t *exec_ctx, const wah_module_t *module, uint32_t func_idx, const wah_value_t *params, uint32_t param_count, wah_value_t *result);

// The core interpreter loop (internal).
static wah_error_t wah_run_interpreter(wah_exec_context_t *exec_ctx);

// Validation helper functions
static wah_error_t wah_validate_opcode(uint16_t opcode_val, const uint8_t **code_ptr, const uint8_t *code_end, wah_validation_context_t *vctx, const wah_code_body_t* code_body);

// Pre-parsing functions
static wah_error_t wah_preparse_code(const wah_module_t* module, uint32_t func_idx, const uint8_t *code, uint32_t code_size, wah_parsed_code_t *parsed_code);
static void wah_free_parsed_code(wah_parsed_code_t *parsed_code);

// --- Module Cleanup ---
void wah_free_module(wah_module_t *module);

#ifdef WAH_IMPLEMENTATION

#include <string.h> // For memcpy, memset
#include <stdlib.h> // For malloc, free
#include <assert.h> // For assert
#include <stdint.h> // For INT32_MIN, INT32_MAX
#include <math.h> // For floating-point functions
#if defined(_MSC_VER)
#include <intrin.h> // For MSVC intrinsics
#endif

// WebAssembly canonical NaN bit patterns
#define WASM_F32_CANONICAL_NAN_BITS 0x7fc00000U
#define WASM_F64_CANONICAL_NAN_BITS 0x7ff8000000000000ULL

// Function to canonicalize a float NaN
static inline float wah_canonicalize_f32(float val) {
    // Check for NaN using val != val (IEEE 754 property)
    if (val != val) {
        uint32_t bits;
        memcpy(&bits, &val, sizeof(uint32_t));
        uint32_t sign_bit = bits & 0x80000000U; // Extract sign bit
        bits = sign_bit | WASM_F32_CANONICAL_NAN_BITS; // Combine sign with canonical NaN
        memcpy(&val, &bits, sizeof(uint32_t));
    }
    return val;
}

// Function to canonicalize a double NaN
static inline double wah_canonicalize_f64(double val) {
    // Check for NaN using val != val (IEEE 754 property)
    if (val != val) {
        uint64_t bits;
        memcpy(&bits, &val, sizeof(uint64_t));
        uint64_t sign_bit = bits & 0x8000000000000000ULL; // Extract sign bit
        bits = sign_bit | WASM_F64_CANONICAL_NAN_BITS; // Combine sign with canonical NaN
        memcpy(&val, &bits, sizeof(uint64_t));
    }
    return val;
}

// --- Integer Utility Functions ---

// popcnt
static inline uint32_t wah_popcount_u32(uint32_t n) {
#ifdef __GNUC__
    return __builtin_popcount(n);
#elif defined(_MSC_VER)
    return __popcnt(n);
#else
    // Generic software implementation
    uint32_t count = 0;
    while (n > 0) {
        n &= (n - 1);
        count++;
    }
    return count;
#endif
}

static inline uint64_t wah_popcount_u64(uint64_t n) {
#ifdef __GNUC__
    return __builtin_popcountll(n);
#elif defined(_MSC_VER)
    return __popcnt64(n);
#else
    // Generic software implementation
    uint64_t count = 0;
    while (n > 0) {
        n &= (n - 1);
        count++;
    }
    return count;
#endif
}

// clz (count leading zeros)
static inline uint32_t wah_clz_u32(uint32_t n) {
#ifdef __GNUC__
    return n == 0 ? 32 : __builtin_clz(n);
#elif defined(_MSC_VER)
    unsigned long index;
    if (_BitScanReverse(&index, n)) {
        return 31 - index;
    } else {
        return 32; // All bits are zero
    }
#else
    // Generic software implementation
    if (n == 0) return 32;
    uint32_t count = 0;
    if (n <= 0x0000FFFF) { count += 16; n <<= 16; }
    if (n <= 0x00FFFFFF) { count += 8; n <<= 8; }
    if (n <= 0x0FFFFFFF) { count += 4; n <<= 4; }
    if (n <= 0x3FFFFFFF) { count += 2; n <<= 2; }
    if (n <= 0x7FFFFFFF) { count += 1; }
    return count;
#endif
}

static inline uint64_t wah_clz_u64(uint64_t n) {
#ifdef __GNUC__
    return n == 0 ? 64 : __builtin_clzll(n);
#elif defined(_MSC_VER)
    unsigned long index;
    if (_BitScanReverse64(&index, n)) {
        return 63 - index;
    } else {
        return 64; // All bits are zero
    }
#else
    // Generic software implementation
    if (n == 0) return 64;
    uint64_t count = 0;
    if (n <= 0x00000000FFFFFFFFULL) { count += 32; n <<= 32; }
    if (n <= 0x0000FFFFFFFFFFFFULL) { count += 16; n <<= 16; }
    if (n <= 0x00FFFFFFFFFFFFFFULL) { count += 8; n <<= 8; }
    if (n <= 0x0FFFFFFFFFFFFFFFULL) { count += 4; n <<= 4; }
    if (n <= 0x3FFFFFFFFFFFFFFFULL) { count += 2; n <<= 2; }
    if (n <= 0x7FFFFFFFFFFFFFFFULL) { count += 1; }
    return count;
#endif
}

// ctz (count trailing zeros)
static inline uint32_t wah_ctz_u32(uint32_t n) {
#ifdef __GNUC__
    return n == 0 ? 32 : __builtin_ctz(n);
#elif defined(_MSC_VER)
    unsigned long index;
    if (_BitScanForward(&index, n)) {
        return index;
    } else {
        return 32; // All bits are zero
    }
#else
    // Generic software implementation
    if (n == 0) return 32;
    uint32_t count = 0;
    while ((n & 1) == 0) {
        n >>= 1;
        count++;
    }
    return count;
#endif
}

static inline uint64_t wah_ctz_u64(uint64_t n) {
#ifdef __GNUC__
    return n == 0 ? 64 : __builtin_ctzll(n);
#elif defined(_MSC_VER)
    unsigned long index;
    if (_BitScanForward64(&index, n)) {
        return index;
    } else {
        return 64; // All bits are zero
    }
#else
    // Generic software implementation
    if (n == 0) return 64;
    uint64_t count = 0;
    while ((n & 1) == 0) {
        n >>= 1;
        count++;
    }
    return count;
#endif
}

// rotl (rotate left)
static inline uint32_t wah_rotl_u32(uint32_t n, uint32_t shift) {
#ifdef __clang__
    return __builtin_rotateleft32(n, shift);
#elif defined(_MSC_VER)
    return _rotl(n, shift);
#else
    shift &= 31; // Ensure shift is within 0-31
    return (n << shift) | (n >> (32 - shift));
#endif
}

static inline uint64_t wah_rotl_u64(uint64_t n, uint64_t shift) {
#ifdef __clang__
    return __builtin_rotateleft64(n, shift);
#elif defined(_MSC_VER)
    return _rotl64(n, shift);
#else
    shift &= 63; // Ensure shift is within 0-63
    return (n << shift) | (n >> (64 - shift));
#endif
}

// rotr (rotate right)
static inline uint32_t wah_rotr_u32(uint32_t n, uint32_t shift) {
#ifdef __clang__
    return __builtin_rotateright32(n, shift);
#elif defined(_MSC_VER)
    return _rotr(n, shift);
#else
    shift &= 31; // Ensure shift is within 0-31
    return (n >> shift) | (n << (32 - shift));
#endif
}

static inline uint64_t wah_rotr_u64(uint64_t n, uint64_t shift) {
#ifdef __clang__
    return __builtin_rotateright64(n, shift);
#elif defined(_MSC_VER)
    return _rotr64(n, shift);
#else
    shift &= 63; // Ensure shift is within 0-63
    return (n >> shift) | (n << (64 - shift));
#endif
}

// nearest (round to nearest, ties to even)
static inline float wah_nearest_f32(float f) {
#ifdef __clang__
    return __builtin_roundevenf(f);
#else
    if (isnan(f) || isinf(f) || f == 0.0f) {
        return f;
    }
    float rounded = roundf(f);
    if (fabsf(f - rounded) == 0.5f) {
        if (((long long)rounded % 2) != 0) {
            return rounded - copysignf(1.0f, f);
        }
    }
    return rounded;
#endif
}

static inline double wah_nearest_f64(double d) {
#ifdef __clang__
    return __builtin_roundeven(d);
#else
    if (isnan(d) || isinf(d) || d == 0.0) {
        return d;
    }
    double rounded = round(d);
    if (fabs(d - rounded) == 0.5) {
        if (((long long)rounded % 2) != 0) {
            return rounded - copysign(1.0, d);
        }
    }
    return rounded;
#endif
}

// Helper functions for floating-point to integer truncations with trap handling
static inline wah_error_t wah_trunc_f32_to_i32(float val, int32_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < (float)INT32_MIN || val >= (float)INT32_MAX + 1.0f) return WAH_ERROR_TRAP;
    *result = (int32_t)truncf(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f32_to_u32(float val, uint32_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < 0.0f || val >= (float)UINT32_MAX + 1.0f) return WAH_ERROR_TRAP;
    *result = (uint32_t)truncf(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f64_to_i32(double val, int32_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < (double)INT32_MIN || val >= (double)INT32_MAX + 1.0) return WAH_ERROR_TRAP;
    *result = (int32_t)trunc(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f64_to_u32(double val, uint32_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < 0.0 || val >= (double)UINT32_MAX + 1.0) return WAH_ERROR_TRAP;
    *result = (uint32_t)trunc(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f32_to_i64(float val, int64_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < (float)INT64_MIN || val >= (float)INT64_MAX + 1.0f) return WAH_ERROR_TRAP;
    *result = (int64_t)truncf(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f32_to_u64(float val, uint64_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < 0.0f || val >= (float)UINT64_MAX + 1.0f) return WAH_ERROR_TRAP;
    *result = (uint64_t)truncf(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f64_to_i64(double val, int64_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < (double)INT64_MIN || val >= (double)INT64_MAX + 1.0) return WAH_ERROR_TRAP;
    *result = (int64_t)trunc(val);
    return WAH_OK;
}

static inline wah_error_t wah_trunc_f64_to_u64(double val, uint64_t *result) {
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP;
    if (val < 0.0 || val >= (double)UINT64_MAX + 1.0) return WAH_ERROR_TRAP;
    *result = (uint64_t)trunc(val);
    return WAH_OK;
}

// --- Helper Macros ---
#define WAH_CHECK(expr) do { \
    wah_error_t _err = (expr); \
    if (_err != WAH_OK) { WAH_LOG("WAH_CHECK(%s) failed due to: %s", #expr, wah_strerror(_err)); return _err; } \
} while(0)

#define WAH_CHECK_GOTO(expr, label) do { \
    err = (expr); \
    if (err != WAH_OK) { WAH_LOG("WAH_CHECK_GOTO(%s, %s) failed due to: %s", #expr, #label, wah_strerror(err)); goto label; } \
} while(0)

#define WAH_BOUNDS_CHECK(cond, error) do { \
    if (!(cond)) { WAH_LOG("WAH_BOUNDS_CHECK(%s, %s) failed", #cond, #error); return (error); } \
} while(0)

#define WAH_BOUNDS_CHECK_GOTO(cond, error, label) do { \
    if (!(cond)) { err = (error); WAH_LOG("WAH_BOUNDS_CHECK_GOTO(%s, %s, %s) failed", #cond, #error, #label); goto label; } \
} while(0)

// --- Safe Memory Allocation ---
static inline wah_error_t wah_malloc(size_t count, size_t elemsize, void** out_ptr) {
    *out_ptr = NULL;
    if (count == 0) {
        return WAH_OK;
    }
    if (elemsize > 0 && count > SIZE_MAX / elemsize) {
        return WAH_ERROR_OUT_OF_MEMORY;
    }
    size_t total_size = count * elemsize;
    *out_ptr = malloc(total_size);
    if (!*out_ptr) {
        return WAH_ERROR_OUT_OF_MEMORY;
    }
    return WAH_OK;
}

static inline wah_error_t wah_realloc(size_t count, size_t elemsize, void** p_ptr) {
    if (count == 0) {
        free(*p_ptr);
        *p_ptr = NULL;
        return WAH_OK;
    }
    if (elemsize > 0 && count > SIZE_MAX / elemsize) {
        return WAH_ERROR_OUT_OF_MEMORY;
    }
    size_t total_size = count * elemsize;
    void* new_ptr = realloc(*p_ptr, total_size);
    if (!new_ptr) {
        return WAH_ERROR_OUT_OF_MEMORY;
    }
    *p_ptr = new_ptr;
    return WAH_OK;
}

#define WAH_MALLOC_ARRAY(ptr, count) \
    do { \
        void *_alloc_ptr; \
        wah_error_t _alloc_err = wah_malloc((count), sizeof(*(ptr)), &_alloc_ptr); \
        if (_alloc_err != WAH_OK) { \
            WAH_LOG("WAH_MALLOC_ARRAY(%s, %s) failed due to OOM", #ptr, #count); \
            return _alloc_err; \
        } \
        (ptr) = _alloc_ptr; \
    } while (0)

#define WAH_REALLOC_ARRAY(ptr, count) \
    do { \
        void *_alloc_ptr = (ptr); \
        wah_error_t _alloc_err = wah_realloc((count), sizeof(*(ptr)), &_alloc_ptr); \
        if (_alloc_err != WAH_OK) { \
            WAH_LOG("WAH_REALLOC_ARRAY(%s, %s) failed due to OOM", #ptr, #count); \
            return _alloc_err; \
        } \
        (ptr) = _alloc_ptr; \
    } while (0)

#define WAH_MALLOC_ARRAY_GOTO(ptr, count, label) \
    do { \
        void* _alloc_ptr; \
        err = wah_malloc((count), sizeof(*(ptr)), &_alloc_ptr); \
        if (err != WAH_OK) { \
            WAH_LOG("WAH_MALLOC_ARRAY_GOTO(%s, %s, %s) failed due to OOM", #ptr, #count, #label); \
            goto label; \
        } \
        (ptr) = _alloc_ptr; \
    } while (0)

#define WAH_REALLOC_ARRAY_GOTO(ptr, count, label) \
    do { \
        void* _alloc_ptr = ptr; \
        err = wah_realloc((count), sizeof(*(ptr)), &_alloc_ptr); \
        if (err != WAH_OK) { \
            WAH_LOG("WAH_REALLOC_ARRAY_GOTO(%s, %s, %s) failed due to OOM", #ptr, #count, #label); \
            goto label; \
        } \
        (ptr) = _alloc_ptr; \
    } while (0)

const char *wah_strerror(wah_error_t err) {
    switch (err) {
        case WAH_OK: return "Success";
        case WAH_ERROR_INVALID_MAGIC_NUMBER: return "Invalid WASM magic number";
        case WAH_ERROR_INVALID_VERSION: return "Invalid WASM version";
        case WAH_ERROR_UNEXPECTED_EOF: return "Unexpected end of file";
        case WAH_ERROR_UNKNOWN_SECTION: return "Unknown section or opcode";
        case WAH_ERROR_TOO_LARGE: return "exceeding implementation limits (or value too large)";
        case WAH_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case WAH_ERROR_VALIDATION_FAILED: return "Validation failed";
        case WAH_ERROR_TRAP: return "Runtime trap";
        case WAH_ERROR_CALL_STACK_OVERFLOW: return "Call stack overflow";
        case WAH_ERROR_MEMORY_OUT_OF_BOUNDS: return "Memory access out of bounds";
        default: return "Unknown error";
    }
}

static inline wah_error_t wah_type_stack_push(wah_type_stack_t *stack, wah_val_type_t type) {
    WAH_BOUNDS_CHECK(stack->sp < WAH_MAX_TYPE_STACK_SIZE, WAH_ERROR_VALIDATION_FAILED);
    stack->data[stack->sp++] = type;
    return WAH_OK;
}

static inline wah_error_t wah_type_stack_pop(wah_type_stack_t *stack, wah_val_type_t *out_type) {
    WAH_BOUNDS_CHECK(stack->sp > 0, WAH_ERROR_VALIDATION_FAILED);
    *out_type = stack->data[--stack->sp];
    return WAH_OK;
}

// Stack depth tracking helpers
static inline wah_error_t wah_validation_push_type(wah_validation_context_t *vctx, wah_val_type_t type) {
    WAH_CHECK(wah_type_stack_push(&vctx->type_stack, vctx->is_unreachable ? WAH_VAL_TYPE_ANY : type));
    vctx->current_stack_depth++;
    if (vctx->current_stack_depth > vctx->max_stack_depth) {
        vctx->max_stack_depth = vctx->current_stack_depth;
    }
    return WAH_OK;
}

static inline wah_error_t wah_validation_pop_type(wah_validation_context_t *vctx, wah_val_type_t *out_type) {
    if (vctx->is_unreachable) {
        *out_type = WAH_VAL_TYPE_ANY;
        // In an unreachable state, pop always succeeds conceptually, and stack height still changes.
        // We still decrement current_stack_depth to track the conceptual stack height.
        // We don't need to pop from type_stack.data as it's already filled with WAH_VAL_TYPE_ANY or ignored.
        vctx->current_stack_depth--; // Always decrement in unreachable state
        return WAH_OK;
    }

    // If reachable, check for stack underflow
    WAH_BOUNDS_CHECK(vctx->current_stack_depth > 0, WAH_ERROR_VALIDATION_FAILED);
    WAH_CHECK(wah_type_stack_pop(&vctx->type_stack, out_type));
    vctx->current_stack_depth--;
    return WAH_OK;
}

// Helper function to validate if an actual type matches an expected type, considering WAH_VAL_TYPE_ANY
static inline wah_error_t wah_validate_type_match(wah_val_type_t actual, wah_val_type_t expected) {
    if (actual == expected || actual == WAH_VAL_TYPE_ANY) {
        return WAH_OK;
    }
    return WAH_ERROR_VALIDATION_FAILED;
}

// Helper to read a section header
static wah_error_t wah_read_section_header(const uint8_t **ptr, const uint8_t *end, wah_section_id_t *id, uint32_t *size) {
    if (*ptr >= end) {
        return WAH_ERROR_UNEXPECTED_EOF;
    }
    *id = (wah_section_id_t)*(*ptr)++;
    return wah_decode_uleb128(ptr, end, size);
}

// --- Internal Section Parsing Functions ---
static wah_error_t wah_parse_type_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    
    module->type_count = count;
    WAH_MALLOC_ARRAY(module->types, count);
    memset(module->types, 0, sizeof(wah_func_type_t) * count);

    for (uint32_t i = 0; i < count; ++i) {
        if (**ptr != WAH_VAL_TYPE_FUNC) {
            return WAH_ERROR_UNKNOWN_SECTION;
        }
        (*ptr)++;

        // Parse parameter types
        uint32_t param_count_type;
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &param_count_type));
        
        module->types[i].param_count = param_count_type;
        WAH_MALLOC_ARRAY(module->types[i].param_types, param_count_type);
        for (uint32_t j = 0; j < param_count_type; ++j) {
            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            wah_val_type_t type = (wah_val_type_t)**ptr;
            (*ptr)++;
            if (!(type == WAH_VAL_TYPE_I32 || type == WAH_VAL_TYPE_I64 ||
                  type == WAH_VAL_TYPE_F32 || type == WAH_VAL_TYPE_F64)) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            module->types[i].param_types[j] = type;
        }

        // Parse result types
        uint32_t result_count_type;
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &result_count_type));
        
        if (result_count_type > 1) {
            return WAH_ERROR_VALIDATION_FAILED;
        }
        
        module->types[i].result_count = result_count_type;
        WAH_MALLOC_ARRAY(module->types[i].result_types, result_count_type);
        
        for (uint32_t j = 0; j < result_count_type; ++j) {
            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            wah_val_type_t type = (wah_val_type_t)**ptr;
            (*ptr)++;
            if (!(type == WAH_VAL_TYPE_I32 || type == WAH_VAL_TYPE_I64 ||
                  type == WAH_VAL_TYPE_F32 || type == WAH_VAL_TYPE_F64)) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            module->types[i].result_types[j] = type;
        }
    }
    return WAH_OK;
}

static wah_error_t wah_parse_function_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    
    module->function_count = count;
    WAH_MALLOC_ARRAY(module->function_type_indices, count);

    for (uint32_t i = 0; i < count; ++i) {
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->function_type_indices[i]));
        
        if (module->function_type_indices[i] >= module->type_count) {
            return WAH_ERROR_VALIDATION_FAILED;
        }
    }
    return WAH_OK;
}

// Validation helper function that handles a single opcode
static wah_error_t wah_validate_opcode(uint16_t opcode_val, const uint8_t **code_ptr, const uint8_t *code_end, wah_validation_context_t *vctx, const wah_code_body_t* code_body) {
    switch (opcode_val) {
#define LOAD_OP(T, max_lg_align) { \
            uint32_t align, offset; \
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &align)); \
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &offset)); \
            if (align > max_lg_align) return WAH_ERROR_VALIDATION_FAILED; \
            wah_val_type_t addr_type; \
            WAH_CHECK(wah_validation_pop_type(vctx, &addr_type)); \
            WAH_CHECK(wah_validate_type_match(addr_type, WAH_VAL_TYPE_I32)); \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_##T); \
        }

#define STORE_OP(T, max_lg_align) { \
            uint32_t align, offset; \
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &align)); \
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &offset)); \
            if (align > max_lg_align) return WAH_ERROR_VALIDATION_FAILED; \
            wah_val_type_t val_type, addr_type; \
            WAH_CHECK(wah_validation_pop_type(vctx, &val_type)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &addr_type)); \
            WAH_CHECK(wah_validate_type_match(val_type, WAH_VAL_TYPE_##T)); \
            WAH_CHECK(wah_validate_type_match(addr_type, WAH_VAL_TYPE_I32)); \
            return WAH_OK; \
        }

        case WAH_OP_I32_LOAD: LOAD_OP(I32, 2)
        case WAH_OP_I64_LOAD: LOAD_OP(I64, 3)
        case WAH_OP_F32_LOAD: LOAD_OP(F32, 2)
        case WAH_OP_F64_LOAD: LOAD_OP(F64, 3)
        case WAH_OP_I32_LOAD8_S: case WAH_OP_I32_LOAD8_U: LOAD_OP(I32, 0)
        case WAH_OP_I32_LOAD16_S: case WAH_OP_I32_LOAD16_U: LOAD_OP(I32, 1)
        case WAH_OP_I64_LOAD8_S: case WAH_OP_I64_LOAD8_U: LOAD_OP(I64, 0)
        case WAH_OP_I64_LOAD16_S: case WAH_OP_I64_LOAD16_U: LOAD_OP(I64, 1)
        case WAH_OP_I64_LOAD32_S: case WAH_OP_I64_LOAD32_U: LOAD_OP(I64, 2)
        case WAH_OP_I32_STORE: STORE_OP(I32, 2)
        case WAH_OP_I64_STORE: STORE_OP(I64, 3)
        case WAH_OP_F32_STORE: STORE_OP(F32, 2)
        case WAH_OP_F64_STORE: STORE_OP(F64, 3)
        case WAH_OP_I32_STORE8: STORE_OP(I32, 0)
        case WAH_OP_I32_STORE16: STORE_OP(I32, 1)
        case WAH_OP_I64_STORE8: STORE_OP(I64, 0)
        case WAH_OP_I64_STORE16: STORE_OP(I64, 1)
        case WAH_OP_I64_STORE32: STORE_OP(I64, 2)

#undef LOAD_OP
#undef STORE_OP

        case WAH_OP_MEMORY_SIZE: {
            uint32_t mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &mem_idx)); // Expect 0x00 for memory index
            if (mem_idx != 0) return WAH_ERROR_VALIDATION_FAILED; // Only memory 0 supported
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); // Pushes current memory size in pages
        }
        case WAH_OP_MEMORY_GROW: {
            uint32_t mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &mem_idx)); // Expect 0x00 for memory index
            if (mem_idx != 0) return WAH_ERROR_VALIDATION_FAILED; // Only memory 0 supported
            wah_val_type_t pages_to_grow_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &pages_to_grow_type)); // Pops pages to grow by (i32)
            WAH_CHECK(wah_validate_type_match(pages_to_grow_type, WAH_VAL_TYPE_I32));
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); // Pushes old memory size in pages (i32)
        }
        case WAH_OP_MEMORY_FILL: {
            uint32_t mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &mem_idx)); // Expect 0x00 for memory index
            if (mem_idx != 0) return WAH_ERROR_VALIDATION_FAILED; // Only memory 0 supported
            wah_val_type_t size_type, val_type, dst_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &size_type)); // Pops size (i32)
            WAH_CHECK(wah_validation_pop_type(vctx, &val_type));  // Pops value (i32)
            WAH_CHECK(wah_validation_pop_type(vctx, &dst_type));  // Pops destination address (i32)
            WAH_CHECK(wah_validate_type_match(size_type, WAH_VAL_TYPE_I32));
            WAH_CHECK(wah_validate_type_match(val_type, WAH_VAL_TYPE_I32));
            WAH_CHECK(wah_validate_type_match(dst_type, WAH_VAL_TYPE_I32));
            return WAH_OK; // Pushes nothing
        }
        case WAH_OP_CALL: {
            uint32_t func_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &func_idx));
            // Basic validation: check if func_idx is within bounds
            if (func_idx >= vctx->module->function_count) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            // Type checking for call
            const wah_func_type_t *called_func_type = &vctx->module->types[vctx->module->function_type_indices[func_idx]];
            // Pop parameters from type stack
            for (int32_t j = called_func_type->param_count - 1; j >= 0; --j) {
                wah_val_type_t popped_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &popped_type));
                WAH_CHECK(wah_validate_type_match(popped_type, called_func_type->param_types[j]));
            }
            // Push results onto type stack
            for (uint32_t j = 0; j < called_func_type->result_count; ++j) {
                WAH_CHECK(wah_validation_push_type(vctx, called_func_type->result_types[j]));
            }
            return WAH_OK;
        }
        case WAH_OP_CALL_INDIRECT: {
            uint32_t type_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &type_idx));
            uint32_t table_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &table_idx));

            // Validate table index
            if (table_idx >= vctx->module->table_count) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            // Only funcref tables are supported for now
            if (vctx->module->tables[table_idx].elem_type != WAH_VAL_TYPE_FUNCREF) {
                return WAH_ERROR_VALIDATION_FAILED;
            }

            // Validate type index
            if (type_idx >= vctx->module->type_count) {
                return WAH_ERROR_VALIDATION_FAILED;
            }

            // Pop function index (i32) from stack
            wah_val_type_t func_idx_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &func_idx_type));
            WAH_CHECK(wah_validate_type_match(func_idx_type, WAH_VAL_TYPE_I32));

            // Get the expected function type
            const wah_func_type_t *expected_func_type = &vctx->module->types[type_idx];

            // Pop parameters from type stack
            for (int32_t j = expected_func_type->param_count - 1; j >= 0; --j) {
                wah_val_type_t popped_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &popped_type));
                WAH_CHECK(wah_validate_type_match(popped_type, expected_func_type->param_types[j]));
            }
            // Push results onto type stack
            for (uint32_t j = 0; j < expected_func_type->result_count; ++j) {
                WAH_CHECK(wah_validation_push_type(vctx, expected_func_type->result_types[j]));
            }
            return WAH_OK;
        }
        case WAH_OP_LOCAL_GET:
        case WAH_OP_LOCAL_SET:
        case WAH_OP_LOCAL_TEE: {
            uint32_t local_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &local_idx));
            
            if (local_idx >= vctx->total_locals) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            
            wah_val_type_t expected_type;
            if (local_idx < vctx->func_type->param_count) {
                expected_type = vctx->func_type->param_types[local_idx];
            } else {
                expected_type = code_body->local_types[local_idx - vctx->func_type->param_count];
            }
            
            if (opcode_val == WAH_OP_LOCAL_GET) {
                return wah_validation_push_type(vctx, expected_type);
            } else if (opcode_val == WAH_OP_LOCAL_SET) {
                wah_val_type_t popped_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &popped_type));
                WAH_CHECK(wah_validate_type_match(popped_type, expected_type));
                return WAH_OK;
            } else if (opcode_val == WAH_OP_LOCAL_TEE) { // WAH_OP_LOCAL_TEE
                wah_val_type_t popped_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &popped_type));
                WAH_CHECK(wah_validate_type_match(popped_type, expected_type));
                return wah_validation_push_type(vctx, expected_type);
            }
            break;
        }

        case WAH_OP_GLOBAL_GET: {
            uint32_t global_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &global_idx));
            if (global_idx >= vctx->module->global_count) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            wah_val_type_t global_type = vctx->module->globals[global_idx].type;
            return wah_validation_push_type(vctx, global_type);
        }
        case WAH_OP_GLOBAL_SET: {
            uint32_t global_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &global_idx));
            if (global_idx >= vctx->module->global_count) {
                return WAH_ERROR_VALIDATION_FAILED;
            }
            if (!vctx->module->globals[global_idx].is_mutable) {
                return WAH_ERROR_VALIDATION_FAILED; // Cannot set immutable global
            }
            wah_val_type_t global_type = vctx->module->globals[global_idx].type;
            wah_val_type_t popped_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &popped_type));
            WAH_CHECK(wah_validate_type_match(popped_type, global_type));
            return WAH_OK;
        }
        
        case WAH_OP_I32_CONST: {
            int32_t val;
            WAH_CHECK(wah_decode_sleb128_32(code_ptr, code_end, &val));
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32);
        }
        case WAH_OP_I64_CONST: {
            int64_t val;
            WAH_CHECK(wah_decode_sleb128_64(code_ptr, code_end, &val));
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I64);
        }
        case WAH_OP_F32_CONST: {
            if (code_end - *code_ptr < 4) return WAH_ERROR_UNEXPECTED_EOF;
            *code_ptr += 4;
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_F32);
        }
        case WAH_OP_F64_CONST: {
            if (code_end - *code_ptr < 8) return WAH_ERROR_UNEXPECTED_EOF;
            *code_ptr += 8;
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_F64);
        }

#define NUM_OPS(N) \
        /* Binary i32/i64 operations */ \
        case WAH_OP_I##N##_ADD: case WAH_OP_I##N##_SUB: case WAH_OP_I##N##_MUL: \
        case WAH_OP_I##N##_DIV_S: case WAH_OP_I##N##_DIV_U: case WAH_OP_I##N##_REM_S: case WAH_OP_I##N##_REM_U: \
        case WAH_OP_I##N##_AND: case WAH_OP_I##N##_OR: case WAH_OP_I##N##_XOR: \
        case WAH_OP_I##N##_SHL: case WAH_OP_I##N##_SHR_S: case WAH_OP_I##N##_SHR_U: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            WAH_CHECK(wah_validate_type_match(type1, WAH_VAL_TYPE_I##N)); \
            WAH_CHECK(wah_validate_type_match(type2, WAH_VAL_TYPE_I##N)); \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I##N); \
        } \
        case WAH_OP_I##N##_EQ: case WAH_OP_I##N##_NE: \
        case WAH_OP_I##N##_LT_S: case WAH_OP_I##N##_LT_U: case WAH_OP_I##N##_GT_S: case WAH_OP_I##N##_GT_U: \
        case WAH_OP_I##N##_LE_S: case WAH_OP_I##N##_LE_U: case WAH_OP_I##N##_GE_S: case WAH_OP_I##N##_GE_U: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            WAH_CHECK(wah_validate_type_match(type1, WAH_VAL_TYPE_I##N)); \
            WAH_CHECK(wah_validate_type_match(type2, WAH_VAL_TYPE_I##N)); \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); /* Comparisons return i32 */ \
        } \
        \
        /* Unary i32/i64 operations */ \
        case WAH_OP_I##N##_EQZ: { \
            wah_val_type_t type; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type)); \
            WAH_CHECK(wah_validate_type_match(type, WAH_VAL_TYPE_I##N)); \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); \
        } \
        \
        /* Binary f32/f64 operations */ \
        case WAH_OP_F##N##_ADD: case WAH_OP_F##N##_SUB: case WAH_OP_F##N##_MUL: case WAH_OP_F##N##_DIV: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            WAH_CHECK(wah_validate_type_match(type1, WAH_VAL_TYPE_F##N)); \
            WAH_CHECK(wah_validate_type_match(type2, WAH_VAL_TYPE_F##N)); \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_F##N); \
        } \
        case WAH_OP_F##N##_EQ: case WAH_OP_F##N##_NE: \
        case WAH_OP_F##N##_LT: case WAH_OP_F##N##_GT: case WAH_OP_F##N##_LE: case WAH_OP_F##N##_GE: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            WAH_CHECK(wah_validate_type_match(type1, WAH_VAL_TYPE_F##N)); \
            WAH_CHECK(wah_validate_type_match(type2, WAH_VAL_TYPE_F##N)); \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); /* Comparisons return i32 */ \
        }

        NUM_OPS(32)
        NUM_OPS(64)

#undef NUM_OPS

#define UNARY_OP(T) { \
    wah_val_type_t type; \
    WAH_CHECK(wah_validation_pop_type(vctx, &type)); \
    WAH_CHECK(wah_validate_type_match(type, WAH_VAL_TYPE_##T)); \
    return wah_validation_push_type(vctx, WAH_VAL_TYPE_##T); \
}

#define BINARY_OP(T) { \
    wah_val_type_t type1, type2; \
    WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
    WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
    WAH_CHECK(wah_validate_type_match(type1, WAH_VAL_TYPE_##T)); \
    WAH_CHECK(wah_validate_type_match(type2, WAH_VAL_TYPE_##T)); \
    return wah_validation_push_type(vctx, WAH_VAL_TYPE_##T); \
}

        // Integer Unary Operations
        case WAH_OP_I32_CLZ: case WAH_OP_I32_CTZ: case WAH_OP_I32_POPCNT: UNARY_OP(I32)
        case WAH_OP_I64_CLZ: case WAH_OP_I64_CTZ: case WAH_OP_I64_POPCNT: UNARY_OP(I64)

        // Integer Binary Operations
        case WAH_OP_I32_ROTL: case WAH_OP_I32_ROTR: BINARY_OP(I32)
        case WAH_OP_I64_ROTL: case WAH_OP_I64_ROTR: BINARY_OP(I64)

        // Floating-Point Unary Operations
        case WAH_OP_F32_ABS: case WAH_OP_F32_NEG: case WAH_OP_F32_CEIL: case WAH_OP_F32_FLOOR:
        case WAH_OP_F32_TRUNC: case WAH_OP_F32_NEAREST: case WAH_OP_F32_SQRT: UNARY_OP(F32)
        case WAH_OP_F64_ABS: case WAH_OP_F64_NEG: case WAH_OP_F64_CEIL: case WAH_OP_F64_FLOOR:
        case WAH_OP_F64_TRUNC: case WAH_OP_F64_NEAREST: case WAH_OP_F64_SQRT: UNARY_OP(F64)

        // Floating-Point Binary Operations
        case WAH_OP_F32_MIN: case WAH_OP_F32_MAX: case WAH_OP_F32_COPYSIGN: BINARY_OP(F32)
        case WAH_OP_F64_MIN: case WAH_OP_F64_MAX: case WAH_OP_F64_COPYSIGN: BINARY_OP(F64)

#define POP_PUSH(INPUT_TYPE, OUTPUT_TYPE) { \
    wah_val_type_t type; \
    WAH_CHECK(wah_validation_pop_type(vctx, &type)); \
    WAH_CHECK(wah_validate_type_match(type, WAH_VAL_TYPE_##INPUT_TYPE)); \
    return wah_validation_push_type(vctx, WAH_VAL_TYPE_##OUTPUT_TYPE); \
}

        // Conversion Operators
        case WAH_OP_I32_WRAP_I64: POP_PUSH(I64, I32)
        case WAH_OP_I32_TRUNC_F32_S: case WAH_OP_I32_TRUNC_F32_U: POP_PUSH(F32, I32)
        case WAH_OP_I32_TRUNC_F64_S: case WAH_OP_I32_TRUNC_F64_U: POP_PUSH(F64, I32)
        case WAH_OP_I64_EXTEND_I32_S: case WAH_OP_I64_EXTEND_I32_U: POP_PUSH(I32, I64)
        case WAH_OP_I64_TRUNC_F32_S: case WAH_OP_I64_TRUNC_F32_U: POP_PUSH(F32, I64)
        case WAH_OP_I64_TRUNC_F64_S: case WAH_OP_I64_TRUNC_F64_U: POP_PUSH(F64, I64)
        case WAH_OP_F32_CONVERT_I32_S: case WAH_OP_F32_CONVERT_I32_U: POP_PUSH(I32, F32)
        case WAH_OP_F32_CONVERT_I64_S: case WAH_OP_F32_CONVERT_I64_U: POP_PUSH(I64, F32)
        case WAH_OP_F32_DEMOTE_F64: POP_PUSH(F64, F32)
        case WAH_OP_F64_CONVERT_I32_S: case WAH_OP_F64_CONVERT_I32_U: POP_PUSH(I32, F64)
        case WAH_OP_F64_CONVERT_I64_S: case WAH_OP_F64_CONVERT_I64_U: POP_PUSH(I64, F64)
        case WAH_OP_F64_PROMOTE_F32: POP_PUSH(F32, F64)
        case WAH_OP_I32_REINTERPRET_F32: POP_PUSH(F32, I32)
        case WAH_OP_I64_REINTERPRET_F64: POP_PUSH(F64, I64)
        case WAH_OP_F32_REINTERPRET_I32: POP_PUSH(I32, F32)
        case WAH_OP_F64_REINTERPRET_I64: POP_PUSH(I64, F64)

#undef UNARY_OP
#undef BINARY_OP
#undef POP_PUSH

        // Parametric operations
        case WAH_OP_DROP: {
            wah_val_type_t type;
            return wah_validation_pop_type(vctx, &type);
        }
        
        case WAH_OP_SELECT: {
            wah_val_type_t c_type, b_type, a_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &c_type));
            WAH_CHECK(wah_validate_type_match(c_type, WAH_VAL_TYPE_I32));
            WAH_CHECK(wah_validation_pop_type(vctx, &b_type));
            WAH_CHECK(wah_validation_pop_type(vctx, &a_type));
            // If a_type and b_type are different, and neither is ANY, then it's an error.
            if (a_type != b_type && a_type != WAH_VAL_TYPE_ANY && b_type != WAH_VAL_TYPE_ANY) return WAH_ERROR_VALIDATION_FAILED;
            // If either is ANY, the result is ANY. Otherwise, it's a_type (which equals b_type).
            if (a_type == WAH_VAL_TYPE_ANY || b_type == WAH_VAL_TYPE_ANY) {
                return wah_validation_push_type(vctx, WAH_VAL_TYPE_ANY);
            } else {
                return wah_validation_push_type(vctx, a_type);
            }
        }
        
        // Control flow operations
        case WAH_OP_NOP:
            return WAH_OK;
        case WAH_OP_UNREACHABLE:
            vctx->is_unreachable = true;
            return WAH_OK;
        
        case WAH_OP_BLOCK:
        case WAH_OP_LOOP:
        case WAH_OP_IF: {
            wah_val_type_t cond_type;
            if (opcode_val == WAH_OP_IF) {
                WAH_CHECK(wah_validation_pop_type(vctx, &cond_type));
                if (cond_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
            }

            int32_t block_type_val;
            WAH_CHECK(wah_decode_sleb128_32(code_ptr, code_end, &block_type_val));

            WAH_BOUNDS_CHECK(vctx->control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED);
            wah_validation_control_frame_t* frame = &vctx->control_stack[vctx->control_sp++];
            frame->opcode = (wah_opcode_t)opcode_val;
            frame->else_found = false;
            frame->is_unreachable = vctx->is_unreachable; // Initialize with current reachability
            frame->stack_height = vctx->current_stack_depth; // Store current stack height
            
            wah_func_type_t* bt = &frame->block_type;
            memset(bt, 0, sizeof(wah_func_type_t));

            if (block_type_val < 0) { // Value type
                wah_val_type_t result_type = WAH_VAL_TYPE_BLOCK_TYPE;
                bool valid_type = true;
                switch(block_type_val) {
                    case -1: result_type = WAH_VAL_TYPE_I32; break;
                    case -2: result_type = WAH_VAL_TYPE_I64; break;
                    case -3: result_type = WAH_VAL_TYPE_F32; break;
                    case -4: result_type = WAH_VAL_TYPE_F64; break;
                    case -0x40: break; // empty
                    default: valid_type = false; break;
                }
                if (!valid_type) return WAH_ERROR_VALIDATION_FAILED;
                
                if (result_type != WAH_VAL_TYPE_BLOCK_TYPE) {
                    bt->result_count = 1;
                    WAH_MALLOC_ARRAY(bt->result_types, 1);
                    bt->result_types[0] = result_type;
                }
            } else { // Function type index
                uint32_t type_idx = (uint32_t)block_type_val;
                if (type_idx >= vctx->module->type_count) return WAH_ERROR_VALIDATION_FAILED;
                const wah_func_type_t* referenced_type = &vctx->module->types[type_idx];
                
                bt->param_count = referenced_type->param_count;
                if (bt->param_count > 0) {
                    WAH_MALLOC_ARRAY(bt->param_types, bt->param_count);
                    memcpy(bt->param_types, referenced_type->param_types, sizeof(wah_val_type_t) * bt->param_count);
                }
                
                bt->result_count = referenced_type->result_count;
                if (bt->result_count > 0) {
                    WAH_MALLOC_ARRAY(bt->result_types, bt->result_count);
                    memcpy(bt->result_types, referenced_type->result_types, sizeof(wah_val_type_t) * bt->result_count);
                }
            }

            // Pop params
            for (int32_t i = bt->param_count - 1; i >= 0; --i) {
                wah_val_type_t param_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &param_type));
                WAH_CHECK(wah_validate_type_match(param_type, bt->param_types[i]));
            }
            
            frame->type_stack_sp = vctx->type_stack.sp;
            
            return WAH_OK;
        }
        case WAH_OP_ELSE: {
            WAH_BOUNDS_CHECK(vctx->control_sp > 0, WAH_ERROR_VALIDATION_FAILED);
            wah_validation_control_frame_t* frame = &vctx->control_stack[vctx->control_sp - 1];
            if (frame->opcode != WAH_OP_IF || frame->else_found) return WAH_ERROR_VALIDATION_FAILED;
            frame->else_found = true;

            // Pop results of 'if' branch and verify
            for (int32_t i = frame->block_type.result_count - 1; i >= 0; --i) {
                wah_val_type_t result_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &result_type));
                // If the stack was unreachable, any type is fine. Otherwise, it must match.
                WAH_CHECK(wah_validate_type_match(result_type, frame->block_type.result_types[i]));
            }
            
            // Reset stack to the state before the 'if' block
            vctx->type_stack.sp = frame->type_stack_sp;
            vctx->current_stack_depth = frame->type_stack_sp;

            // The 'else' branch is now reachable
            vctx->is_unreachable = false;
            return WAH_OK;
        }
        case WAH_OP_END: {
            if (vctx->control_sp == 0) {
                // This is the final 'end' of the function body.
                // Final validation for the function's result types.
                // The stack should contain exactly the function's result types.
                if (vctx->func_type->result_count == 0) {
                    if (vctx->type_stack.sp != 0) return WAH_ERROR_VALIDATION_FAILED;
                } else { // result_count == 1
                    if (vctx->type_stack.sp != 1) return WAH_ERROR_VALIDATION_FAILED;
                    wah_val_type_t result_type;
                    WAH_CHECK(wah_validation_pop_type(vctx, &result_type));
                    WAH_CHECK(wah_validate_type_match(result_type, vctx->func_type->result_types[0]));
                }
                // Reset unreachable state for the function's end
                vctx->is_unreachable = false;
                return WAH_OK;
            }
            
            wah_validation_control_frame_t* frame = &vctx->control_stack[vctx->control_sp - 1];
            
            // Pop results from the executed branch and verify
            for (int32_t i = frame->block_type.result_count - 1; i >= 0; --i) {
                wah_val_type_t result_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &result_type));
                WAH_CHECK(wah_validate_type_match(result_type, frame->block_type.result_types[i]));
            }
            
            // Reset stack to the state before the block
            vctx->type_stack.sp = frame->type_stack_sp;
            vctx->current_stack_depth = frame->type_stack_sp;

            // Push final results of the block
            for (uint32_t i = 0; i < frame->block_type.result_count; ++i) {
                WAH_CHECK(wah_validation_push_type(vctx, frame->block_type.result_types[i]));
            }
            
            // Free memory allocated for the block type in the control frame
            free(frame->block_type.param_types);
            free(frame->block_type.result_types);

            vctx->control_sp--;
            // Restore the unreachable state from the parent control frame
            if (vctx->control_sp > 0) {
                vctx->is_unreachable = vctx->control_stack[vctx->control_sp - 1].is_unreachable;
            } else {
                vctx->is_unreachable = false; // Function level is reachable by default
            }
            return WAH_OK;
        }

        case WAH_OP_BR: {
            uint32_t label_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &label_idx));
            WAH_BOUNDS_CHECK(label_idx < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED);

            wah_validation_control_frame_t *target_frame = &vctx->control_stack[vctx->control_sp - 1 - label_idx];

            // Pop the expected result types of the target block from the current stack
            // The stack must contain these values for the branch to be valid.
            for (int32_t i = target_frame->block_type.result_count - 1; i >= 0; --i) {
                wah_val_type_t actual_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &actual_type));
                WAH_CHECK(wah_validate_type_match(actual_type, target_frame->block_type.result_types[i]));
            }

            // Discard any remaining values on the stack above the target frame's stack height
            while (vctx->current_stack_depth > target_frame->stack_height) {
                wah_val_type_t temp_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &temp_type));
            }

            vctx->is_unreachable = true; // br makes the current path unreachable
            return WAH_OK;
        }

        case WAH_OP_BR_IF: {
            uint32_t label_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &label_idx));
            WAH_BOUNDS_CHECK(label_idx < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED);

            // Pop condition (i32) from stack for br_if
            wah_val_type_t cond_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &cond_type));
            WAH_CHECK(wah_validate_type_match(cond_type, WAH_VAL_TYPE_I32));

            // If the current path is unreachable, the stack is polymorphic, so type checks are trivial.
            // We only need to ensure the conceptual stack height is maintained.
            if (vctx->is_unreachable) {
                return WAH_OK;
            }

            wah_validation_control_frame_t *target_frame = &vctx->control_stack[vctx->control_sp - 1 - label_idx];

            // Check if there are enough values on the stack for the target block's results
            // (which become parameters to the target block if branched to).
            if (vctx->current_stack_depth < target_frame->block_type.result_count) {
                return WAH_ERROR_VALIDATION_FAILED;
            }

            // Check the types of these values without popping them
            for (uint32_t i = 0; i < target_frame->block_type.result_count; ++i) {
                // Access the type stack directly to peek at values
                wah_val_type_t actual_type = vctx->type_stack.data[vctx->type_stack.sp - target_frame->block_type.result_count + i];
                WAH_CHECK(wah_validate_type_match(actual_type, target_frame->block_type.result_types[i]));
            }

            // The stack state for the fall-through path is now correct (condition popped).
            // The current path remains reachable.
            return WAH_OK;
        }

        case WAH_OP_BR_TABLE: {
            uint32_t num_targets;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &num_targets));

            // Decode target table (vector of label_idx)
            for (uint32_t i = 0; i < num_targets; ++i) {
                uint32_t label_idx;
                WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &label_idx));
                WAH_BOUNDS_CHECK(label_idx < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED);
            }

            // Decode default target (label_idx)
            uint32_t default_label_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &default_label_idx));
            WAH_BOUNDS_CHECK(default_label_idx < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED);

            // Pop index (i32) from stack
            wah_val_type_t index_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &index_type));
            WAH_CHECK(wah_validate_type_match(index_type, WAH_VAL_TYPE_I32));

            vctx->is_unreachable = true; // The stack becomes unreachable after br_table
            return WAH_OK;
        }

        case WAH_OP_RETURN:
            // Pop the function's result types from the stack
            for (int32_t j = vctx->func_type->result_count - 1; j >= 0; --j) {
                wah_val_type_t popped_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &popped_type));
                WAH_CHECK(wah_validate_type_match(popped_type, vctx->func_type->result_types[j]));
            }
            vctx->is_unreachable = true; // After return, the current path becomes unreachable
            return WAH_OK;

        default:
            break; // Assume other opcodes are valid for now
    }
    return WAH_OK;
}

static wah_error_t wah_parse_code_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    wah_error_t err = WAH_OK;
    wah_validation_context_t vctx;
    memset(&vctx, 0, sizeof(wah_validation_context_t));

    uint32_t count;
    WAH_CHECK_GOTO(wah_decode_uleb128(ptr, section_end, &count), cleanup);
    if (count != module->function_count) {
        err = WAH_ERROR_VALIDATION_FAILED;
        goto cleanup;
    }
    module->code_count = count;
    WAH_MALLOC_ARRAY_GOTO(module->code_bodies, count, cleanup);
    memset(module->code_bodies, 0, sizeof(wah_code_body_t) * count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t body_size;
        WAH_CHECK_GOTO(wah_decode_uleb128(ptr, section_end, &body_size), cleanup);

        const uint8_t *code_body_end = *ptr + body_size;

        // Parse locals
        uint32_t num_local_entries;
        WAH_CHECK_GOTO(wah_decode_uleb128(ptr, code_body_end, &num_local_entries), cleanup);

        uint32_t current_local_count = 0;
        const uint8_t* ptr_count = *ptr;
        for (uint32_t j = 0; j < num_local_entries; ++j) {
            uint32_t local_type_count;
            WAH_CHECK_GOTO(wah_decode_uleb128(&ptr_count, code_body_end, &local_type_count), cleanup);
            ptr_count++; // Skip the actual type byte
            current_local_count += local_type_count;
        }
        module->code_bodies[i].local_count = current_local_count;
        WAH_MALLOC_ARRAY_GOTO(module->code_bodies[i].local_types, current_local_count, cleanup);

        uint32_t local_idx = 0;
        for (uint32_t j = 0; j < num_local_entries; ++j) {
            uint32_t local_type_count;
            WAH_CHECK_GOTO(wah_decode_uleb128(ptr, code_body_end, &local_type_count), cleanup);
            wah_val_type_t type = (wah_val_type_t)*(*ptr)++;
            for (uint32_t k = 0; k < local_type_count; ++k) {
                module->code_bodies[i].local_types[local_idx++] = type;
            }
        }

        module->code_bodies[i].code_size = code_body_end - *ptr;
        module->code_bodies[i].code = *ptr;

        // --- Validation Pass for Code Body ---
        const wah_func_type_t *func_type = &module->types[module->function_type_indices[i]];
        
        memset(&vctx, 0, sizeof(wah_validation_context_t));
        vctx.is_unreachable = false; // Functions start in a reachable state
        vctx.module = module;
        vctx.func_type = func_type;
        vctx.total_locals = func_type->param_count + module->code_bodies[i].local_count;

        const uint8_t *code_ptr_validation = module->code_bodies[i].code;
        const uint8_t *validation_end = code_ptr_validation + module->code_bodies[i].code_size;

        while (code_ptr_validation < validation_end) {
            uint16_t current_opcode_val;
            WAH_CHECK_GOTO(wah_decode_opcode(&code_ptr_validation, validation_end, &current_opcode_val), cleanup);
            
            if (current_opcode_val == WAH_OP_END) {
                 if (vctx.control_sp == 0) { // End of function
                    if (vctx.func_type->result_count == 0) {
                        if (vctx.type_stack.sp != 0) { // Unmatched control frames
                            err = WAH_ERROR_VALIDATION_FAILED;
                            goto cleanup;
                        }
                    } else { // result_count == 1
                        // If unreachable, the stack is polymorphic, so we don't strictly check sp.
                        // We still pop to ensure the conceptual stack height is correct.
                        wah_val_type_t result_type;
                        WAH_CHECK_GOTO(wah_validation_pop_type(&vctx, &result_type), cleanup);
                        WAH_CHECK_GOTO(wah_validate_type_match(result_type, vctx.func_type->result_types[0]), cleanup);
                    }
                    break; // End of validation loop
                }
            }
            WAH_CHECK_GOTO(wah_validate_opcode(current_opcode_val, &code_ptr_validation, validation_end, &vctx, &module->code_bodies[i]), cleanup);
        }
        if (vctx.control_sp != 0) {
            err = WAH_ERROR_VALIDATION_FAILED;
            goto cleanup;
        }
        // --- End Validation Pass ---
        
        module->code_bodies[i].max_stack_depth = vctx.max_stack_depth;

        // Pre-parse the code for optimized execution
        WAH_CHECK_GOTO(wah_preparse_code(module, i, module->code_bodies[i].code, module->code_bodies[i].code_size, &module->code_bodies[i].parsed_code), cleanup);

        *ptr = code_body_end;
    }
    err = WAH_OK; // Ensure err is WAH_OK if everything succeeded

cleanup:
    if (err != WAH_OK) {
        // Free memory allocated for control frames during validation
        for (int32_t j = vctx.control_sp - 1; j >= 0; --j) {
            wah_validation_control_frame_t* frame = &vctx.control_stack[j];
            free(frame->block_type.param_types);
            free(frame->block_type.result_types);
        }

        if (module->code_bodies) {
            for (uint32_t i = 0; i < module->code_count; ++i) {
                free(module->code_bodies[i].local_types);
                wah_free_parsed_code(&module->code_bodies[i].parsed_code);
            }
            free(module->code_bodies);
            module->code_bodies = NULL;
        }
    }
    return err;
}

static wah_error_t wah_parse_global_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    module->global_count = count;
    WAH_MALLOC_ARRAY(module->globals, count);
    memset(module->globals, 0, sizeof(wah_global_t) * count);

    for (uint32_t i = 0; i < count; ++i) {
        // Global Type
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        wah_val_type_t global_declared_type = (wah_val_type_t)*(*ptr)++;
        module->globals[i].type = global_declared_type;

        // Mutability
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        module->globals[i].is_mutable = (*(*ptr)++ == 1);

        // Init Expr (only const expressions are supported for initial values)
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        wah_opcode_t opcode = (wah_opcode_t)*(*ptr)++;
        switch (opcode) {
            case WAH_OP_I32_CONST: {
                if (global_declared_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
                int32_t val;
                WAH_CHECK(wah_decode_sleb128_32(ptr, section_end, &val));
                module->globals[i].initial_value.i32 = val;
                break;
            }
            case WAH_OP_I64_CONST: {
                if (global_declared_type != WAH_VAL_TYPE_I64) return WAH_ERROR_VALIDATION_FAILED;
                int64_t val;
                WAH_CHECK(wah_decode_sleb128_64(ptr, section_end, &val));
                module->globals[i].initial_value.i64 = val;
                break;
            }
            case WAH_OP_F32_CONST: {
                if (global_declared_type != WAH_VAL_TYPE_F32) return WAH_ERROR_VALIDATION_FAILED;
                if (*ptr + 4 > section_end) return WAH_ERROR_UNEXPECTED_EOF;
                module->globals[i].initial_value.f32 = wah_canonicalize_f32(wah_read_f32_le(*ptr));
                *ptr += 4;
                break;
            }
            case WAH_OP_F64_CONST: {
                if (global_declared_type != WAH_VAL_TYPE_F64) return WAH_ERROR_VALIDATION_FAILED;
                if (*ptr + 8 > section_end) return WAH_ERROR_UNEXPECTED_EOF;
                module->globals[i].initial_value.f64 = wah_canonicalize_f64(wah_read_f64_le(*ptr));
                *ptr += 8;
                break;
            }
            default: {
                // Only const expressions supported for global initializers for now
                return WAH_ERROR_VALIDATION_FAILED; 
            }
        }
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        if (*(*ptr)++ != WAH_OP_END) { // Expect END opcode after init_expr
            return WAH_ERROR_VALIDATION_FAILED; 
        }
    }
    return WAH_OK;
}

static wah_error_t wah_parse_memory_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    module->memory_count = count;
    if (count > 0) {
        WAH_MALLOC_ARRAY(module->memories, count);
        memset(module->memories, 0, sizeof(wah_memory_type_t) * count);

        for (uint32_t i = 0; i < count; ++i) {
            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            uint8_t flags = *(*ptr)++; // Flags for memory type (0x00 for fixed, 0x01 for resizable)

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->memories[i].min_pages));
            
            if (flags & 0x01) { // If resizable, read max_pages
                WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->memories[i].max_pages));
            } else {
                module->memories[i].max_pages = module->memories[i].min_pages; // Fixed size, max is same as min
            }
        }
    }
    return WAH_OK;
}

static wah_error_t wah_parse_table_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    module->table_count = count;
    if (count > 0) {
        WAH_MALLOC_ARRAY(module->tables, count);
        memset(module->tables, 0, sizeof(wah_table_type_t) * count);

        for (uint32_t i = 0; i < count; ++i) {
            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            module->tables[i].elem_type = (wah_val_type_t)*(*ptr)++;
            // Only funcref is supported for now
            if (module->tables[i].elem_type != WAH_VAL_TYPE_FUNCREF) {
                return WAH_ERROR_VALIDATION_FAILED;
            }

            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            uint8_t flags = *(*ptr)++; // Flags for table type (0x00 for fixed, 0x01 for resizable)

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->tables[i].min_elements));

            if (flags & 0x01) { // If resizable, read max_elements
                WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->tables[i].max_elements));
            } else {
                module->tables[i].max_elements = module->tables[i].min_elements; // Fixed size, max is same as min
            }
        }
    }
    return WAH_OK;
}

// Placeholder for unimplemented sections
static wah_error_t wah_parse_unimplemented_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    (void)module; // Suppress unused parameter warning
    *ptr = section_end; // Skip the section
    return WAH_OK;
}

static wah_error_t wah_parse_custom_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    return wah_parse_unimplemented_section(ptr, section_end, module);
}

static wah_error_t wah_parse_import_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    return wah_parse_unimplemented_section(ptr, section_end, module);
}

static wah_error_t wah_parse_export_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    return wah_parse_unimplemented_section(ptr, section_end, module);
}

static wah_error_t wah_parse_start_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    return wah_parse_unimplemented_section(ptr, section_end, module);
}

static wah_error_t wah_parse_element_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    module->element_segment_count = count;
    if (count > 0) {
        WAH_MALLOC_ARRAY(module->element_segments, count);
        memset(module->element_segments, 0, sizeof(wah_element_segment_t) * count);

        for (uint32_t i = 0; i < count; ++i) {
            wah_element_segment_t *segment = &module->element_segments[i];

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->table_idx));
            // For now, only table 0 is supported
            if (segment->table_idx != 0) return WAH_ERROR_VALIDATION_FAILED;

            // Parse offset_expr (expected to be i32.const X end)
            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            wah_opcode_t opcode = (wah_opcode_t)*(*ptr)++;
            if (opcode != WAH_OP_I32_CONST) return WAH_ERROR_VALIDATION_FAILED;

            int32_t offset_val;
            WAH_CHECK(wah_decode_sleb128_32(ptr, section_end, &offset_val));
            segment->offset = (uint32_t)offset_val;

            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            if (*(*ptr)++ != WAH_OP_END) return WAH_ERROR_VALIDATION_FAILED;

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->num_elems));

            // Validate that the segment fits within the table's limits
            if (segment->table_idx >= module->table_count) return WAH_ERROR_VALIDATION_FAILED;
            if ((uint64_t)segment->offset + segment->num_elems > module->tables[segment->table_idx].min_elements) {
                return WAH_ERROR_VALIDATION_FAILED;
            }

            if (segment->num_elems > 0) {
                WAH_MALLOC_ARRAY(segment->func_indices, segment->num_elems);
                for (uint32_t j = 0; j < segment->num_elems; ++j) {
                    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->func_indices[j]));
                }
            }
        }
    }
    return WAH_OK;
}

static wah_error_t wah_parse_data_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    return wah_parse_unimplemented_section(ptr, section_end, module);
}

static wah_error_t wah_parse_datacount_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    return wah_parse_unimplemented_section(ptr, section_end, module);
}

// Pre-parsing function to convert bytecode to optimized structure
static wah_error_t wah_preparse_code(const wah_module_t* module, uint32_t func_idx, const uint8_t *code, uint32_t code_size, wah_parsed_code_t *parsed_code) {
    (void)module; (void)func_idx; // Suppress unused parameter warnings

    wah_error_t err = WAH_OK;
    memset(parsed_code, 0, sizeof(wah_parsed_code_t));

    typedef struct { wah_opcode_t opcode; uint32_t target_idx; } wah_control_frame_t;

    uint32_t* block_targets = NULL;
    uint32_t block_target_count = 0, block_target_capacity = 0;
    wah_control_frame_t control_stack[WAH_MAX_CONTROL_DEPTH];
    uint32_t control_sp = 0;
    uint32_t preparsed_size = 0;

    const uint8_t *ptr = code, *end = code + code_size;

    // --- Pass 1: Find block boundaries and calculate preparsed size ---
    while (ptr < end) {
        const uint8_t* instr_ptr = ptr;
        uint16_t opcode;
        WAH_CHECK_GOTO(wah_decode_opcode(&ptr, end, &opcode), cleanup);

        uint32_t preparsed_instr_size = sizeof(uint16_t);

        #define GROW_BLOCK_TARGETS() do { \
                if (block_target_count >= block_target_capacity) { \
                    block_target_capacity = block_target_capacity == 0 ? 16 : block_target_capacity * 2; \
                    WAH_REALLOC_ARRAY_GOTO(block_targets, block_target_capacity, cleanup); \
                } \
            } while (0)

        switch (opcode) {
            case WAH_OP_BLOCK: case WAH_OP_LOOP: case WAH_OP_IF: {
                int32_t block_type;
                WAH_CHECK_GOTO(wah_decode_sleb128_32(&ptr, end, &block_type), cleanup);
                GROW_BLOCK_TARGETS();
                WAH_BOUNDS_CHECK_GOTO(control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED, cleanup);
                uint32_t target_idx = block_target_count++;
                block_targets[target_idx] = preparsed_size; // To be overwritten for WAH_OP_IF
                control_stack[control_sp++] = (wah_control_frame_t){.opcode=(wah_opcode_t)opcode, .target_idx=target_idx};
                preparsed_instr_size = (opcode == WAH_OP_IF) ? sizeof(uint16_t) + sizeof(uint32_t) : 0;
                break;
            }
            case WAH_OP_ELSE: {
                WAH_BOUNDS_CHECK_GOTO(control_sp > 0 && control_stack[control_sp - 1].opcode == WAH_OP_IF, WAH_ERROR_VALIDATION_FAILED, cleanup);
                preparsed_instr_size = sizeof(uint16_t) + sizeof(uint32_t);
                block_targets[control_stack[control_sp - 1].target_idx] = preparsed_size + preparsed_instr_size;
                GROW_BLOCK_TARGETS();
                uint32_t target_idx = block_target_count++;
                control_stack[control_sp - 1] = (wah_control_frame_t){.opcode=(wah_opcode_t)opcode, .target_idx=target_idx};
                break;
            }
            case WAH_OP_END: {
                if (control_sp > 0) {
                    wah_control_frame_t frame = control_stack[--control_sp];
                    if (frame.opcode != WAH_OP_LOOP) { // BLOCK, IF, ELSE
                        block_targets[frame.target_idx] = preparsed_size;
                    }
                    preparsed_instr_size = 0;
                } else { // Final END
                    preparsed_instr_size = sizeof(uint16_t);
                }
                break;
            }
            case WAH_OP_BR: case WAH_OP_BR_IF: {
                 uint32_t d;
                 WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &d), cleanup);
                 preparsed_instr_size += sizeof(uint32_t);
                 break;
            }
            case WAH_OP_BR_TABLE: {
                uint32_t num_targets;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &num_targets), cleanup);
                preparsed_instr_size += sizeof(uint32_t) * (num_targets + 2);
                for (uint32_t i = 0; i < num_targets + 1; ++i) {
                    uint32_t d;
                    WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &d), cleanup);
                }
                break;
            }
            case WAH_OP_LOCAL_GET: case WAH_OP_LOCAL_SET: case WAH_OP_LOCAL_TEE:
            case WAH_OP_GLOBAL_GET: case WAH_OP_GLOBAL_SET: case WAH_OP_CALL: {
                uint32_t v;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &v), cleanup);
                preparsed_instr_size += sizeof(uint32_t);
                break;
            }
            case WAH_OP_CALL_INDIRECT: {
                uint32_t t, i;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &t), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &i), cleanup);
                preparsed_instr_size += sizeof(uint32_t) * 2;
                break;
            }
            case WAH_OP_I32_CONST: {
                int32_t v;
                WAH_CHECK_GOTO(wah_decode_sleb128_32(&ptr, end, &v), cleanup);
                preparsed_instr_size += sizeof(int32_t);
                break;
            }
            case WAH_OP_I64_CONST: {
                int64_t v;
                WAH_CHECK_GOTO(wah_decode_sleb128_64(&ptr, end, &v), cleanup);
                preparsed_instr_size += sizeof(int64_t);
                break;
            }
            case WAH_OP_F32_CONST: ptr += 4; preparsed_instr_size += 4; break;
            case WAH_OP_F64_CONST: ptr += 8; preparsed_instr_size += 8; break;

            case WAH_OP_I32_LOAD: case WAH_OP_I64_LOAD: case WAH_OP_F32_LOAD: case WAH_OP_F64_LOAD:
            case WAH_OP_I32_LOAD8_S: case WAH_OP_I32_LOAD8_U: case WAH_OP_I32_LOAD16_S: case WAH_OP_I32_LOAD16_U:
            case WAH_OP_I64_LOAD8_S: case WAH_OP_I64_LOAD8_U: case WAH_OP_I64_LOAD16_S: case WAH_OP_I64_LOAD16_U:
            case WAH_OP_I64_LOAD32_S: case WAH_OP_I64_LOAD32_U:
            case WAH_OP_I32_STORE: case WAH_OP_I64_STORE: case WAH_OP_F32_STORE: case WAH_OP_F64_STORE:
            case WAH_OP_I32_STORE8: case WAH_OP_I32_STORE16: case WAH_OP_I64_STORE8: case WAH_OP_I64_STORE16: case WAH_OP_I64_STORE32: {
                uint32_t a, o;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &a), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &o), cleanup);
                preparsed_instr_size += sizeof(uint32_t);
                break;
            }
            case WAH_OP_MEMORY_SIZE: case WAH_OP_MEMORY_GROW: case WAH_OP_MEMORY_FILL: {
                uint32_t m;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &m), cleanup);
                break;
            }
        }
        preparsed_size += preparsed_instr_size;
        ptr = instr_ptr + (ptr - instr_ptr); // This is not a bug; it's to make it clear that ptr is advanced inside the switch
    }
    WAH_BOUNDS_CHECK_GOTO(control_sp == 0, WAH_ERROR_VALIDATION_FAILED, cleanup);

    // --- Allocate and perform Pass 2: Generate preparsed bytecode ---
    WAH_MALLOC_ARRAY_GOTO(parsed_code->bytecode, preparsed_size, cleanup);
    parsed_code->bytecode_size = preparsed_size;

    ptr = code; control_sp = 0;
    uint8_t* write_ptr = parsed_code->bytecode;
    uint32_t current_block_idx = 0;

    while (ptr < end) {
        uint16_t opcode;
        const uint8_t* saved_ptr = ptr;
        WAH_CHECK_GOTO(wah_decode_opcode(&ptr, end, &opcode), cleanup);

        if (opcode == WAH_OP_BLOCK || opcode == WAH_OP_LOOP) {
            int32_t block_type; WAH_CHECK_GOTO(wah_decode_sleb128_32(&ptr, end, &block_type), cleanup);
            WAH_BOUNDS_CHECK_GOTO(control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED, cleanup);
            control_stack[control_sp++] = (wah_control_frame_t){.opcode=(wah_opcode_t)opcode, .target_idx=current_block_idx++};
            continue;
        }
        if (opcode == WAH_OP_END) {
            if (control_sp > 0) { control_sp--; continue; }
        }

        wah_write_u16_le(write_ptr, opcode);
        write_ptr += sizeof(uint16_t);

        switch (opcode) {
            case WAH_OP_IF: {
                int32_t block_type;
                WAH_CHECK_GOTO(wah_decode_sleb128_32(&ptr, end, &block_type), cleanup);
                WAH_BOUNDS_CHECK_GOTO(control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED, cleanup);
                control_stack[control_sp++] = (wah_control_frame_t){.opcode=WAH_OP_IF, .target_idx=current_block_idx};
                wah_write_u32_le(write_ptr, block_targets[current_block_idx++]);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_ELSE: {
                WAH_BOUNDS_CHECK_GOTO(control_sp > 0 && control_stack[control_sp - 1].opcode == WAH_OP_IF, WAH_ERROR_VALIDATION_FAILED, cleanup);
                control_stack[control_sp - 1] = (wah_control_frame_t){.opcode=WAH_OP_ELSE, .target_idx=current_block_idx};
                wah_write_u32_le(write_ptr, block_targets[current_block_idx++]);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_BR: case WAH_OP_BR_IF: {
                uint32_t relative_depth;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &relative_depth), cleanup);
                WAH_BOUNDS_CHECK_GOTO(relative_depth < control_sp, WAH_ERROR_VALIDATION_FAILED, cleanup);
                wah_control_frame_t* frame = &control_stack[control_sp - 1 - relative_depth];
                wah_write_u32_le(write_ptr, block_targets[frame->target_idx]);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_BR_TABLE: {
                uint32_t num_targets;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &num_targets), cleanup);
                wah_write_u32_le(write_ptr, num_targets);
                write_ptr += sizeof(uint32_t);
                for (uint32_t i = 0; i < num_targets + 1; ++i) {
                    uint32_t relative_depth;
                    WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &relative_depth), cleanup);
                    WAH_BOUNDS_CHECK_GOTO(relative_depth < control_sp, WAH_ERROR_VALIDATION_FAILED, cleanup);
                    wah_control_frame_t* frame = &control_stack[control_sp - 1 - relative_depth];
                    wah_write_u32_le(write_ptr, block_targets[frame->target_idx]);
                    write_ptr += sizeof(uint32_t);
                }
                break;
            }
            case WAH_OP_LOCAL_GET: case WAH_OP_LOCAL_SET: case WAH_OP_LOCAL_TEE:
            case WAH_OP_GLOBAL_GET: case WAH_OP_GLOBAL_SET: case WAH_OP_CALL: {
                uint32_t v;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &v), cleanup);
                wah_write_u32_le(write_ptr, v);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_CALL_INDIRECT: {
                uint32_t t, i;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &t), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &i), cleanup);
                wah_write_u32_le(write_ptr, t);
                write_ptr += sizeof(uint32_t);
                wah_write_u32_le(write_ptr, i);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_I32_CONST: {
                int32_t v;
                WAH_CHECK_GOTO(wah_decode_sleb128_32(&ptr, end, &v), cleanup);
                wah_write_u32_le(write_ptr, (uint32_t)v);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_I64_CONST: {
                int64_t v;
                WAH_CHECK_GOTO(wah_decode_sleb128_64(&ptr, end, &v), cleanup);
                wah_write_u64_le(write_ptr, (uint64_t)v);
                write_ptr += sizeof(uint64_t);
                break;
            }
            case WAH_OP_F32_CONST: memcpy(write_ptr, ptr, 4); ptr += 4; write_ptr += 4; break;
            case WAH_OP_F64_CONST: memcpy(write_ptr, ptr, 8); ptr += 8; write_ptr += 8; break;
            case WAH_OP_I32_LOAD: case WAH_OP_I64_LOAD: case WAH_OP_F32_LOAD: case WAH_OP_F64_LOAD:
            case WAH_OP_I32_LOAD8_S: case WAH_OP_I32_LOAD8_U: case WAH_OP_I32_LOAD16_S: case WAH_OP_I32_LOAD16_U:
            case WAH_OP_I64_LOAD8_S: case WAH_OP_I64_LOAD8_U: case WAH_OP_I64_LOAD16_S: case WAH_OP_I64_LOAD16_U:
            case WAH_OP_I64_LOAD32_S: case WAH_OP_I64_LOAD32_U:
            case WAH_OP_I32_STORE: case WAH_OP_I64_STORE: case WAH_OP_F32_STORE: case WAH_OP_F64_STORE:
            case WAH_OP_I32_STORE8: case WAH_OP_I32_STORE16: case WAH_OP_I64_STORE8: case WAH_OP_I64_STORE16: case WAH_OP_I64_STORE32: {
                uint32_t a, o;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &a), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &o), cleanup);
                wah_write_u32_le(write_ptr, o);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_MEMORY_SIZE: case WAH_OP_MEMORY_GROW: case WAH_OP_MEMORY_FILL: {
                uint32_t m;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &m), cleanup);
                break;
            }
        }
        ptr = saved_ptr + (ptr - saved_ptr);
    }

cleanup:
    free(block_targets);
    if (err != WAH_OK && parsed_code->bytecode) {
        free(parsed_code->bytecode);
        parsed_code->bytecode = NULL;
        parsed_code->bytecode_size = 0;
    }
    return err;
}

static void wah_free_parsed_code(wah_parsed_code_t *parsed_code) {
    if (!parsed_code) return;
    free(parsed_code->bytecode);
}

static wah_error_t wah_decode_opcode(const uint8_t **ptr, const uint8_t *end, uint16_t *opcode_val) {
    if (*ptr >= end) {
        return WAH_ERROR_UNEXPECTED_EOF;
    }
    uint8_t first_byte = *(*ptr)++;

    if (first_byte > 0xF0) { // Multi-byte opcode prefix is remapped: F1 23 -> 1023, FC DEF -> CDEF
        uint32_t sub_opcode_val;
        WAH_CHECK(wah_decode_uleb128(ptr, end, &sub_opcode_val));
        if (sub_opcode_val > 0xFFF) return WAH_ERROR_VALIDATION_FAILED; // Sub-opcode out of expected range
        *opcode_val = (uint16_t)(((first_byte & 0x0F) << 12) | sub_opcode_val);
    } else {
        *opcode_val = (uint16_t)first_byte;
    }
    return WAH_OK;
}

wah_error_t wah_parse_module(const uint8_t *wasm_binary, size_t binary_size, wah_module_t *module) {
    wah_error_t err = WAH_OK;
    if (!wasm_binary || !module || binary_size < 8) {
        return WAH_ERROR_UNEXPECTED_EOF; // Or a more specific error
    }

    memset(module, 0, sizeof(wah_module_t)); // Initialize module struct

    const uint8_t *ptr = wasm_binary;
    const uint8_t *end = wasm_binary + binary_size;

    // For section order validation
    int8_t last_parsed_order = 0; // Start with 0, as Type section is 1. Custom sections are 0 in map.

    // 1. Check Magic Number
    uint32_t magic = wah_read_u32_le(ptr);
    ptr += 4;
    if (magic != 0x6D736100) {
        return WAH_ERROR_INVALID_MAGIC_NUMBER;
    }

    // 2. Check Version
    if (ptr + 4 > end) {
        return WAH_ERROR_UNEXPECTED_EOF;
    }
    uint32_t version = wah_read_u32_le(ptr);
    ptr += 4;
    if (version != 0x01) {
        return WAH_ERROR_INVALID_VERSION;
    }

    // 3. Parse Sections
    while (ptr < end) {
        wah_section_id_t section_id;
        uint32_t section_size;
        WAH_CHECK_GOTO(wah_read_section_header(&ptr, end, &section_id, &section_size), cleanup_parse);

        // Get section handler from lookup table
        if (section_id >= sizeof(wah_section_handlers) / sizeof(*wah_section_handlers)) {
            err = WAH_ERROR_UNKNOWN_SECTION; // Unknown section ID
            goto cleanup_parse;
        }
        const struct wah_section_handler_s *handler = &wah_section_handlers[section_id];

        // Section order validation
        if (section_id != WAH_SECTION_CUSTOM) { // Custom sections do not affect the order
            if (handler->order < last_parsed_order) {
                err = WAH_ERROR_VALIDATION_FAILED; // Invalid section order
                goto cleanup_parse;
            }
            last_parsed_order = handler->order;
        }

        const uint8_t *section_payload_end = ptr + section_size;
        if (section_payload_end > end) {
            err = WAH_ERROR_UNEXPECTED_EOF;
            goto cleanup_parse;
        }

        WAH_LOG("Parsing section ID: %d, size: %u", section_id, section_size);
        WAH_CHECK_GOTO(handler->parser_func(&ptr, section_payload_end, module), cleanup_parse);

        // Ensure we consumed exactly the section_size bytes
        if (ptr != section_payload_end) {
            err = WAH_ERROR_VALIDATION_FAILED; // Indicate a parsing error within the section
            goto cleanup_parse;
        }
    }

    return WAH_OK;

cleanup_parse:
    wah_free_module(module);
    return err;
}

// --- Interpreter Implementation ---

wah_error_t wah_exec_context_create(wah_exec_context_t *exec_ctx, const wah_module_t *module) {
    memset(exec_ctx, 0, sizeof(wah_exec_context_t));
    wah_error_t err = WAH_OK;

    exec_ctx->value_stack_capacity = WAH_DEFAULT_VALUE_STACK_SIZE;
    WAH_MALLOC_ARRAY_GOTO(exec_ctx->value_stack, exec_ctx->value_stack_capacity, cleanup);

    exec_ctx->call_stack_capacity = WAH_DEFAULT_MAX_CALL_DEPTH;
    WAH_MALLOC_ARRAY_GOTO(exec_ctx->call_stack, exec_ctx->call_stack_capacity, cleanup);

    if (module->global_count > 0) {
        WAH_MALLOC_ARRAY_GOTO(exec_ctx->globals, module->global_count, cleanup);
        // Initialize globals from the module definition
        for (uint32_t i = 0; i < module->global_count; ++i) {
            exec_ctx->globals[i] = module->globals[i].initial_value;
        }
    }

    exec_ctx->module = module;
    exec_ctx->global_count = module->global_count;
    exec_ctx->max_call_depth = WAH_DEFAULT_MAX_CALL_DEPTH;
    exec_ctx->sp = 0;
    exec_ctx->call_depth = 0;

    if (module->memory_count > 0) {
        exec_ctx->memory_size = module->memories[0].min_pages * WAH_WASM_PAGE_SIZE;
        WAH_MALLOC_ARRAY_GOTO(exec_ctx->memory_base, exec_ctx->memory_size, cleanup);
        memset(exec_ctx->memory_base, 0, exec_ctx->memory_size);
    }

    if (module->table_count > 0) {
        WAH_MALLOC_ARRAY_GOTO(exec_ctx->tables, module->table_count, cleanup);
        memset(exec_ctx->tables, 0, sizeof(wah_value_t*) * module->table_count);

        exec_ctx->table_count = module->table_count;
        for (uint32_t i = 0; i < exec_ctx->table_count; ++i) {
            uint32_t min_elements = module->tables[i].min_elements;
            WAH_MALLOC_ARRAY_GOTO(exec_ctx->tables[i], min_elements, cleanup);
            memset(exec_ctx->tables[i], 0, sizeof(wah_value_t) * min_elements); // Initialize to null (0)
        }

        // Initialize tables with element segments
        for (uint32_t i = 0; i < module->element_segment_count; ++i) {
            const wah_element_segment_t *segment = &module->element_segments[i];
            
            // Validation should be done at parse time. Assert here as a safety net.
            assert(segment->table_idx < exec_ctx->table_count);
            assert((uint64_t)segment->offset + segment->num_elems <= module->tables[segment->table_idx].min_elements);

            for (uint32_t j = 0; j < segment->num_elems; ++j) {
                exec_ctx->tables[segment->table_idx][segment->offset + j].i32 = (int32_t)segment->func_indices[j];
            }
        }
    }

    return WAH_OK;

cleanup:
    if (err != WAH_OK) {
        if (exec_ctx->tables) {
            for (uint32_t i = 0; i < exec_ctx->table_count; ++i) {
                free(exec_ctx->tables[i]);
            }
            free(exec_ctx->tables);
        }
        free(exec_ctx->memory_base);
        free(exec_ctx->globals);
        free(exec_ctx->call_stack);
        free(exec_ctx->value_stack);
        memset(exec_ctx, 0, sizeof(wah_exec_context_t));
    }
    return err;
}

void wah_exec_context_destroy(wah_exec_context_t *exec_ctx) {
    if (!exec_ctx) return;
    free(exec_ctx->value_stack);
    free(exec_ctx->call_stack);
    free(exec_ctx->globals);
    free(exec_ctx->memory_base);
    if (exec_ctx->tables) {
        for (uint32_t i = 0; i < exec_ctx->table_count; ++i) {
            free(exec_ctx->tables[i]);
        }
        free(exec_ctx->tables);
    }
    memset(exec_ctx, 0, sizeof(wah_exec_context_t));
}

// Pushes a new call frame. This is an internal helper.
static wah_error_t wah_push_frame(wah_exec_context_t *ctx, uint32_t func_idx, uint32_t locals_offset) {
    if (ctx->call_depth >= ctx->max_call_depth) {
        return WAH_ERROR_CALL_STACK_OVERFLOW;
    }

    const wah_code_body_t *code_body = &ctx->module->code_bodies[func_idx];
    wah_call_frame_t *frame = &ctx->call_stack[ctx->call_depth++];

    frame->code = code_body;
    frame->bytecode_ip = code_body->parsed_code.bytecode;
    frame->locals_offset = locals_offset;
    frame->func_idx = func_idx;

    return WAH_OK;
}

#define RELOAD_FRAME() \
    do { \
        frame = &ctx->call_stack[ctx->call_depth - 1]; \
        bytecode_ip = frame->bytecode_ip; \
        bytecode_base = frame->code->parsed_code.bytecode; \
    } while(0)

static wah_error_t wah_run_interpreter(wah_exec_context_t *ctx) {
    wah_error_t err = WAH_OK;

    // These are pointers to the current frame's state for faster access.
    wah_call_frame_t *frame;
    const uint8_t *bytecode_ip;
    const uint8_t *bytecode_base;

    RELOAD_FRAME(); // Initial frame load

    while (ctx->call_depth > 0) { // Loop while there are active call frames
        uint16_t opcode = wah_read_u16_le(bytecode_ip);
        bytecode_ip += sizeof(uint16_t);

        switch (opcode) {
            case WAH_OP_BLOCK: // Should not appear in preparsed code
            case WAH_OP_LOOP:  // Should not appear in preparsed code
                err = WAH_ERROR_VALIDATION_FAILED;
                goto cleanup;

            case WAH_OP_IF: {
                uint32_t offset = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                if (ctx->value_stack[--ctx->sp].i32 == 0) {
                    bytecode_ip = bytecode_base + offset;
                }
                break;
            }
            case WAH_OP_ELSE: { // This is an unconditional jump
                uint32_t offset = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                bytecode_ip = bytecode_base + offset;
                break;
            }
            case WAH_OP_BR: {
                uint32_t offset = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                bytecode_ip = bytecode_base + offset;
                break;
            }
            case WAH_OP_BR_IF: {
                uint32_t offset = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                if (ctx->value_stack[--ctx->sp].i32 != 0) {
                    bytecode_ip = bytecode_base + offset;
                }
                break;
            }
            case WAH_OP_BR_TABLE: {
                uint32_t index = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t num_targets = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);

                uint32_t target_offset;
                if (index < num_targets) {
                    // Jump to the specified target
                    target_offset = wah_read_u32_le(bytecode_ip + index * sizeof(uint32_t));
                } else {
                    // Jump to the default target (the last one in the list)
                    target_offset = wah_read_u32_le(bytecode_ip + num_targets * sizeof(uint32_t));
                }
                bytecode_ip = bytecode_base + target_offset;
                break;
            }
            case WAH_OP_I32_CONST: {
                ctx->value_stack[ctx->sp++].i32 = (int32_t)wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                break;
            }
            case WAH_OP_I64_CONST: {
                ctx->value_stack[ctx->sp++].i64 = (int64_t)wah_read_u64_le(bytecode_ip);
                bytecode_ip += sizeof(uint64_t);
                break;
            }
            case WAH_OP_F32_CONST: {
                ctx->value_stack[ctx->sp++].f32 = wah_canonicalize_f32(wah_read_f32_le(bytecode_ip));
                bytecode_ip += sizeof(float);
                break;
            }
            case WAH_OP_F64_CONST: {
                ctx->value_stack[ctx->sp++].f64 = wah_canonicalize_f64(wah_read_f64_le(bytecode_ip));
                bytecode_ip += sizeof(double);
                break;
            }
            case WAH_OP_LOCAL_GET: {
                uint32_t local_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                ctx->value_stack[ctx->sp++] = ctx->value_stack[frame->locals_offset + local_idx];
                break;
            }
            case WAH_OP_LOCAL_SET: {
                uint32_t local_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                ctx->value_stack[frame->locals_offset + local_idx] = ctx->value_stack[--ctx->sp];
                break;
            }
            case WAH_OP_LOCAL_TEE: {
                uint32_t local_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                wah_value_t val = ctx->value_stack[ctx->sp - 1];
                ctx->value_stack[frame->locals_offset + local_idx] = val;
                break;
            }
            case WAH_OP_GLOBAL_GET: {
                uint32_t global_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                ctx->value_stack[ctx->sp++] = ctx->globals[global_idx];
                break;
            }
            case WAH_OP_GLOBAL_SET: {
                uint32_t global_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                ctx->globals[global_idx] = ctx->value_stack[--ctx->sp];
                break;
            }
            case WAH_OP_CALL: {
                uint32_t called_func_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                const wah_func_type_t *called_func_type = &ctx->module->types[ctx->module->function_type_indices[called_func_idx]];
                const wah_code_body_t *called_code = &ctx->module->code_bodies[called_func_idx];

                uint32_t new_locals_offset = ctx->sp - called_func_type->param_count;
                
                frame->bytecode_ip = bytecode_ip;

                err = wah_push_frame(ctx, called_func_idx, new_locals_offset);
                if (err != WAH_OK) goto cleanup;

                uint32_t num_locals = called_code->local_count;
                if (num_locals > 0) {
                    if (ctx->sp + num_locals > ctx->value_stack_capacity) {
                        err = WAH_ERROR_TOO_LARGE;
                        goto cleanup;
                    }
                    memset(&ctx->value_stack[ctx->sp], 0, sizeof(wah_value_t) * num_locals);
                    ctx->sp += num_locals;
                }

                RELOAD_FRAME();
                break;
            }
            case WAH_OP_CALL_INDIRECT: {
                uint32_t type_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                uint32_t table_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);

                // Pop function index from stack
                uint32_t func_table_idx = (uint32_t)ctx->value_stack[--ctx->sp].i32;

                // Validate table_idx
                if (table_idx >= ctx->table_count) {
                    err = WAH_ERROR_TRAP; // Table index out of bounds
                    goto cleanup;
                }

                // Validate func_table_idx against table size
                if (func_table_idx >= ctx->module->tables[table_idx].min_elements) { // Use min_elements as current size
                    err = WAH_ERROR_TRAP; // Function index out of table bounds
                    goto cleanup;
                }

                // Get the actual function index from the table
                uint32_t actual_func_idx = (uint32_t)ctx->tables[table_idx][func_table_idx].i32;

                // Validate actual_func_idx against module's function count
                if (actual_func_idx >= ctx->module->function_count) {
                    err = WAH_ERROR_TRAP; // Invalid function index in table
                    goto cleanup;
                }

                // Get expected function type (from instruction)
                const wah_func_type_t *expected_func_type = &ctx->module->types[type_idx];
                // Get actual function type (from module's function type indices) (function_type_indices stores type_idx for each function)
                const wah_func_type_t *actual_func_type = &ctx->module->types[ctx->module->function_type_indices[actual_func_idx]];

                // Type check: compare expected and actual function types
                if (expected_func_type->param_count != actual_func_type->param_count ||
                    expected_func_type->result_count != actual_func_type->result_count) {
                    err = WAH_ERROR_TRAP; // Type mismatch (param/result count)
                    goto cleanup;
                }
                for (uint32_t i = 0; i < expected_func_type->param_count; ++i) {
                    if (expected_func_type->param_types[i] != actual_func_type->param_types[i]) {
                        err = WAH_ERROR_TRAP; // Type mismatch (param type)
                        goto cleanup;
                    }
                }
                for (uint32_t i = 0; i < expected_func_type->result_count; ++i) {
                    if (expected_func_type->result_types[i] != actual_func_type->result_types[i]) {
                        err = WAH_ERROR_TRAP; // Type mismatch (result type)
                        goto cleanup;
                    }
                }
                // Perform the call using actual_func_idx
                const wah_code_body_t *called_code = &ctx->module->code_bodies[actual_func_idx];
                uint32_t new_locals_offset = ctx->sp - expected_func_type->param_count; // Use expected_func_type for stack manipulation

                frame->bytecode_ip = bytecode_ip;

                err = wah_push_frame(ctx, actual_func_idx, new_locals_offset);
                if (err != WAH_OK) goto cleanup;

                uint32_t num_locals = called_code->local_count;
                if (num_locals > 0) {
                    if (ctx->sp + num_locals > ctx->value_stack_capacity) {
                        err = WAH_ERROR_TOO_LARGE;
                        goto cleanup;
                    }
                    memset(&ctx->value_stack[ctx->sp], 0, sizeof(wah_value_t) * num_locals);
                    ctx->sp += num_locals;
                }

                RELOAD_FRAME();
                break;
            }
            case WAH_OP_RETURN: {
                const wah_func_type_t *func_type = &ctx->module->types[ctx->module->function_type_indices[frame->func_idx]];
                uint32_t results_to_keep = func_type->result_count;
                wah_value_t result_val;
                if (results_to_keep == 1) {
                    result_val = ctx->value_stack[ctx->sp - 1];
                }

                ctx->sp = frame->locals_offset;
                ctx->call_depth--;

                if (results_to_keep == 1) {
                    ctx->value_stack[ctx->sp++] = result_val;
                }

                if (ctx->call_depth > 0) {
                    RELOAD_FRAME();
                }
                break;
            }
            case WAH_OP_END: { // End of function
                const wah_func_type_t *func_type = &ctx->module->types[ctx->module->function_type_indices[frame->func_idx]];
                uint32_t results_to_keep = func_type->result_count;
                wah_value_t result_val;
                if (results_to_keep == 1) {
                    if (ctx->sp > frame->locals_offset) {
                         result_val = ctx->value_stack[ctx->sp - 1];
                    } else {
                        results_to_keep = 0;
                    }
                }

                ctx->sp = frame->locals_offset;
                ctx->call_depth--;

                if (results_to_keep == 1) {
                    ctx->value_stack[ctx->sp++] = result_val;
                }

                if (ctx->call_depth > 0) {
                    RELOAD_FRAME();
                }
                break;
            }

            #define VSTACK_TOP (ctx->value_stack[ctx->sp - 1])
            #define VSTACK_B (ctx->value_stack[ctx->sp - 1])
            #define VSTACK_A (ctx->value_stack[ctx->sp - 2])
            #define BINOP_I(N,op) { VSTACK_A.i##N = (int##N##_t)((uint##N##_t)VSTACK_A.i##N op (uint##N##_t)VSTACK_B.i##N); ctx->sp--; break; }
            #define CMP_I_S(N,op) { VSTACK_A.i32 = VSTACK_A.i##N op VSTACK_B.i##N ? 1 : 0; ctx->sp--; break; }
            #define CMP_I_U(N,op) { VSTACK_A.i32 = (uint##N##_t)VSTACK_A.i##N op (uint##N##_t)VSTACK_B.i##N ? 1 : 0; ctx->sp--; break; }
            #define BINOP_F(N,op) { VSTACK_A.f##N = VSTACK_A.f##N op VSTACK_B.f##N; ctx->sp--; break; }
            #define CMP_F(N,op)   { VSTACK_A.i32 = VSTACK_A.f##N op VSTACK_B.f##N ? 1 : 0; ctx->sp--; break; }
            #define UNOP_I_FN(N,fn)  { VSTACK_TOP.i##N = (int##N##_t)fn((uint##N##_t)VSTACK_TOP.i##N); break; }
            #define BINOP_I_FN(N,fn) { VSTACK_A.i##N = (int##N##_t)fn((uint##N##_t)VSTACK_A.i##N, (uint##N##_t)VSTACK_B.i##N); ctx->sp--; break; }
            #define UNOP_F_FN(N,fn)  { VSTACK_TOP.f##N = fn(VSTACK_TOP.f##N); break; }
            #define BINOP_F_FN(N,fn) { VSTACK_A.f##N = fn(VSTACK_A.f##N, VSTACK_B.f##N); ctx->sp--; break; }

#define NUM_OPS(N,F) \
            case WAH_OP_I##N##_CLZ: UNOP_I_FN(N, wah_clz_u##N) \
            case WAH_OP_I##N##_CTZ: UNOP_I_FN(N, wah_ctz_u##N) \
            case WAH_OP_I##N##_POPCNT: UNOP_I_FN(N, wah_popcount_u##N) \
            case WAH_OP_I##N##_ADD: BINOP_I(N,+) \
            case WAH_OP_I##N##_SUB: BINOP_I(N,-) \
            case WAH_OP_I##N##_MUL: BINOP_I(N,*) \
            case WAH_OP_I##N##_DIV_S: {  \
                if (VSTACK_B.i##N == 0) { err = WAH_ERROR_TRAP; goto cleanup; } \
                if (VSTACK_A.i##N == INT##N##_MIN && VSTACK_B.i##N == -1) { err = WAH_ERROR_TRAP; goto cleanup; } \
                VSTACK_A.i##N /= VSTACK_B.i##N; ctx->sp--; break;  \
            } \
            case WAH_OP_I##N##_DIV_U: {  \
                if (VSTACK_B.i##N == 0) { err = WAH_ERROR_TRAP; goto cleanup; } \
                VSTACK_A.i##N = (int##N##_t)((uint##N##_t)VSTACK_A.i##N / (uint##N##_t)VSTACK_B.i##N); ctx->sp--; break; \
            } \
            case WAH_OP_I##N##_REM_S: {  \
                if (VSTACK_B.i##N == 0) { err = WAH_ERROR_TRAP; goto cleanup; } \
                if (VSTACK_A.i##N == INT##N##_MIN && VSTACK_B.i##N == -1) { VSTACK_A.i##N = 0; } else { VSTACK_A.i##N %= VSTACK_B.i##N; } \
                ctx->sp--; break;  \
            } \
            case WAH_OP_I##N##_REM_U: {  \
                if (VSTACK_B.i##N == 0) { err = WAH_ERROR_TRAP; goto cleanup; } \
                VSTACK_A.i##N = (int##N##_t)((uint##N##_t)VSTACK_A.i##N % (uint##N##_t)VSTACK_B.i##N); ctx->sp--; break; \
            } \
            case WAH_OP_I##N##_AND: BINOP_I(N,&) \
            case WAH_OP_I##N##_OR:  BINOP_I(N,|) \
            case WAH_OP_I##N##_XOR: BINOP_I(N,^) \
            case WAH_OP_I##N##_SHL: { VSTACK_A.i##N = (int##N##_t)((uint##N##_t)VSTACK_A.i##N << (VSTACK_B.i##N & (N-1))); ctx->sp--; break; } \
            case WAH_OP_I##N##_SHR_S: { VSTACK_A.i##N >>= (VSTACK_B.i##N & (N-1)); ctx->sp--; break; } \
            case WAH_OP_I##N##_SHR_U: { VSTACK_A.i##N = (int##N##_t)((uint##N##_t)VSTACK_A.i##N >> (VSTACK_B.i##N & (N-1))); ctx->sp--; break; } \
            case WAH_OP_I##N##_ROTL: BINOP_I_FN(N, wah_rotl_u##N) \
            case WAH_OP_I##N##_ROTR: BINOP_I_FN(N, wah_rotr_u##N) \
            \
            case WAH_OP_I##N##_EQ:   CMP_I_S(N,==) \
            case WAH_OP_I##N##_NE:   CMP_I_S(N,!=) \
            case WAH_OP_I##N##_LT_S: CMP_I_S(N,<) \
            case WAH_OP_I##N##_LT_U: CMP_I_U(N,<) \
            case WAH_OP_I##N##_GT_S: CMP_I_S(N,>) \
            case WAH_OP_I##N##_GT_U: CMP_I_U(N,>) \
            case WAH_OP_I##N##_LE_S: CMP_I_S(N,<=) \
            case WAH_OP_I##N##_LE_U: CMP_I_U(N,<=) \
            case WAH_OP_I##N##_GE_S: CMP_I_S(N,>=) \
            case WAH_OP_I##N##_GE_U: CMP_I_U(N,>=) \
            case WAH_OP_I##N##_EQZ:  { VSTACK_A.i32 = (VSTACK_A.i##N == 0) ? 1 : 0; break; } \
            \
            case WAH_OP_F##N##_ABS: UNOP_F_FN(N, fabs##F) \
            case WAH_OP_F##N##_NEG: UNOP_F_FN(N, -) \
            case WAH_OP_F##N##_CEIL: UNOP_F_FN(N, ceil##F) \
            case WAH_OP_F##N##_FLOOR: UNOP_F_FN(N, floor##F) \
            case WAH_OP_F##N##_TRUNC: UNOP_F_FN(N, trunc##F) \
            case WAH_OP_F##N##_NEAREST: UNOP_F_FN(N, wah_nearest_f##N) \
            case WAH_OP_F##N##_SQRT: UNOP_F_FN(N, sqrt##F) \
            case WAH_OP_F##N##_ADD: BINOP_F(N,+) \
            case WAH_OP_F##N##_SUB: BINOP_F(N,-) \
            case WAH_OP_F##N##_MUL: BINOP_F(N,*) \
            case WAH_OP_F##N##_DIV: BINOP_F(N,/) /* Let hardware handle division by zero (NaN/inf) */ \
            case WAH_OP_F##N##_EQ: CMP_F(N,==) \
            case WAH_OP_F##N##_NE: CMP_F(N,!=) \
            case WAH_OP_F##N##_LT: CMP_F(N,<) \
            case WAH_OP_F##N##_GT: CMP_F(N,>) \
            case WAH_OP_F##N##_LE: CMP_F(N,<=) \
            case WAH_OP_F##N##_GE: CMP_F(N,>=) \
            case WAH_OP_F##N##_MIN: BINOP_F_FN(N, fmin##F) \
            case WAH_OP_F##N##_MAX: BINOP_F_FN(N, fmax##F) \
            case WAH_OP_F##N##_COPYSIGN: BINOP_F_FN(N, copysign##F)

#define LOAD_OP(N, T, value_field, cast) { \
                uint32_t offset = wah_read_u32_le(bytecode_ip); \
                bytecode_ip += sizeof(uint32_t); \
                uint32_t addr = (uint32_t)ctx->value_stack[--ctx->sp].i32; \
                uint64_t effective_addr = (uint64_t)addr + offset; \
                \
                if (effective_addr >= ctx->memory_size || ctx->memory_size - effective_addr < N/8) { \
                    err = WAH_ERROR_MEMORY_OUT_OF_BOUNDS; \
                    goto cleanup; \
                } \
                ctx->value_stack[ctx->sp++].value_field = cast wah_read_##T##_le(ctx->memory_base + effective_addr); \
                break; \
            }

#define STORE_OP(N, T, value_field, value_type, cast) { \
                uint32_t offset = wah_read_u32_le(bytecode_ip); \
                bytecode_ip += sizeof(uint32_t); \
                value_type val = ctx->value_stack[--ctx->sp].value_field; \
                uint32_t addr = (uint32_t)ctx->value_stack[--ctx->sp].i32; \
                uint64_t effective_addr = (uint64_t)addr + offset; \
                \
                if (effective_addr >= ctx->memory_size || ctx->memory_size - effective_addr < N/8) { \
                    err = WAH_ERROR_MEMORY_OUT_OF_BOUNDS; \
                    goto cleanup; \
                } \
                wah_write_##T##_le(ctx->memory_base + effective_addr, cast (val)); \
                break; \
            }

#define CONVERT(from_field, cast, to_field) { VSTACK_TOP.to_field = cast (VSTACK_TOP.from_field); break; }
#define CONVERT_CHECK(from_field, call, ty, cast, to_field) \
            { ty res; WAH_CHECK(call(VSTACK_TOP.from_field, &res)); VSTACK_TOP.to_field = cast (res); break; }
#define REINTERPRET(from_field, from_ty, to_field, to_ty) \
            { union { from_ty from; to_ty to; } u = { .from = VSTACK_TOP.from_field }; VSTACK_TOP.to_field = u.to; break; }

            NUM_OPS(32,f)
            NUM_OPS(64,)

            case WAH_OP_I32_LOAD: LOAD_OP(32, u32, i32, (int32_t))
            case WAH_OP_I64_LOAD: LOAD_OP(64, u64, i64, (int64_t))
            case WAH_OP_F32_LOAD: LOAD_OP(32, f32, f32, )
            case WAH_OP_F64_LOAD: LOAD_OP(64, f64, f64, )
            case WAH_OP_I32_LOAD8_S: LOAD_OP(8, u8, i32, (int32_t)(int8_t))
            case WAH_OP_I32_LOAD8_U: LOAD_OP(8, u8, i32, (int32_t))
            case WAH_OP_I32_LOAD16_S: LOAD_OP(16, u16, i32, (int32_t)(int16_t))
            case WAH_OP_I32_LOAD16_U: LOAD_OP(16, u16, i32, (int32_t))
            case WAH_OP_I64_LOAD8_S: LOAD_OP(8, u8, i64, (int64_t)(int8_t))
            case WAH_OP_I64_LOAD8_U: LOAD_OP(8, u8, i64, (int64_t))
            case WAH_OP_I64_LOAD16_S: LOAD_OP(16, u16, i64, (int64_t)(int16_t))
            case WAH_OP_I64_LOAD16_U: LOAD_OP(16, u16, i64, (int64_t))
            case WAH_OP_I64_LOAD32_S: LOAD_OP(32, u32, i64, (int64_t)(int32_t))
            case WAH_OP_I64_LOAD32_U: LOAD_OP(32, u32, i64, (int64_t))

            case WAH_OP_I32_STORE: STORE_OP(32, u32, i32, int32_t, (uint32_t))
            case WAH_OP_I64_STORE: STORE_OP(64, u64, i64, int64_t, (uint64_t))
            case WAH_OP_F32_STORE: STORE_OP(32, f32, f32, float, wah_canonicalize_f32)
            case WAH_OP_F64_STORE: STORE_OP(64, f64, f64, double, wah_canonicalize_f64)
            case WAH_OP_I32_STORE8: STORE_OP(8, u8, i32, int32_t, (uint8_t))
            case WAH_OP_I32_STORE16: STORE_OP(16, u16, i32, int32_t, (uint16_t))
            case WAH_OP_I64_STORE8: STORE_OP(8, u8, i64, int64_t, (uint8_t))
            case WAH_OP_I64_STORE16: STORE_OP(16, u16, i64, int64_t, (uint16_t))
            case WAH_OP_I64_STORE32: STORE_OP(32, u32, i64, int64_t, (uint32_t))

            case WAH_OP_I32_WRAP_I64: CONVERT(i64, (int32_t), i32)
            case WAH_OP_I32_TRUNC_F32_S: CONVERT_CHECK(f32, wah_trunc_f32_to_i32, int32_t, , i32)
            case WAH_OP_I32_TRUNC_F32_U: CONVERT_CHECK(f32, wah_trunc_f32_to_u32, uint32_t, (int32_t), i32)
            case WAH_OP_I32_TRUNC_F64_S: CONVERT_CHECK(f64, wah_trunc_f64_to_i32, int32_t, , i32)
            case WAH_OP_I32_TRUNC_F64_U: CONVERT_CHECK(f64, wah_trunc_f64_to_u32, uint32_t, (int32_t), i32)

            case WAH_OP_I64_EXTEND_I32_S: CONVERT(i32, (int64_t), i64)
            case WAH_OP_I64_EXTEND_I32_U: CONVERT(i32, (int64_t)(uint32_t), i64)
            case WAH_OP_I64_TRUNC_F32_S: CONVERT_CHECK(f32, wah_trunc_f32_to_i64, int64_t, , i64)
            case WAH_OP_I64_TRUNC_F32_U: CONVERT_CHECK(f32, wah_trunc_f32_to_u64, uint64_t, (int64_t), i64)
            case WAH_OP_I64_TRUNC_F64_S: CONVERT_CHECK(f64, wah_trunc_f64_to_i64, int64_t, , i64)
            case WAH_OP_I64_TRUNC_F64_U: CONVERT_CHECK(f64, wah_trunc_f64_to_u64, uint64_t, (int64_t), i64)

            case WAH_OP_F32_CONVERT_I32_S: CONVERT(i32, (float), f32)
            case WAH_OP_F32_CONVERT_I32_U: CONVERT(i32, (float)(uint32_t), f32)
            case WAH_OP_F32_CONVERT_I64_S: CONVERT(i64, (float), f32)
            case WAH_OP_F32_CONVERT_I64_U: CONVERT(i64, (float)(uint64_t), f32)
            case WAH_OP_F32_DEMOTE_F64: CONVERT(f64, (float), f32)

            case WAH_OP_F64_CONVERT_I32_S: CONVERT(i32, (double), f64)
            case WAH_OP_F64_CONVERT_I32_U: CONVERT(i32, (double)(uint32_t), f64)
            case WAH_OP_F64_CONVERT_I64_S: CONVERT(i64, (double), f64)
            case WAH_OP_F64_CONVERT_I64_U: CONVERT(i64, (double)(uint64_t), f64)
            case WAH_OP_F64_PROMOTE_F32: CONVERT(f32, (double), f64)

            case WAH_OP_I32_REINTERPRET_F32: REINTERPRET(f32, float, i32, int32_t)
            case WAH_OP_I64_REINTERPRET_F64: REINTERPRET(f64, double, i64, int64_t)
            case WAH_OP_F32_REINTERPRET_I32: REINTERPRET(i32, int32_t, f32, float)
            case WAH_OP_F64_REINTERPRET_I64: REINTERPRET(i64, int64_t, f64, double)

#undef VSTACK_TOP
#undef VSTACK_B
#undef VSTACK_A
#undef BINOP_I
#undef CMP_I_S
#undef CMP_I_U
#undef BINOP_F
#undef CMP_F
#undef UNOP_I_FN
#undef BINOP_I_FN
#undef UNOP_F_FN
#undef BINOP_F_FN
#undef NUM_OPS
#undef LOAD_OP
#undef STORE_OP
#undef CONVERT
#undef CONVERT_CHECK
#undef REINTERPRET

            case WAH_OP_MEMORY_SIZE: {
                // memory index (always 0x00) is consumed by preparse, no need to read here
                ctx->value_stack[ctx->sp++].i32 = (int32_t)(ctx->memory_size / WAH_WASM_PAGE_SIZE);
                break;
            }
            case WAH_OP_MEMORY_GROW: {
                // memory index (always 0x00) is consumed by preparse, no need to read here
                int32_t pages_to_grow = ctx->value_stack[--ctx->sp].i32;
                if (pages_to_grow < 0) {
                    ctx->value_stack[ctx->sp++].i32 = -1; // Cannot grow by negative pages
                    break;
                }

                uint32_t old_pages = ctx->memory_size / WAH_WASM_PAGE_SIZE;
                uint64_t new_pages = (uint64_t)old_pages + pages_to_grow;

                // Check against max_pages if defined (module->memories[0].max_pages)
                // For now, we assume no max_pages or effectively unlimited if not set
                if (ctx->module->memory_count > 0 && ctx->module->memories[0].max_pages > 0 && new_pages > ctx->module->memories[0].max_pages) {
                    ctx->value_stack[ctx->sp++].i32 = -1; // Exceeds max memory
                    break;
                }

                size_t new_memory_size = (size_t)new_pages * WAH_WASM_PAGE_SIZE;
                WAH_REALLOC_ARRAY_GOTO(ctx->memory_base, new_memory_size, cleanup);

                // Initialize newly allocated memory to zero
                if (new_memory_size > ctx->memory_size) {
                    memset(ctx->memory_base + ctx->memory_size, 0, new_memory_size - ctx->memory_size);
                }

                ctx->memory_size = (uint32_t)new_memory_size;
                ctx->value_stack[ctx->sp++].i32 = (int32_t)old_pages;
                break;
            }
            case WAH_OP_MEMORY_FILL: {
                // memory index (always 0x00) is consumed by preparse, no need to read here
                uint32_t size = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint8_t val = (uint8_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t dst = (uint32_t)ctx->value_stack[--ctx->sp].i32;

                if ((uint64_t)dst + size > ctx->memory_size) {
                    err = WAH_ERROR_MEMORY_OUT_OF_BOUNDS;
                    goto cleanup;
                }
                memset(ctx->memory_base + dst, val, size);
                break;
            }
            case WAH_OP_DROP: { ctx->sp--; break; }
            case WAH_OP_SELECT: {
                wah_value_t c = ctx->value_stack[--ctx->sp];
                wah_value_t b = ctx->value_stack[--ctx->sp];
                wah_value_t a = ctx->value_stack[--ctx->sp];
                ctx->value_stack[ctx->sp++] = c.i32 ? a : b;
                break;
            }
            
            case WAH_OP_NOP: break;
            case WAH_OP_UNREACHABLE: err = WAH_ERROR_TRAP; goto cleanup;
            
            default:
                err = WAH_ERROR_UNKNOWN_SECTION;
                goto cleanup;
        }
    }

    return WAH_OK;

cleanup:
    // Before returning, store the final IP back into the (potentially last) frame
    if (ctx->call_depth > 0) {
        frame->bytecode_ip = bytecode_ip;
    }
    return err;
}

wah_error_t wah_call(wah_exec_context_t *exec_ctx, const wah_module_t *module, uint32_t func_idx, const wah_value_t *params, uint32_t param_count, wah_value_t *result) {
    if (func_idx >= module->function_count) {
        return WAH_ERROR_UNKNOWN_SECTION; 
    }

    const wah_func_type_t *func_type = &module->types[module->function_type_indices[func_idx]];
    if (param_count != func_type->param_count) {
        return WAH_ERROR_VALIDATION_FAILED;
    }

    // Push initial params onto the value stack
    for (uint32_t i = 0; i < param_count; ++i) {
        if (exec_ctx->sp >= exec_ctx->value_stack_capacity) {
            return WAH_ERROR_TOO_LARGE; // Value stack overflow
        }
        exec_ctx->value_stack[exec_ctx->sp++] = params[i];
    }

    // Push the first frame. Locals offset is the current stack pointer before parameters.
    WAH_CHECK(wah_push_frame(exec_ctx, func_idx, exec_ctx->sp - func_type->param_count));
    
    // Reserve space for the function's own locals and initialize them to zero
    uint32_t num_locals = exec_ctx->call_stack[0].code->local_count;
    if (num_locals > 0) {
        if (exec_ctx->sp + num_locals > exec_ctx->value_stack_capacity) {
            // wah_exec_context_destroy(&ctx);
            return WAH_ERROR_OUT_OF_MEMORY; // A more specific stack overflow error would be better
        }
        memset(&exec_ctx->value_stack[exec_ctx->sp], 0, sizeof(wah_value_t) * num_locals);
        exec_ctx->sp += num_locals;
    }

    // Run the main interpreter loop
    WAH_CHECK(wah_run_interpreter(exec_ctx));

    // After execution, if a result is expected, it's on top of the stack.
    if (result && func_type->result_count > 0) {
        // Check if the stack has any value to pop. It might be empty if the function trapped before returning a value.
        if (exec_ctx->sp > 0) {
            *result = exec_ctx->value_stack[exec_ctx->sp - 1];
        }
    }

    return WAH_OK;
}

// --- Module Cleanup Implementation ---
void wah_free_module(wah_module_t *module) {
    if (!module) {
        return;
    }

    if (module->types) {
        for (uint32_t i = 0; i < module->type_count; ++i) {
            free(module->types[i].param_types);
            free(module->types[i].result_types);
        }
        free(module->types);
    }
    free(module->function_type_indices);

    if (module->code_bodies) {
        for (uint32_t i = 0; i < module->code_count; ++i) {
            free(module->code_bodies[i].local_types);
            wah_free_parsed_code(&module->code_bodies[i].parsed_code);
        }
        free(module->code_bodies);
    }

    free(module->globals);
    free(module->memories);
    free(module->tables);

    if (module->element_segments) {
        for (uint32_t i = 0; i < module->element_segment_count; ++i) {
            free(module->element_segments[i].func_indices);
        }
        free(module->element_segments);
    }

    // Reset all fields to 0/NULL
    memset(module, 0, sizeof(wah_module_t));
}

#endif // WAH_IMPLEMENTATION
#endif // WAH_H
