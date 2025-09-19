// WebAssembly interpreter in a Header file (WAH)

#ifndef WAH_H
#define WAH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- Error Codes ---
typedef enum {
    WAH_OK = 0,
    WAH_ERROR_INVALID_MAGIC_NUMBER,
    WAH_ERROR_INVALID_VERSION,
    WAH_ERROR_UNEXPECTED_EOF,
    WAH_ERROR_UNKNOWN_SECTION,
    WAH_ERROR_INVALID_LEB128,
    WAH_ERROR_OUT_OF_MEMORY,
    WAH_ERROR_VALIDATION_FAILED,
    WAH_ERROR_TRAP,  // Runtime trap (division by zero, integer overflow, etc.)
    WAH_ERROR_CALL_STACK_OVERFLOW,
    WAH_ERROR_MEMORY_OUT_OF_BOUNDS, // Memory access out of bounds
    // Add more specific error codes as needed
} wah_error_t;

// --- Memory Structure ---
#define WAH_WASM_PAGE_SIZE 65536 // 64 KB

typedef struct {
    uint32_t min_pages;
    uint32_t max_pages; // For future extension, currently ignored for fixed size
} wah_memory_type_t;

// --- LEB128 Decoding ---
// Helper function to decode an unsigned LEB128 integer
static inline wah_error_t wah_decode_uleb128(const uint8_t **ptr, const uint8_t *end, uint32_t *result) {
    *result = 0;
    uint32_t shift = 0;
    uint8_t byte;

    do {
        if (*ptr >= end) {
            return WAH_ERROR_UNEXPECTED_EOF;
        }
        if (shift >= 32) {
            return WAH_ERROR_INVALID_LEB128;
        }
        byte = *(*ptr)++;
        *result |= (byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    return WAH_OK;
}

// Helper function to decode a signed LEB128 integer (32-bit)
static inline wah_error_t wah_decode_sleb128_32(const uint8_t **ptr, const uint8_t *end, int32_t *result) {
    uint32_t val = 0;
    uint32_t shift = 0;
    uint8_t byte;

    do {
        if (*ptr >= end) return WAH_ERROR_UNEXPECTED_EOF;
        if (shift >= 32) return WAH_ERROR_INVALID_LEB128;
        byte = *(*ptr)++;
        val |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    if (shift < 32) {
        uint32_t sign_bit = 1U << (shift - 1);
        if ((val & sign_bit) != 0) {
            val |= ~0U << shift;
        }
    }
    *result = (int32_t)val;
    return WAH_OK;
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
            return WAH_ERROR_INVALID_LEB128;
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
        if (shift >= 64) return WAH_ERROR_INVALID_LEB128;
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

// Helper to read a float from a byte array in little-endian format
static inline float wah_read_f32_le(const uint8_t *ptr) {
    union {
        uint32_t i;
        float f;
    } u;
    u.i = wah_read_u32_le(ptr);
    return u.f;
}

// Helper to read a double from a byte array in little-endian format
static inline double wah_read_f64_le(const uint8_t *ptr) {
    union {
        uint64_t i;
        double d;
    } u;
    u.i = wah_read_u64_le(ptr);
    return u.d;
}


// --- WebAssembly Types ---
typedef enum {
    WAH_VAL_TYPE_I32 = 0x7F,
    WAH_VAL_TYPE_I64 = 0x7E,
    WAH_VAL_TYPE_F32 = 0x7D,
    WAH_VAL_TYPE_F64 = 0x7C,
    WAH_VAL_TYPE_ANYFUNC = 0x70, // For table elements
    WAH_VAL_TYPE_FUNC = 0x60,    // For function types
    WAH_VAL_TYPE_BLOCK_TYPE = 0x40 // Special type for blocks
} wah_val_type_t;

// --- WebAssembly Opcodes (subset) ---
typedef enum {
    // Control Flow Operators
    WAH_OP_UNREACHABLE = 0x00, WAH_OP_NOP = 0x01, WAH_OP_BLOCK = 0x02, WAH_OP_LOOP = 0x03,
    WAH_OP_IF = 0x04, WAH_OP_ELSE = 0x05,
    WAH_OP_END = 0x0B, WAH_OP_BR = 0x0C, WAH_OP_BR_IF = 0x0D, WAH_OP_BR_TABLE = 0x0E,
    WAH_OP_RETURN = 0x0F, WAH_OP_CALL = 0x10,

    // Parametric Operators
    WAH_OP_DROP = 0x1A,
    WAH_OP_SELECT = 0x1B,

    // Variable Access
    WAH_OP_LOCAL_GET = 0x20, WAH_OP_LOCAL_SET = 0x21, WAH_OP_LOCAL_TEE = 0x22,
    WAH_OP_GLOBAL_GET = 0x23, WAH_OP_GLOBAL_SET = 0x24,

    // Memory Operators
    WAH_OP_I32_LOAD = 0x28,
    WAH_OP_I32_STORE = 0x36,
    WAH_OP_MEMORY_SIZE = 0x3F,
    WAH_OP_MEMORY_GROW = 0x40,
    WAH_OP_MEMORY_FILL = 0xC00B,

    // Constants
    WAH_OP_I32_CONST = 0x41, WAH_OP_I64_CONST = 0x42, WAH_OP_F32_CONST = 0x43, WAH_OP_F64_CONST = 0x44,

    // Numeric Operators
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
    WAH_OP_I32_ADD = 0x6A, WAH_OP_I32_SUB = 0x6B, WAH_OP_I32_MUL = 0x6C,
    WAH_OP_I32_DIV_S = 0x6D, WAH_OP_I32_DIV_U = 0x6E, WAH_OP_I32_REM_S = 0x6F, WAH_OP_I32_REM_U = 0x70,
    WAH_OP_I32_AND = 0x71, WAH_OP_I32_OR = 0x72, WAH_OP_I32_XOR = 0x73,
    WAH_OP_I32_SHL = 0x74, WAH_OP_I32_SHR_S = 0x75, WAH_OP_I32_SHR_U = 0x76,
    WAH_OP_I64_ADD = 0x7C, WAH_OP_I64_SUB = 0x7D, WAH_OP_I64_MUL = 0x7E,
    WAH_OP_I64_DIV_S = 0x7F, WAH_OP_I64_DIV_U = 0x80, WAH_OP_I64_REM_S = 0x81, WAH_OP_I64_REM_U = 0x82,
    WAH_OP_I64_AND = 0x83, WAH_OP_I64_OR = 0x84, WAH_OP_I64_XOR = 0x85,
    WAH_OP_I64_SHL = 0x86, WAH_OP_I64_SHR_S = 0x87, WAH_OP_I64_SHR_U = 0x88,
    WAH_OP_F32_ADD = 0x92, WAH_OP_F32_SUB = 0x93, WAH_OP_F32_MUL = 0x94, WAH_OP_F32_DIV = 0x95,
    WAH_OP_F64_ADD = 0xA0, WAH_OP_F64_SUB = 0xA1, WAH_OP_F64_MUL = 0xA2, WAH_OP_F64_DIV = 0xA3,
} wah_opcode_t;

// --- WebAssembly Miscellaneous Opcodes (multi-byte) ---
typedef enum {
    WAH_MISC_OP_MEMORY_FILL = 0x0B,
    // Add other miscellaneous opcodes as needed
} wah_misc_opcode_t;

// --- WebAssembly Value Representation ---
typedef union {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
} wah_value_t;

// --- Operand Stack ---
typedef struct {
    wah_value_t *data; // Dynamically allocated based on function requirements
    uint32_t sp; // Stack pointer
    uint32_t capacity; // Allocated capacity
} wah_stack_t;

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
    const uint16_t *ip;           // Instruction pointer into the parsed opcodes
    const uint8_t *u8_arg_ptr;
    const uint16_t *u16_arg_ptr;
    const uint32_t *u32_arg_ptr;
    const uint64_t *u64_arg_ptr;
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
    WAH_SECTION_DATACOUNT = 12, // Proposed, not in all versions
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
    uint32_t instruction_count;
    uint16_t *opcodes;           // Array of opcodes
    uint8_t *u8_args;           // Array of uint8 arguments
    uint16_t *u16_args;         // Array of uint16 arguments  
    uint32_t *u32_args;         // Array of uint32 arguments (can be cast to int32_t)
    uint64_t *u64_args;         // Array of uint64 arguments (can be cast to int64_t or double)
    
    // Indices for argument arrays (pointing to next available slot)
    uint32_t u8_arg_count;
    uint32_t u16_arg_count;
    uint32_t u32_arg_count;
    uint32_t u64_arg_count;
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
    // Type Section
    uint32_t type_count;
    wah_func_type_t *types;

    // Function Section
    uint32_t function_count;
    uint32_t *function_type_indices; // Index into the types array

    // Code Section
    uint32_t code_count;
    wah_code_body_t *code_bodies;

    // Global Section
    uint32_t global_count;
    wah_global_t *globals;

    // Memory Section
    uint32_t memory_count;
    wah_memory_type_t *memories;

    // Add other sections as they are implemented
} wah_module_t;

// --- Validation Context ---
typedef struct {
    wah_type_stack_t type_stack;
    const wah_func_type_t *func_type; // Type of the function being validated
    const wah_module_t *module; // Reference to the module for global/function lookups
    uint32_t total_locals; // Total number of locals (params + declared locals)
    uint32_t current_stack_depth; // Current stack depth during validation
    uint32_t max_stack_depth; // Maximum stack depth seen during validation
} wah_validation_context_t;

// --- Parser Functions ---
wah_error_t wah_parse_module(const uint8_t *wasm_binary, size_t binary_size, wah_module_t *module);

// Internal section parsing functions
static wah_error_t wah_parse_type_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_function_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_code_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_global_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
static wah_error_t wah_parse_memory_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);

// --- Interpreter Functions ---
// Creates and initializes an execution context.
wah_error_t wah_exec_context_create(wah_exec_context_t *exec_ctx, const wah_module_t *module);

// Destroys and frees resources of an execution context.
void wah_exec_context_destroy(wah_exec_context_t *exec_ctx);

// The main entry point to call a WebAssembly function.
wah_error_t wah_call(wah_exec_context_t *exec_ctx, const wah_module_t *module, uint32_t func_idx, const wah_value_t *params, uint32_t param_count, wah_value_t *result);

// The core interpreter loop (internal).
static wah_error_t wah_run_interpreter(wah_exec_context_t *exec_ctx);

// Validation helper functions
static wah_error_t wah_validate_opcode(uint16_t opcode_val, const uint8_t **code_ptr, const uint8_t *code_end, wah_validation_context_t *vctx);

// Pre-parsing functions
static wah_error_t wah_preparse_code(const uint8_t *code, uint32_t code_size, wah_parsed_code_t *parsed_code);
static void wah_free_parsed_code(wah_parsed_code_t *parsed_code);

// --- Module Cleanup ---
void wah_free_module(wah_module_t *module);

#ifdef WAH_IMPLEMENTATION

#include <string.h> // For memcpy, memset
#include <stdlib.h> // For malloc, free
#include <assert.h> // For assert
#include <stdint.h> // For INT32_MIN, INT32_MAX

// --- Helper Macros ---
#define WAH_CHECK(expr) do { \
    wah_error_t _err = (expr); \
    if (_err != WAH_OK) return _err; \
} while(0)

#define WAH_CHECK_GOTO(expr, label) do { \
    err = (expr); \
    if (err != WAH_OK) goto label; \
} while(0)

#define WAH_BOUNDS_CHECK(cond, error) do { \
    if (!(cond)) return (error); \
} while(0)

#define WAH_BOUNDS_CHECK_GOTO(cond, error, label) do { \
    if (!(cond)) { err = (error); goto label; } \
} while(0)



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
    WAH_CHECK(wah_type_stack_push(&vctx->type_stack, type));
    vctx->current_stack_depth++;
    if (vctx->current_stack_depth > vctx->max_stack_depth) {
        vctx->max_stack_depth = vctx->current_stack_depth;
    }
    return WAH_OK;
}

static inline wah_error_t wah_validation_pop_type(wah_validation_context_t *vctx, wah_val_type_t *out_type) {
    // This validates that we have something to pop
    WAH_CHECK(wah_type_stack_pop(&vctx->type_stack, out_type));
    vctx->current_stack_depth--;
    return WAH_OK;
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
    module->types = (wah_func_type_t*)malloc(sizeof(wah_func_type_t) * count);
    if (!module->types) return WAH_ERROR_OUT_OF_MEMORY;
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
        module->types[i].param_types = (wah_val_type_t*)malloc(sizeof(wah_val_type_t) * param_count_type);
        if (!module->types[i].param_types) return WAH_ERROR_OUT_OF_MEMORY;
        
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
        module->types[i].result_types = (wah_val_type_t*)malloc(sizeof(wah_val_type_t) * result_count_type);
        if (!module->types[i].result_types) return WAH_ERROR_OUT_OF_MEMORY;
        
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
    module->function_type_indices = (uint32_t*)malloc(sizeof(uint32_t) * count);
    if (!module->function_type_indices) return WAH_ERROR_OUT_OF_MEMORY;

    for (uint32_t i = 0; i < count; ++i) {
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->function_type_indices[i]));
        
        if (module->function_type_indices[i] >= module->type_count) {
            return WAH_ERROR_VALIDATION_FAILED;
        }
    }
    return WAH_OK;
}

// Validation helper function that handles a single opcode
static wah_error_t wah_validate_opcode(uint16_t opcode_val, const uint8_t **code_ptr, const uint8_t *code_end, wah_validation_context_t *vctx) {
    wah_error_t err = WAH_OK;
    
    switch (opcode_val) {
        case WAH_OP_I32_LOAD: {
            uint32_t align, offset;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &align));
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &offset));
            wah_val_type_t addr_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &addr_type));
            if (addr_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32);
        }
        case WAH_OP_I32_STORE: {
            uint32_t align, offset;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &align));
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &offset));
            wah_val_type_t val_type, addr_type;
            WAH_CHECK(wah_validation_pop_type(vctx, &val_type));
            WAH_CHECK(wah_validation_pop_type(vctx, &addr_type));
            if (val_type != WAH_VAL_TYPE_I32 || addr_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
            return WAH_OK;
        }
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
            if (pages_to_grow_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
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
            if (size_type != WAH_VAL_TYPE_I32 || val_type != WAH_VAL_TYPE_I32 || dst_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
            return WAH_OK; // Pushes nothing
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
                expected_type = vctx->module->code_bodies[0].local_types[local_idx - vctx->func_type->param_count];
            }
            
            if (opcode_val == WAH_OP_LOCAL_GET) {
                return wah_validation_push_type(vctx, expected_type);
            } else if (opcode_val == WAH_OP_LOCAL_SET) {
                wah_val_type_t popped_type;
                err = wah_validation_pop_type(vctx, &popped_type);
                return (err != WAH_OK || popped_type != expected_type) ? WAH_ERROR_VALIDATION_FAILED : WAH_OK;
            } else if (opcode_val == WAH_OP_LOCAL_TEE) { // WAH_OP_LOCAL_TEE
                wah_val_type_t popped_type;
                err = wah_validation_pop_type(vctx, &popped_type);
                if (err != WAH_OK || popped_type != expected_type) return WAH_ERROR_VALIDATION_FAILED;
                return wah_validation_push_type(vctx, expected_type);
            }
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

#define OPS(N) \
        /* Binary i32/i64 operations */ \
        case WAH_OP_I##N##_ADD: case WAH_OP_I##N##_SUB: case WAH_OP_I##N##_MUL: \
        case WAH_OP_I##N##_DIV_S: case WAH_OP_I##N##_DIV_U: case WAH_OP_I##N##_REM_S: case WAH_OP_I##N##_REM_U: \
        case WAH_OP_I##N##_AND: case WAH_OP_I##N##_OR: case WAH_OP_I##N##_XOR: \
        case WAH_OP_I##N##_SHL: case WAH_OP_I##N##_SHR_S: case WAH_OP_I##N##_SHR_U: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            if (type1 != WAH_VAL_TYPE_I##N || type2 != WAH_VAL_TYPE_I##N) return WAH_ERROR_VALIDATION_FAILED; \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I##N); \
        } \
        case WAH_OP_I##N##_EQ: case WAH_OP_I##N##_NE: \
        case WAH_OP_I##N##_LT_S: case WAH_OP_I##N##_LT_U: case WAH_OP_I##N##_GT_S: case WAH_OP_I##N##_GT_U: \
        case WAH_OP_I##N##_LE_S: case WAH_OP_I##N##_LE_U: case WAH_OP_I##N##_GE_S: case WAH_OP_I##N##_GE_U: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            if (type1 != WAH_VAL_TYPE_I##N || type2 != WAH_VAL_TYPE_I##N) return WAH_ERROR_VALIDATION_FAILED; \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); /* Comparisons return i32 */ \
        } \
        \
        /* Unary i32/i64 operations */ \
        case WAH_OP_I##N##_EQZ: { \
            wah_val_type_t type; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type)); \
            if (type != WAH_VAL_TYPE_I##N) return WAH_ERROR_VALIDATION_FAILED; \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); \
        } \
        \
        /* Binary f32/f64 operations */ \
        case WAH_OP_F##N##_ADD: case WAH_OP_F##N##_SUB: case WAH_OP_F##N##_MUL: case WAH_OP_F##N##_DIV: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            if (type1 != WAH_VAL_TYPE_F##N || type2 != WAH_VAL_TYPE_F##N) return WAH_ERROR_VALIDATION_FAILED; \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_F##N); \
        } \
        case WAH_OP_F##N##_EQ: case WAH_OP_F##N##_NE: \
        case WAH_OP_F##N##_LT: case WAH_OP_F##N##_GT: case WAH_OP_F##N##_LE: case WAH_OP_F##N##_GE: { \
            wah_val_type_t type1, type2; \
            WAH_CHECK(wah_validation_pop_type(vctx, &type1)); \
            WAH_CHECK(wah_validation_pop_type(vctx, &type2)); \
            if (type1 != WAH_VAL_TYPE_F##N || type2 != WAH_VAL_TYPE_F##N) return WAH_ERROR_VALIDATION_FAILED; \
            return wah_validation_push_type(vctx, WAH_VAL_TYPE_I32); /* Comparisons return i32 */ \
        }

        OPS(32)
        OPS(64)

#undef OPS

        // Parametric operations
        case WAH_OP_DROP: {
            wah_val_type_t type;
            return wah_validation_pop_type(vctx, &type);
        }
        
        case WAH_OP_SELECT: {
            wah_val_type_t c_type, b_type, a_type;
            err = wah_validation_pop_type(vctx, &c_type);
            if (err != WAH_OK || c_type != WAH_VAL_TYPE_I32) return WAH_ERROR_VALIDATION_FAILED;
            WAH_CHECK(wah_validation_pop_type(vctx, &b_type));
            err = wah_validation_pop_type(vctx, &a_type);
            if (err != WAH_OK || a_type != b_type) return WAH_ERROR_VALIDATION_FAILED;
            return wah_validation_push_type(vctx, a_type);
        }
        
        // Control flow operations
        case WAH_OP_NOP:
        case WAH_OP_UNREACHABLE:
            return WAH_OK;
            
        default:
            // For now, assume other opcodes are valid
            return WAH_OK;
    }
}

static wah_error_t wah_parse_code_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    wah_error_t err;
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    module->code_count = count;
    module->code_bodies = (wah_code_body_t*)malloc(sizeof(wah_code_body_t) * count);
    if (!module->code_bodies) return WAH_ERROR_OUT_OF_MEMORY;
    memset(module->code_bodies, 0, sizeof(wah_code_body_t) * count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t body_size;
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &body_size));

        (void)*ptr; // code_start_of_body would be used for debugging
        const uint8_t *code_end = *ptr + body_size;

        // Parse locals
        uint32_t num_local_entries;
        const uint8_t *locals_start = *ptr;
        WAH_CHECK(wah_decode_uleb128(ptr, code_end, &num_local_entries));

        uint32_t current_local_count = 0;
        // First pass to count total locals
        for (uint32_t j = 0; j < num_local_entries; ++j) {
            uint32_t local_type_count;
            WAH_CHECK(wah_decode_uleb128(ptr, code_end, &local_type_count));
            (*ptr)++; // Skip the actual type byte
            current_local_count += local_type_count;
        }
        module->code_bodies[i].local_count = current_local_count;
        module->code_bodies[i].local_types = (wah_val_type_t*)malloc(sizeof(wah_val_type_t) * current_local_count);
        if (!module->code_bodies[i].local_types) return WAH_ERROR_OUT_OF_MEMORY;

        // Rewind ptr to re-read locals and store types
        *ptr = locals_start; // Reset ptr to the beginning of the local entries
        WAH_CHECK(wah_decode_uleb128(ptr, code_end, &num_local_entries)); // Re-read num_local_entries

        uint32_t local_idx = 0;
        for (uint32_t j = 0; j < num_local_entries; ++j) {
            uint32_t local_type_count;
            WAH_CHECK(wah_decode_uleb128(ptr, code_end, &local_type_count));
            wah_val_type_t type = (wah_val_type_t)*(*ptr)++;
            for (uint32_t k = 0; k < local_type_count; ++k) {
                module->code_bodies[i].local_types[local_idx++] = type;
            }
        }

        // The remaining bytes are the actual code
        module->code_bodies[i].code_size = code_end - *ptr; // Corrected calculation
        module->code_bodies[i].code = *ptr; // Point directly into the WASM binary

        // --- Validation Pass for Code Body ---
        const wah_func_type_t *func_type = &module->types[module->function_type_indices[i]];
        uint32_t current_total_locals = func_type->param_count + module->code_bodies[i].local_count;

        const uint8_t *code_ptr_validation = *ptr;
        const uint8_t *code_body_end_validation = code_end;

        wah_validation_context_t vctx;
        memset(&vctx, 0, sizeof(wah_validation_context_t));
        vctx.module = module;
        vctx.func_type = func_type;
        vctx.total_locals = current_total_locals;
        vctx.current_stack_depth = 0;
        vctx.max_stack_depth = 0;

        while (code_ptr_validation < code_body_end_validation) {
            uint16_t current_opcode_val;
            uint8_t first_byte = *code_ptr_validation++;

            if (first_byte == 0xFC) {
                uint32_t sub_opcode_val;
                WAH_CHECK(wah_decode_uleb128(&code_ptr_validation, code_body_end_validation, &sub_opcode_val));
                if (sub_opcode_val > 0xFFF) return WAH_ERROR_INVALID_LEB128; // Sub-opcode out of expected range
                current_opcode_val = (uint16_t)(0xC000 | sub_opcode_val);
            } else {
                current_opcode_val = (uint16_t)first_byte;
            }
            
            if (current_opcode_val == WAH_OP_END) {
                // At the end of a function, the type stack should match the function's result type
                if (vctx.func_type->result_count == 0) {
                    if (vctx.type_stack.sp != 0) {
                        return WAH_ERROR_VALIDATION_FAILED; // Stack should be empty for void return
                    }
                } else if (vctx.func_type->result_count == 1) {
                    wah_val_type_t popped_type;
                    err = wah_validation_pop_type(&vctx, &popped_type);
                    if (err != WAH_OK || vctx.type_stack.sp != 0 || popped_type != vctx.func_type->result_types[0]) {
                        return WAH_ERROR_VALIDATION_FAILED; // Stack should have one result of correct type
                    }
                } else {
                    // Multiple results not yet supported in this validation
                    return WAH_ERROR_VALIDATION_FAILED;
                }
                break;
            } else if (current_opcode_val == WAH_OP_CALL) {
                uint32_t func_idx;
                WAH_CHECK(wah_decode_uleb128(&code_ptr_validation, code_body_end_validation, &func_idx));
                // Basic validation: check if func_idx is within bounds
                if (func_idx >= module->function_count) {
                    return WAH_ERROR_VALIDATION_FAILED;
                }
                // Type checking for call
                const wah_func_type_t *called_func_type = &module->types[module->function_type_indices[func_idx]];
                // Pop parameters from type stack
                for (int32_t j = called_func_type->param_count - 1; j >= 0; --j) {
                    wah_val_type_t popped_type;
                    err = wah_validation_pop_type(&vctx, &popped_type);
                    if (err != WAH_OK || popped_type != called_func_type->param_types[j]) {
                        return WAH_ERROR_VALIDATION_FAILED;
                    }
                }
                // Push results onto type stack
                for (uint32_t j = 0; j < called_func_type->result_count; ++j) {
                    WAH_CHECK(wah_validation_push_type(&vctx, called_func_type->result_types[j]));
                }
            } else {
                WAH_CHECK(wah_validate_opcode(current_opcode_val, &code_ptr_validation, code_body_end_validation, &vctx));
            }
        }
        // --- End Validation Pass ---
        
        // Store the maximum stack depth computed during validation
        module->code_bodies[i].max_stack_depth = vctx.max_stack_depth;

        // Pre-parse the code for optimized execution
        WAH_CHECK(wah_preparse_code(module->code_bodies[i].code, module->code_bodies[i].code_size, &module->code_bodies[i].parsed_code));

        *ptr = code_end; // Advance ptr past this function body
    }
    return WAH_OK;
}

static wah_error_t wah_parse_global_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));
    module->global_count = count;
    module->globals = (wah_global_t*)malloc(sizeof(wah_global_t) * count);
    if (!module->globals) return WAH_ERROR_OUT_OF_MEMORY;
    memset(module->globals, 0, sizeof(wah_global_t) * count);

    for (uint32_t i = 0; i < count; ++i) {
        // Global Type
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        module->globals[i].type = (wah_val_type_t)*(*ptr)++;

        // Mutability
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        module->globals[i].is_mutable = (*(*ptr)++ == 1);

        // Init Expr (only const expressions are supported for initial values)
        if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
        wah_opcode_t opcode = (wah_opcode_t)*(*ptr)++;
        switch (opcode) {
            case WAH_OP_I32_CONST: {
                int32_t val;
                WAH_CHECK(wah_decode_sleb128_32(ptr, section_end, &val));
                module->globals[i].initial_value.i32 = val;
                break;
            }
            case WAH_OP_I64_CONST: {
                int64_t val;
                WAH_CHECK(wah_decode_sleb128_64(ptr, section_end, &val));
                module->globals[i].initial_value.i64 = val;
                break;
            }
            case WAH_OP_F32_CONST: {
                if (*ptr + 4 > section_end) return WAH_ERROR_UNEXPECTED_EOF;
                module->globals[i].initial_value.f32 = wah_read_f32_le(*ptr);
                *ptr += 4;
                break;
            }
            case WAH_OP_F64_CONST: {
                if (*ptr + 8 > section_end) return WAH_ERROR_UNEXPECTED_EOF;
                module->globals[i].initial_value.f64 = wah_read_f64_le(*ptr);
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
        module->memories = (wah_memory_type_t*)malloc(sizeof(wah_memory_type_t) * count);
        if (!module->memories) return WAH_ERROR_OUT_OF_MEMORY;
        memset(module->memories, 0, sizeof(wah_memory_type_t) * count);

        for (uint32_t i = 0; i < count; ++i) {
            if (*ptr >= section_end) return WAH_ERROR_UNEXPECTED_EOF;
            uint8_t flags = *(*ptr)++; // Flags for memory type (0x00 for fixed, 0x01 for resizable)

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->memories[i].min_pages));
            
            if (flags & 0x01) { // If resizable, read max_pages
                WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->memories[i].max_pages));
            } else {
                module->memories[i].max_pages = module->memories[i].min_pages; // Fixed size
            }
        }
    }
    return WAH_OK;
}

// Pre-parsing function to convert bytecode to optimized structure
static wah_error_t wah_preparse_code(const uint8_t *code, uint32_t code_size, wah_parsed_code_t *parsed_code) {
    memset(parsed_code, 0, sizeof(wah_parsed_code_t));
    
    // First pass: count instructions and arguments
    const uint8_t *ptr = code;
    const uint8_t *end = code + code_size;
    uint32_t instruction_count = 0;
    uint32_t u8_arg_count = 0, u16_arg_count = 0, u32_arg_count = 0, u64_arg_count = 0;
    
    while (ptr < end) {
        wah_opcode_t opcode = (wah_opcode_t)*ptr++;
        instruction_count++;
        
        switch (opcode) {
            case WAH_OP_LOCAL_GET:
            case WAH_OP_LOCAL_SET:
            case WAH_OP_LOCAL_TEE:
            case WAH_OP_GLOBAL_GET:
            case WAH_OP_GLOBAL_SET:
            case WAH_OP_CALL: {
                uint32_t arg;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &arg));
                u32_arg_count++;
                break;
            }
            case WAH_OP_I32_CONST: {
                int32_t arg;
                WAH_CHECK(wah_decode_sleb128_32(&ptr, end, &arg));
                u32_arg_count++;
                break;
            }
            case WAH_OP_I64_CONST: {
                int64_t arg;
                WAH_CHECK(wah_decode_sleb128_64(&ptr, end, &arg));
                u64_arg_count++;
                break;
            }
            case WAH_OP_F32_CONST: {
                if (ptr + 4 > end) return WAH_ERROR_UNEXPECTED_EOF;
                ptr += 4;
                u32_arg_count++;
                break;
            }
            case WAH_OP_F64_CONST: {
                if (ptr + 8 > end) return WAH_ERROR_UNEXPECTED_EOF;
                ptr += 8;
                u64_arg_count++;
                break;
            }
            case WAH_OP_I32_LOAD:
            case WAH_OP_I32_STORE: {
                uint32_t align_flags;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &align_flags)); // align flags
                uint32_t offset;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &offset)); // offset
                u32_arg_count += 2;
                break;
            }
            case WAH_OP_MEMORY_SIZE:
            case WAH_OP_MEMORY_GROW: {
                uint32_t mem_idx;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &mem_idx)); // memory index (always 0x00)
                break;
            }
            case 0xFC: { // Multi-byte opcode prefix
                uint32_t sub_opcode_val;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &sub_opcode_val));
                if (sub_opcode_val > 0xFFF) return WAH_ERROR_INVALID_LEB128; // Sub-opcode out of expected range
                wah_misc_opcode_t misc_opcode = (wah_misc_opcode_t)sub_opcode_val;

                switch (misc_opcode) {
                    case WAH_MISC_OP_MEMORY_FILL: {
                        uint32_t mem_idx;
                        WAH_CHECK(wah_decode_uleb128(&ptr, end, &mem_idx)); // memory index (always 0x00)
                        break;
                    }
                    default:
                        return WAH_ERROR_UNKNOWN_SECTION; // Unknown miscellaneous opcode
                }
                break;
            }
            case WAH_OP_END:
            case WAH_OP_RETURN:
            case WAH_OP_NOP:
            case WAH_OP_UNREACHABLE:
            case WAH_OP_DROP:
            case WAH_OP_SELECT:
            // All binary and unary operations without immediate arguments
            case WAH_OP_I32_ADD: case WAH_OP_I32_SUB: case WAH_OP_I32_MUL:
            case WAH_OP_I32_DIV_S: case WAH_OP_I32_DIV_U: case WAH_OP_I32_REM_S: case WAH_OP_I32_REM_U:
            case WAH_OP_I32_AND: case WAH_OP_I32_OR: case WAH_OP_I32_XOR:
            case WAH_OP_I32_SHL: case WAH_OP_I32_SHR_S: case WAH_OP_I32_SHR_U:
            case WAH_OP_I32_EQ: case WAH_OP_I32_NE: case WAH_OP_I32_LT_S: case WAH_OP_I32_LT_U:
            case WAH_OP_I32_GT_S: case WAH_OP_I32_GT_U: case WAH_OP_I32_LE_S: case WAH_OP_I32_LE_U:
            case WAH_OP_I32_GE_S: case WAH_OP_I32_GE_U: case WAH_OP_I32_EQZ:
            case WAH_OP_I64_ADD: case WAH_OP_I64_SUB: case WAH_OP_I64_MUL:
            case WAH_OP_I64_DIV_S: case WAH_OP_I64_DIV_U: case WAH_OP_I64_REM_S: case WAH_OP_I64_REM_U:
            case WAH_OP_I64_AND: case WAH_OP_I64_OR: case WAH_OP_I64_XOR:
            case WAH_OP_I64_SHL: case WAH_OP_I64_SHR_S: case WAH_OP_I64_SHR_U:
            case WAH_OP_I64_EQ: case WAH_OP_I64_NE: case WAH_OP_I64_LT_S: case WAH_OP_I64_LT_U:
            case WAH_OP_I64_GT_S: case WAH_OP_I64_GT_U: case WAH_OP_I64_LE_S: case WAH_OP_I64_LE_U:
            case WAH_OP_I64_GE_S: case WAH_OP_I64_GE_U: case WAH_OP_I64_EQZ:
            case WAH_OP_F32_ADD: case WAH_OP_F32_SUB: case WAH_OP_F32_MUL: case WAH_OP_F32_DIV:
            case WAH_OP_F32_EQ: case WAH_OP_F32_NE: case WAH_OP_F32_LT: case WAH_OP_F32_GT: 
            case WAH_OP_F32_LE: case WAH_OP_F32_GE:
            case WAH_OP_F64_ADD: case WAH_OP_F64_SUB: case WAH_OP_F64_MUL: case WAH_OP_F64_DIV:
            case WAH_OP_F64_EQ: case WAH_OP_F64_NE: case WAH_OP_F64_LT: case WAH_OP_F64_GT: 
            case WAH_OP_F64_LE: case WAH_OP_F64_GE:
                // No arguments
                break;
            default:
                return WAH_ERROR_UNKNOWN_SECTION; // Unknown opcode
        }
    }
    
    // Allocate arrays
    parsed_code->instruction_count = instruction_count;
    parsed_code->opcodes = (uint16_t*)malloc(instruction_count * sizeof(uint16_t));
    if (!parsed_code->opcodes) return WAH_ERROR_OUT_OF_MEMORY;
    
    if (u8_arg_count > 0) {
        parsed_code->u8_args = (uint8_t*)malloc(u8_arg_count);
        if (!parsed_code->u8_args) { free(parsed_code->opcodes); return WAH_ERROR_OUT_OF_MEMORY; }
    }
    if (u16_arg_count > 0) {
        parsed_code->u16_args = (uint16_t*)malloc(u16_arg_count * sizeof(uint16_t));
        if (!parsed_code->u16_args) { free(parsed_code->opcodes); free(parsed_code->u8_args); return WAH_ERROR_OUT_OF_MEMORY; }
    }
    if (u32_arg_count > 0) {
        parsed_code->u32_args = (uint32_t*)malloc(u32_arg_count * sizeof(uint32_t));
        if (!parsed_code->u32_args) { 
            free(parsed_code->opcodes); free(parsed_code->u8_args); free(parsed_code->u16_args); 
            return WAH_ERROR_OUT_OF_MEMORY; 
        }
    }
    if (u64_arg_count > 0) {
        parsed_code->u64_args = (uint64_t*)malloc(u64_arg_count * sizeof(uint64_t));
        if (!parsed_code->u64_args) { 
            free(parsed_code->opcodes); free(parsed_code->u8_args); free(parsed_code->u16_args); free(parsed_code->u32_args);
            return WAH_ERROR_OUT_OF_MEMORY; 
        }
    }
    
    // Second pass: populate arrays
    ptr = code;
    uint32_t op_idx = 0;
    parsed_code->u8_arg_count = 0;
    parsed_code->u16_arg_count = 0; 
    parsed_code->u32_arg_count = 0;
    parsed_code->u64_arg_count = 0;
    
    while (ptr < end) {
        uint16_t current_opcode_val;
        uint8_t first_byte = *ptr++; // Read first byte

        if (first_byte == 0xFC) {
            uint32_t sub_opcode_val;
            WAH_CHECK(wah_decode_uleb128(&ptr, end, &sub_opcode_val));
            assert(sub_opcode_val <= 0xFFF); // Already checked in first pass
            current_opcode_val = (uint16_t)(0xC000 | sub_opcode_val);
        } else {
            current_opcode_val = (uint16_t)first_byte;
        }
        parsed_code->opcodes[op_idx++] = current_opcode_val; // Store the remapped opcode
        
        switch (current_opcode_val) {
            case WAH_OP_LOCAL_GET:
            case WAH_OP_LOCAL_SET:
            case WAH_OP_LOCAL_TEE:
            case WAH_OP_GLOBAL_GET:
            case WAH_OP_GLOBAL_SET:
            case WAH_OP_CALL: {
                uint32_t arg;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &arg));
                parsed_code->u32_args[parsed_code->u32_arg_count++] = arg;
                break;
            }
            case WAH_OP_I32_CONST: {
                int32_t arg;
                WAH_CHECK(wah_decode_sleb128_32(&ptr, end, &arg));
                parsed_code->u32_args[parsed_code->u32_arg_count++] = (uint32_t)arg;
                break;
            }
            case WAH_OP_I64_CONST: {
                int64_t arg;
                WAH_CHECK(wah_decode_sleb128_64(&ptr, end, &arg));
                parsed_code->u64_args[parsed_code->u64_arg_count++] = (uint64_t)arg;
                break;
            }
            case WAH_OP_F32_CONST: {
                parsed_code->u32_args[parsed_code->u32_arg_count++] = wah_read_u32_le(ptr);
                ptr += 4;
                break;
            }
            case WAH_OP_F64_CONST: {
                parsed_code->u64_args[parsed_code->u64_arg_count++] = wah_read_u64_le(ptr);
                ptr += 8;
                break;
            }
            case WAH_OP_I32_LOAD:
            case WAH_OP_I32_STORE: {
                uint32_t align_flags;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &align_flags)); // align flags
                uint32_t offset;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &offset)); // offset
                parsed_code->u32_args[parsed_code->u32_arg_count++] = align_flags;
                parsed_code->u32_args[parsed_code->u32_arg_count++] = offset;
                break;
            }
            case WAH_OP_MEMORY_SIZE:
            case WAH_OP_MEMORY_GROW: {
                uint32_t mem_idx;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &mem_idx)); // memory index (always 0x00)
                // No argument to store
                break;
            }
            case WAH_OP_MEMORY_FILL: { // This is the remapped opcode
                uint32_t mem_idx;
                WAH_CHECK(wah_decode_uleb128(&ptr, end, &mem_idx)); // memory index (always 0x00)
                // No argument to store
                break;
            }
            default:
                // No arguments to store
                break;
        }
    }
    
    return WAH_OK;
}

static void wah_free_parsed_code(wah_parsed_code_t *parsed_code) {
    if (!parsed_code) return;
    free(parsed_code->opcodes);
    free(parsed_code->u8_args);
    free(parsed_code->u16_args);
    free(parsed_code->u32_args);
    free(parsed_code->u64_args);
    memset(parsed_code, 0, sizeof(wah_parsed_code_t));
}

wah_error_t wah_parse_module(const uint8_t *wasm_binary, size_t binary_size, wah_module_t *module) {
    wah_error_t err = WAH_OK;
    if (!wasm_binary || !module || binary_size < 8) {
        return WAH_ERROR_UNEXPECTED_EOF; // Or a more specific error
    }

    memset(module, 0, sizeof(wah_module_t)); // Initialize module struct

    const uint8_t *ptr = wasm_binary;
    const uint8_t *end = wasm_binary + binary_size;

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

        (void)ptr; // section_payload_start would be used for debugging
        const uint8_t *section_payload_end = ptr + section_size;

        if (section_payload_end > end) {
            err = WAH_ERROR_UNEXPECTED_EOF;
            goto cleanup_parse;
        }

        switch (section_id) {
            case WAH_SECTION_TYPE:
                WAH_CHECK_GOTO(wah_parse_type_section(&ptr, section_payload_end, module), cleanup_parse);
                break;
            case WAH_SECTION_FUNCTION:
                WAH_CHECK_GOTO(wah_parse_function_section(&ptr, section_payload_end, module), cleanup_parse);
                break;
            case WAH_SECTION_CODE:
                WAH_CHECK_GOTO(wah_parse_code_section(&ptr, section_payload_end, module), cleanup_parse);
                break;
            case WAH_SECTION_GLOBAL:
                WAH_CHECK_GOTO(wah_parse_global_section(&ptr, section_payload_end, module), cleanup_parse);
                break;
            case WAH_SECTION_MEMORY:
                WAH_CHECK_GOTO(wah_parse_memory_section(&ptr, section_payload_end, module), cleanup_parse);
                break;
            case WAH_SECTION_CUSTOM:
            case WAH_SECTION_IMPORT:
            case WAH_SECTION_TABLE:
            case WAH_SECTION_EXPORT:
            case WAH_SECTION_START:
            case WAH_SECTION_ELEMENT:
            case WAH_SECTION_DATA:
            case WAH_SECTION_DATACOUNT:
                // Skip unknown or unimplemented sections for now
                ptr += section_size;
                break;
            default:
                // Unknown section ID
                return WAH_ERROR_UNKNOWN_SECTION;
        }

        // Ensure we consumed exactly the section_size bytes
        if (ptr != section_payload_end) {
            // This indicates a parsing error within the section
            // For now, we'll just advance the pointer to the end of the section
            // In a real implementation, this would be a more specific error
            ptr = section_payload_end;
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

    exec_ctx->value_stack_capacity = WAH_DEFAULT_VALUE_STACK_SIZE;
    exec_ctx->value_stack = (wah_value_t*)malloc(sizeof(wah_value_t) * exec_ctx->value_stack_capacity);
    if (!exec_ctx->value_stack) return WAH_ERROR_OUT_OF_MEMORY;

    exec_ctx->call_stack_capacity = WAH_DEFAULT_MAX_CALL_DEPTH;
    exec_ctx->call_stack = (wah_call_frame_t*)malloc(sizeof(wah_call_frame_t) * exec_ctx->call_stack_capacity);
    if (!exec_ctx->call_stack) {
        free(exec_ctx->value_stack);
        return WAH_ERROR_OUT_OF_MEMORY;
    }

    if (module->global_count > 0) {
        exec_ctx->globals = (wah_value_t*)malloc(sizeof(wah_value_t) * module->global_count);
        if (!exec_ctx->globals) {
            free(exec_ctx->value_stack);
            free(exec_ctx->call_stack);
            return WAH_ERROR_OUT_OF_MEMORY;
        }
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
        exec_ctx->memory_base = (uint8_t*)malloc(exec_ctx->memory_size);
        if (!exec_ctx->memory_base) {
            free(exec_ctx->value_stack);
            free(exec_ctx->call_stack);
            free(exec_ctx->globals);
            return WAH_ERROR_OUT_OF_MEMORY;
        }
        memset(exec_ctx->memory_base, 0, exec_ctx->memory_size);
    }

    return WAH_OK;
}

void wah_exec_context_destroy(wah_exec_context_t *exec_ctx) {
    if (!exec_ctx) return;
    free(exec_ctx->value_stack);
    free(exec_ctx->call_stack);
    free(exec_ctx->globals);
    free(exec_ctx->memory_base);
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
    frame->ip = code_body->parsed_code.opcodes;
    frame->u8_arg_ptr = code_body->parsed_code.u8_args;
    frame->u16_arg_ptr = code_body->parsed_code.u16_args;
    frame->u32_arg_ptr = code_body->parsed_code.u32_args;
    frame->u64_arg_ptr = code_body->parsed_code.u64_args;
    frame->locals_offset = locals_offset;
    frame->func_idx = func_idx;

    return WAH_OK;
}

#define RELOAD_FRAME() \
    do { \
        frame = &ctx->call_stack[ctx->call_depth - 1]; \
        ip = frame->ip; \
        u32_arg_ptr = frame->u32_arg_ptr; \
        u64_arg_ptr = frame->u64_arg_ptr; \
    } while(0)

static wah_error_t wah_run_interpreter(wah_exec_context_t *ctx) {
    wah_error_t err = WAH_OK;

    // These are pointers to the current frame's state for faster access.
    wah_call_frame_t *frame;
    const uint16_t *ip;
    const uint32_t *u32_arg_ptr;
    const uint64_t *u64_arg_ptr;

    RELOAD_FRAME(); // Initial frame load

    while (ctx->call_depth > 0) { // Loop while there are active call frames
        uint16_t opcode = *ip++;

        switch (opcode) {
            case WAH_OP_I32_CONST: {
                ctx->value_stack[ctx->sp++].i32 = (int32_t)*u32_arg_ptr++;
                break;
            }
            case WAH_OP_I64_CONST: {
                ctx->value_stack[ctx->sp++].i64 = (int64_t)*u64_arg_ptr++;
                break;
            }
            case WAH_OP_F32_CONST: {
                memcpy(&ctx->value_stack[ctx->sp++].f32, u32_arg_ptr++, 4);
                break;
            }
            case WAH_OP_F64_CONST: {
                memcpy(&ctx->value_stack[ctx->sp++].f64, u64_arg_ptr++, 8);
                break;
            }
            case WAH_OP_LOCAL_GET: {
                uint32_t local_idx = *u32_arg_ptr++;
                ctx->value_stack[ctx->sp++] = ctx->value_stack[frame->locals_offset + local_idx];
                break;
            }
            case WAH_OP_LOCAL_SET: {
                uint32_t local_idx = *u32_arg_ptr++;
                ctx->value_stack[frame->locals_offset + local_idx] = ctx->value_stack[--ctx->sp];
                break;
            }
            case WAH_OP_LOCAL_TEE: {
                uint32_t local_idx = *u32_arg_ptr++;
                wah_value_t val = ctx->value_stack[ctx->sp - 1];
                ctx->value_stack[frame->locals_offset + local_idx] = val;
                break;
            }
            case WAH_OP_GLOBAL_GET: {
                uint32_t global_idx = *u32_arg_ptr++;
                ctx->value_stack[ctx->sp++].i32 = ctx->globals[global_idx].i32;
                break;
            }
            case WAH_OP_GLOBAL_SET: {
                uint32_t global_idx = *u32_arg_ptr++;
                ctx->globals[global_idx].i32 = ctx->value_stack[--ctx->sp].i32;
                break;
            }
            case WAH_OP_CALL: {
                uint32_t called_func_idx = *u32_arg_ptr++;
                const wah_func_type_t *called_func_type = &ctx->module->types[ctx->module->function_type_indices[called_func_idx]];
                const wah_code_body_t *called_code = &ctx->module->code_bodies[called_func_idx];

                uint32_t new_locals_offset = ctx->sp - called_func_type->param_count;
                
                frame->ip = ip;
                frame->u32_arg_ptr = u32_arg_ptr;
                frame->u64_arg_ptr = u64_arg_ptr;

                err = wah_push_frame(ctx, called_func_idx, new_locals_offset);
                if (err != WAH_OK) goto cleanup;

                uint32_t num_locals = called_code->local_count;
                if (num_locals > 0) {
                    if (ctx->sp + num_locals > ctx->value_stack_capacity) {
                        err = WAH_ERROR_OUT_OF_MEMORY;
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

            #define VSTACK_B (ctx->value_stack[ctx->sp - 1])
            #define VSTACK_A (ctx->value_stack[ctx->sp - 2])
            #define BINOP_I(N,op) { VSTACK_A.i##N = (int64_t)((uint64_t)VSTACK_A.i##N op (uint64_t)VSTACK_B.i##N); ctx->sp--; break; }
            #define CMP_I_S(N,op) { VSTACK_A.i32 = VSTACK_A.i##N op VSTACK_B.i##N ? 1 : 0; ctx->sp--; break; }
            #define CMP_I_U(N,op) { VSTACK_A.i32 = (uint64_t)VSTACK_A.i##N op (uint64_t)VSTACK_B.i##N ? 1 : 0; ctx->sp--; break; }
            #define BINOP_F(N,op) { VSTACK_A.f##N = VSTACK_A.f##N op VSTACK_B.f##N; ctx->sp--; break; }
            #define CMP_F(N,op)   { VSTACK_A.i32 = VSTACK_A.f##N op VSTACK_B.f##N ? 1 : 0; ctx->sp--; break; }

#define OPS(N) \
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
            case WAH_OP_F##N##_ADD: BINOP_F(N,+) \
            case WAH_OP_F##N##_SUB: BINOP_F(N,-) \
            case WAH_OP_F##N##_MUL: BINOP_F(N,*) \
            case WAH_OP_F##N##_DIV: BINOP_F(N,/) /* Let hardware handle division by zero (NaN/inf) */ \
            case WAH_OP_F##N##_EQ: CMP_F(N,==) \
            case WAH_OP_F##N##_NE: CMP_F(N,!=) \
            case WAH_OP_F##N##_LT: CMP_F(N,<) \
            case WAH_OP_F##N##_GT: CMP_F(N,>) \
            case WAH_OP_F##N##_LE: CMP_F(N,<=) \
            case WAH_OP_F##N##_GE: CMP_F(N,>=)

            OPS(32)
            OPS(64)

#undef OPS

            case WAH_OP_I32_LOAD: {
                uint32_t align = *u32_arg_ptr++; // Not used for now, but parsed
                uint32_t offset = *u32_arg_ptr++;
                uint32_t addr = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t effective_addr = addr + offset;

                if (effective_addr >= ctx->memory_size || ctx->memory_size - effective_addr < 4) {
                    err = WAH_ERROR_MEMORY_OUT_OF_BOUNDS;
                    goto cleanup;
                }
                int32_t loaded_val;
                memcpy(&loaded_val, ctx->memory_base + effective_addr, 4);
                ctx->value_stack[ctx->sp++].i32 = loaded_val;
                break;
            }
            case WAH_OP_I32_STORE: {
                uint32_t align = *u32_arg_ptr++; // Not used for now, but parsed
                uint32_t offset = *u32_arg_ptr++;
                int32_t val = ctx->value_stack[--ctx->sp].i32;
                uint32_t addr = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t effective_addr = addr + offset;

                if (effective_addr >= ctx->memory_size || ctx->memory_size - effective_addr < 4) {
                    err = WAH_ERROR_MEMORY_OUT_OF_BOUNDS;
                    goto cleanup;
                }
                memcpy(ctx->memory_base + effective_addr, &val, 4);
                break;
            }
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
                uint32_t new_pages = old_pages + (uint32_t)pages_to_grow;

                // Check against max_pages if defined (module->memories[0].max_pages)
                // For now, we assume no max_pages or effectively unlimited if not set
                if (ctx->module->memory_count > 0 && ctx->module->memories[0].max_pages > 0 && new_pages > ctx->module->memories[0].max_pages) {
                    ctx->value_stack[ctx->sp++].i32 = -1; // Exceeds max memory
                    break;
                }

                size_t new_memory_size = (size_t)new_pages * WAH_WASM_PAGE_SIZE;
                uint8_t *new_memory_base = (uint8_t*)realloc(ctx->memory_base, new_memory_size);

                if (new_memory_base == NULL) {
                    ctx->value_stack[ctx->sp++].i32 = -1; // Realloc failed
                    break;
                }

                // Initialize newly allocated memory to zero
                if (new_memory_size > ctx->memory_size) {
                    memset(new_memory_base + ctx->memory_size, 0, new_memory_size - ctx->memory_size);
                }

                ctx->memory_base = new_memory_base;
                ctx->memory_size = (uint32_t)new_memory_size;
                ctx->value_stack[ctx->sp++].i32 = (int32_t)old_pages;
                break;
            }
            case WAH_OP_MEMORY_FILL: {
                // memory index (always 0x00) is consumed by preparse, no need to read here
                uint32_t size = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint8_t val = (uint8_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t dst = (uint32_t)ctx->value_stack[--ctx->sp].i32;

                if (dst + size > ctx->memory_size || dst > ctx->memory_size) {
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
        frame->ip = ip;
        frame->u32_arg_ptr = u32_arg_ptr;
        frame->u64_arg_ptr = u64_arg_ptr;
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
            return WAH_ERROR_OUT_OF_MEMORY; // Value stack overflow
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

    // Reset all fields to 0/NULL
    memset(module, 0, sizeof(wah_module_t));
}

#endif // WAH_IMPLEMENTATION
#endif // WAH_H
