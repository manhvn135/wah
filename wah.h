// WebAssembly interpreter in a Header file (WAH)

#ifndef WAH_H
#define WAH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

typedef enum {
    WAH_OK = 0,
    WAH_ERROR_INVALID_MAGIC_NUMBER,
    WAH_ERROR_INVALID_VERSION,
    WAH_ERROR_UNEXPECTED_EOF,
    WAH_ERROR_UNKNOWN_SECTION,
    WAH_ERROR_TOO_LARGE,
    WAH_ERROR_OUT_OF_MEMORY,
    WAH_ERROR_VALIDATION_FAILED,
    WAH_ERROR_TRAP,
    WAH_ERROR_CALL_STACK_OVERFLOW,
    WAH_ERROR_MEMORY_OUT_OF_BOUNDS,
    WAH_ERROR_NOT_FOUND,
    WAH_ERROR_MISUSE,
} wah_error_t;

typedef union {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
} wah_value_t;

typedef int32_t wah_type_t;

#define WAH_TYPE_I32 -1
#define WAH_TYPE_I64 -2
#define WAH_TYPE_F32 -3
#define WAH_TYPE_F64 -4

#define WAH_TYPE_IS_FUNCTION(t) ((t) == -100)
#define WAH_TYPE_IS_MEMORY(t)   ((t) == -200)
#define WAH_TYPE_IS_TABLE(t)    ((t) == -300)
#define WAH_TYPE_IS_GLOBAL(t)   ((t) >= -4)

typedef uint64_t wah_entry_id_t;

typedef struct {
    wah_entry_id_t id;
    wah_type_t type;
    const char *name;
    size_t name_len;
    bool is_mutable;

    // Semi-private fields:
    union {
        wah_value_t global_val; // For WAH_TYPE_IS_GLOBAL
        struct { // For WAH_TYPE_IS_MEMORY
            uint32_t min_pages, max_pages;
        } memory;
        struct { // For WAH_TYPE_IS_TABLE
            wah_type_t elem_type;
            uint32_t min_elements, max_elements;
        } table;
        struct { // For WAH_TYPE_FUNCTION
            uint32_t param_count, result_count;
            const wah_type_t *param_types, *result_types;
        } func;
    } u;
} wah_entry_t;

typedef struct wah_module_s {
    uint32_t type_count;
    uint32_t function_count;
    uint32_t code_count;
    uint32_t global_count;
    uint32_t memory_count;
    uint32_t table_count;
    uint32_t element_segment_count;
    uint32_t data_segment_count;
    uint32_t export_count;

    uint32_t start_function_idx;
    bool has_start_function;
    bool has_data_count_section; // True if data count section was present
    uint32_t min_data_segment_count_required;

    struct wah_func_type_s *types;
    uint32_t *function_type_indices; // Index into the types array
    struct wah_code_body_s *code_bodies;
    struct wah_global_s *globals;
    struct wah_memory_type_s *memories;
    struct wah_table_type_s *tables;
    struct wah_element_segment_s *element_segments;
    struct wah_data_segment_s *data_segments;
    struct wah_export_s *exports;
} wah_module_t;

typedef struct wah_exec_context_s {
    wah_value_t *value_stack;       // A single, large stack for operands and locals
    uint32_t sp;                    // Stack pointer for the value_stack (points to next free slot)
    uint32_t value_stack_capacity;

    struct wah_call_frame_s *call_stack; // The call frame stack
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

// Convert error code to human-readable string
const char *wah_strerror(wah_error_t err);

wah_error_t wah_parse_module(const uint8_t *wasm_binary, size_t binary_size, wah_module_t *module);

size_t wah_module_num_exports(const wah_module_t *module);
wah_error_t wah_module_export(const wah_module_t *module, size_t idx, wah_entry_t *out);
wah_error_t wah_module_export_by_name(const wah_module_t *module, const char *name, wah_entry_t *out);

wah_error_t wah_module_entry(const wah_module_t *module, wah_entry_id_t entry_id, wah_entry_t *out);

// Creates and initializes an execution context.
wah_error_t wah_exec_context_create(wah_exec_context_t *exec_ctx, const wah_module_t *module);

// Destroys and frees resources of an execution context.
void wah_exec_context_destroy(wah_exec_context_t *exec_ctx);

// The main entry point to call a WebAssembly function.
wah_error_t wah_call(wah_exec_context_t *exec_ctx, const wah_module_t *module, uint32_t func_idx, const wah_value_t *params, uint32_t param_count, wah_value_t *result);

// --- Module Cleanup ---
void wah_free_module(wah_module_t *module);

wah_error_t wah_module_entry(const wah_module_t *module, wah_entry_id_t entry_id, wah_entry_t *out);

// Accessors for wah_entry_t
static inline int32_t wah_entry_i32(const wah_entry_t *entry) {
    assert(entry);
    return entry->type == WAH_TYPE_I32 ? entry->u.global_val.i32 : 0;
}

static inline int64_t wah_entry_i64(const wah_entry_t *entry) {
    assert(entry);
    return entry->type == WAH_TYPE_I64 ? entry->u.global_val.i64 : 0;
}

static inline float wah_entry_f32(const wah_entry_t *entry) {
    assert(entry);
    return entry->type == WAH_TYPE_F32 ? entry->u.global_val.f32 : 0.0f / 0.0f;
}

static inline double wah_entry_f64(const wah_entry_t *entry) {
    assert(entry);
    return entry->type == WAH_TYPE_F64 ? entry->u.global_val.f64 : 0.0 / 0.0;
}

static inline wah_error_t wah_entry_memory(const wah_entry_t *entry, uint32_t *min_pages, uint32_t *max_pages) {
    if (!entry) return WAH_ERROR_MISUSE;
    if (!min_pages) return WAH_ERROR_MISUSE;
    if (!max_pages) return WAH_ERROR_MISUSE;
    if (!WAH_TYPE_IS_MEMORY(entry->type)) return WAH_ERROR_MISUSE;
    *min_pages = entry->u.memory.min_pages;
    *max_pages = entry->u.memory.max_pages;
    return WAH_OK;
}

static inline wah_error_t wah_entry_table(const wah_entry_t *entry, wah_type_t *elem_type, uint32_t *min_elements, uint32_t *max_elements) {
    if (!entry) return WAH_ERROR_MISUSE;
    if (!elem_type) return WAH_ERROR_MISUSE;
    if (!min_elements) return WAH_ERROR_MISUSE;
    if (!max_elements) return WAH_ERROR_MISUSE;
    if (!WAH_TYPE_IS_TABLE(entry->type)) return WAH_ERROR_MISUSE;
    *elem_type = entry->u.table.elem_type; // Directly assign wah_type_t
    *min_elements = entry->u.table.min_elements;
    *max_elements = entry->u.table.max_elements;
    return WAH_OK;
}

static inline wah_error_t wah_entry_func(const wah_entry_t *entry,
                                         uint32_t *out_nargs, const wah_type_t **out_args,
                                         uint32_t *out_nrets, const wah_type_t **out_rets) {
    if (!entry) return WAH_ERROR_MISUSE;
    if (!out_nargs) return WAH_ERROR_MISUSE;
    if (!out_args) return WAH_ERROR_MISUSE;
    if (!out_nrets) return WAH_ERROR_MISUSE;
    if (!out_rets) return WAH_ERROR_MISUSE;
    if (!WAH_TYPE_IS_FUNCTION(entry->type)) return WAH_ERROR_MISUSE;
    *out_nargs = entry->u.func.param_count;
    *out_args = entry->u.func.param_types;
    *out_nrets = entry->u.func.result_count;
    *out_rets = entry->u.func.result_types;
    return WAH_OK;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef WAH_IMPLEMENTATION

#include <string.h> // For memcpy, memset
#include <stdlib.h> // For malloc, free
#include <assert.h> // For assert
#include <stdint.h> // For INT32_MIN, INT32_MAX
#include <math.h> // For floating-point functions
#if defined(_MSC_VER)
#include <intrin.h> // For MSVC intrinsics
#endif

#ifdef WAH_DEBUG
#include <stdio.h>
#define WAH_LOG(fmt, ...) printf("(%d) " fmt "\n", __LINE__, ##__VA_ARGS__)
#else
#define WAH_LOG(fmt, ...) (void)(0)
#endif

#define WAH_TYPE_FUNCTION -100
#define WAH_TYPE_MEMORY   -200
#define WAH_TYPE_TABLE    -300

#define WAH_TYPE_FUNCREF WAH_TYPE_FUNCTION
#define WAH_TYPE_ANY -5

#define WAH_ENTRY_KIND_FUNCTION 0
#define WAH_ENTRY_KIND_TABLE    1
#define WAH_ENTRY_KIND_MEMORY   2
#define WAH_ENTRY_KIND_GLOBAL   3

#define WAH_MAKE_ENTRY_ID(kind, index) (((wah_entry_id_t)(kind) << 32) | (index))
#define WAH_GET_ENTRY_KIND(id)         ((uint32_t)((id) >> 32))
#define WAH_GET_ENTRY_INDEX(id)        ((uint32_t)((id) & 0xFFFFFFFF))

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
    WAH_OP_MEMORY_SIZE = 0x3F, WAH_OP_MEMORY_GROW = 0x40,
    WAH_OP_MEMORY_INIT = 0xC008, WAH_OP_MEMORY_COPY = 0xC00A, WAH_OP_MEMORY_FILL = 0xC00B,

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

// --- Memory Structure ---
#define WAH_WASM_PAGE_SIZE 65536 // 64 KB

typedef struct wah_memory_type_s {
    uint32_t min_pages, max_pages;
} wah_memory_type_t;

// --- WebAssembly Table Structures ---
typedef struct wah_table_type_s {
    wah_type_t elem_type;
    uint32_t min_elements;
    uint32_t max_elements; // 0 if no maximum
} wah_table_type_t;

typedef struct wah_data_segment_s {
    uint32_t flags;
    uint32_t memory_idx; // Only for active segments (flags & 0x02)
    uint32_t offset;     // Result of the offset_expr (i32.const X end)
    uint32_t data_len;
    const uint8_t *data; // Pointer to the raw data bytes within the WASM binary
} wah_data_segment_t;

typedef struct wah_export_s {
    const char *name;
    size_t name_len;
    uint8_t kind; // WASM export kind (0=func, 1=table, 2=mem, 3=global)
    uint32_t index; // Index into the respective module array (functions, tables, etc.)
} wah_export_t;

// --- WebAssembly Element Segment Structure ---
typedef struct wah_element_segment_s {
    uint32_t table_idx;
    uint32_t offset; // Result of the offset_expr
    uint32_t num_elems;
    uint32_t *func_indices; // Array of function indices
}
wah_element_segment_t;

// --- Operand Stack ---
typedef struct {
    wah_value_t *data; // Dynamically allocated based on function requirements
    uint32_t sp; // Stack pointer
    uint32_t capacity; // Allocated capacity
} wah_stack_t;

// --- Type Stack for Validation ---
#define WAH_MAX_TYPE_STACK_SIZE 1024 // Maximum size of the type stack for validation
typedef struct {
    wah_type_t data[WAH_MAX_TYPE_STACK_SIZE];
    uint32_t sp; // Stack pointer
} wah_type_stack_t;

// --- Execution Context ---

// Represents a single function call's state on the call stack.
typedef struct wah_call_frame_s {
    const uint8_t *bytecode_ip;  // Instruction pointer into the parsed bytecode
    const struct wah_code_body_s *code; // The function body being executed
    uint32_t locals_offset;      // Offset into the shared value_stack for this frame's locals
    uint32_t func_idx;           // Index of the function being executed
} wah_call_frame_t;

// The main context for the entire WebAssembly interpretation.
#define WAH_DEFAULT_MAX_CALL_DEPTH 1024
#define WAH_DEFAULT_VALUE_STACK_SIZE (64 * 1024)

// --- Function Type ---
typedef struct wah_func_type_s {
    uint32_t param_count;
    uint32_t result_count;
    wah_type_t *param_types;
    wah_type_t *result_types;
} wah_func_type_t;

// --- Pre-parsed Opcode Structure for Optimized Execution ---
typedef struct {
    uint8_t *bytecode;           // Combined array of opcodes and arguments
    uint32_t bytecode_size;      // Total size of the bytecode array
} wah_parsed_code_t;

// --- Code Body Structure ---
typedef struct wah_code_body_s {
    uint32_t local_count;
    wah_type_t *local_types; // Array of types for local variables
    uint32_t code_size;
    const uint8_t *code; // Pointer to the raw instruction bytes within the WASM binary
    uint32_t max_stack_depth; // Maximum operand stack depth required
    wah_parsed_code_t parsed_code; // Pre-parsed opcodes and arguments for optimized execution
} wah_code_body_t;

// --- Global Variable Structure ---
typedef struct wah_global_s {
    wah_type_t type;
    bool is_mutable;
    wah_value_t initial_value; // Stored after parsing the init_expr
} wah_global_t;

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
    wah_module_t *module; // Reference to the module for global/function lookups
    uint32_t total_locals; // Total number of locals (params + declared locals)
    uint32_t current_stack_depth; // Current stack depth during validation
    uint32_t max_stack_depth; // Maximum stack depth seen during validation
    bool is_unreachable; // True if the current code path is unreachable

    // Control flow validation stack
    wah_validation_control_frame_t control_stack[WAH_MAX_CONTROL_DEPTH];
    uint32_t control_sp;
} wah_validation_context_t;

// --- Helper Macros ---
#define WAH_CHECK(expr) do { \
    wah_error_t _err = (expr); \
    if (_err != WAH_OK) { WAH_LOG("WAH_CHECK(%s) failed due to: %s", #expr, wah_strerror(_err)); return _err; } \
} while(0)

#define WAH_CHECK_GOTO(expr, label) do { \
    err = (expr); \
    if (err != WAH_OK) { WAH_LOG("WAH_CHECK_GOTO(%s, %s) failed due to: %s", #expr, #label, wah_strerror(err)); goto label; } \
} while(0)

#define WAH_ENSURE(cond, error) do { \
    if (!(cond)) { WAH_LOG("WAH_ENSURE(%s, %s) failed", #cond, #error); return (error); } \
} while(0)

#define WAH_ENSURE_GOTO(cond, error, label) do { \
    if (!(cond)) { err = (error); WAH_LOG("WAH_ENSURE_GOTO(%s, %s, %s) failed", #cond, #error, #label); goto label; } \
} while(0)

// Helper macro to check for __builtin_xxx functions with __has_builtin
#if defined(__has_builtin)
#define WAH_HAS_BUILTIN(x) __has_builtin(x)
#else
#define WAH_HAS_BUILTIN(x) 0
#endif

// --- Safe Memory Allocation ---
static inline wah_error_t wah_malloc(size_t count, size_t elemsize, void** out_ptr) {
    *out_ptr = NULL;
    if (count == 0) {
        return WAH_OK;
    }
    WAH_ENSURE(elemsize == 0 || count <= SIZE_MAX / elemsize, WAH_ERROR_OUT_OF_MEMORY);
    size_t total_size = count * elemsize;
    *out_ptr = malloc(total_size);
    WAH_ENSURE(*out_ptr, WAH_ERROR_OUT_OF_MEMORY);
    return WAH_OK;
}

static inline wah_error_t wah_realloc(size_t count, size_t elemsize, void** p_ptr) {
    if (count == 0) {
        free(*p_ptr);
        *p_ptr = NULL;
        return WAH_OK;
    }
    WAH_ENSURE(elemsize == 0 || count <= SIZE_MAX / elemsize, WAH_ERROR_OUT_OF_MEMORY);
    size_t total_size = count * elemsize;
    void* new_ptr = realloc(*p_ptr, total_size);
    WAH_ENSURE(new_ptr, WAH_ERROR_OUT_OF_MEMORY);
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
        case WAH_ERROR_NOT_FOUND: return "Item not found";
        case WAH_ERROR_MISUSE: return "API misused: invalid arguments";
        default: return "Unknown error";
    }
}

// --- LEB128 Decoding ---
// Helper function to decode an unsigned LEB128 integer
static inline wah_error_t wah_decode_uleb128(const uint8_t **ptr, const uint8_t *end, uint32_t *result) {
    uint64_t val = 0;
    uint32_t shift = 0;
    uint8_t byte;

    for (int i = 0; i < 5; ++i) { // Max 5 bytes for a 32-bit value
        WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
        byte = *(*ptr)++;
        val |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            WAH_ENSURE(val <= UINT32_MAX, WAH_ERROR_TOO_LARGE);
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
        WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
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
            WAH_ENSURE((int64_t)val >= INT32_MIN && (int64_t)val <= INT32_MAX, WAH_ERROR_TOO_LARGE);
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
        WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
        WAH_ENSURE(shift < 64, WAH_ERROR_TOO_LARGE);
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
        WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
        WAH_ENSURE(shift < 64, WAH_ERROR_TOO_LARGE);
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

// Helper function to validate if a byte sequence is valid UTF-8
static inline bool wah_is_valid_utf8(const char *s, size_t len) {
    const unsigned char *bytes = (const unsigned char *)s;
    size_t i = 0;
    while (i < len) {
        unsigned char byte = bytes[i];
        if (byte < 0x80) { // 1-byte sequence (0xxxxxxx)
            i++;
            continue;
        }
        if ((byte & 0xE0) == 0xC0) { // 2-byte sequence (110xxxxx 10xxxxxx)
            if (i + 1 >= len || (bytes[i+1] & 0xC0) != 0x80 || (byte & 0xFE) == 0xC0) return false; // Overlong encoding
            i += 2;
            continue;
        }
        if ((byte & 0xF0) == 0xE0) { // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
            if (i + 2 >= len || (bytes[i+1] & 0xC0) != 0x80 || (bytes[i+2] & 0xC0) != 0x80 ||
                (byte == 0xE0 && bytes[i+1] < 0xA0) || // Overlong encoding
                (byte == 0xED && bytes[i+1] >= 0xA0)) return false; // Surrogate pair
            i += 3;
            continue;
        }
        if ((byte & 0xF8) == 0xF0) { // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            if (i + 3 >= len || (bytes[i+1] & 0xC0) != 0x80 || (bytes[i+2] & 0xC0) != 0x80 || (bytes[i+3] & 0xC0) != 0x80 ||
                (byte == 0xF0 && bytes[i+1] < 0x90) || // Overlong encoding
                (byte == 0xF4 && bytes[i+1] >= 0x90)) return false; // Codepoint > 0x10FFFF
            i += 4;
            continue;
        }
        return false; // Invalid start byte
    }
    return true;
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

// --- Parser Functions ---
static wah_error_t wah_decode_opcode(const uint8_t **ptr, const uint8_t *end, uint16_t *opcode_val);
static wah_error_t wah_decode_val_type(const uint8_t **ptr, const uint8_t *end, wah_type_t *out_type);
static wah_error_t wah_decode_ref_type(const uint8_t **ptr, const uint8_t *end, wah_type_t *out_type);

// The core interpreter loop (internal).
static wah_error_t wah_run_interpreter(wah_exec_context_t *exec_ctx);

// Validation helper functions
static wah_error_t wah_validate_opcode(uint16_t opcode_val, const uint8_t **code_ptr, const uint8_t *code_end, wah_validation_context_t *vctx, const wah_code_body_t* code_body);

// Pre-parsing functions
static wah_error_t wah_preparse_code(const wah_module_t* module, uint32_t func_idx, const uint8_t *code, uint32_t code_size, wah_parsed_code_t *parsed_code);
static void wah_free_parsed_code(wah_parsed_code_t *parsed_code);

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
#if WAH_HAS_BUILTIN(__builtin_popcount) || defined(__GNUC__)
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
#if WAH_HAS_BUILTIN(__builtin_popcountll) || defined(__GNUC__)
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
#if WAH_HAS_BUILTIN(__builtin_clz) || defined(__GNUC__)
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
#if WAH_HAS_BUILTIN(__builtin_clzll) || defined(__GNUC__)
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
#if WAH_HAS_BUILTIN(__builtin_ctz) || defined(__GNUC__)
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
#if WAH_HAS_BUILTIN(__builtin_ctzll) || defined(__GNUC__)
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
#if WAH_HAS_BUILTIN(__builtin_rotateleft32)
    return __builtin_rotateleft32(n, shift);
#elif defined(_MSC_VER)
    return _rotl(n, shift);
#else
    shift &= 31; // Ensure shift is within 0-31
    return (n << shift) | (n >> (32 - shift));
#endif
}

static inline uint64_t wah_rotl_u64(uint64_t n, uint64_t shift) {
#if WAH_HAS_BUILTIN(__builtin_rotateleft64)
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
#if WAH_HAS_BUILTIN(__builtin_rotateright32)
    return __builtin_rotateright32(n, shift);
#elif defined(_MSC_VER)
    return _rotr(n, shift);
#else
    shift &= 31; // Ensure shift is within 0-31
    return (n >> shift) | (n << (32 - shift));
#endif
}

static inline uint64_t wah_rotr_u64(uint64_t n, uint64_t shift) {
#if WAH_HAS_BUILTIN(__builtin_rotateright64)
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
#if WAH_HAS_BUILTIN(__builtin_roundevenf) && defined(__clang__)
    return __builtin_roundevenf(f);
#else
    if (isnan(f) || isinf(f) || f == 0.0f) return f;
    float rounded = roundf(f);
    if (fabsf(f - rounded) == 0.5f && ((long long)rounded % 2) != 0) return rounded - copysignf(1.0f, f);
    return rounded;
#endif
}

static inline double wah_nearest_f64(double d) {
#if WAH_HAS_BUILTIN(__builtin_roundeven) && defined(__clang__)
    return __builtin_roundeven(d);
#else
    if (isnan(d) || isinf(d) || d == 0.0) return d;
    double rounded = round(d);
    if (fabs(d - rounded) == 0.5 && ((long long)rounded % 2) != 0) return rounded - copysign(1.0, d);
    return rounded;
#endif
}

// Helper functions for floating-point to integer truncations with trap handling
#define DEFINE_TRUNC_F2I(N, fty, T, ity, lo, hi, call) \
static inline wah_error_t wah_trunc_f##N##_to_##T(fty val, ity *result) { \
    if (isnan(val) || isinf(val)) return WAH_ERROR_TRAP; \
    if (val < (lo) || val >= (hi)) return WAH_ERROR_TRAP; \
    *result = (ity)call(val); \
    return WAH_OK; \
}

DEFINE_TRUNC_F2I(32, float,  i32,  int32_t,  (float)INT32_MIN,  (float) INT32_MAX + 1.0f, truncf)
DEFINE_TRUNC_F2I(32, float,  u32, uint32_t,                 0,  (float)UINT32_MAX + 1.0f, truncf)
DEFINE_TRUNC_F2I(64, double, i32,  int32_t, (double)INT32_MIN, (double) INT32_MAX + 1.0,  trunc)
DEFINE_TRUNC_F2I(64, double, u32, uint32_t,                 0, (double)UINT32_MAX + 1.0,  trunc)
DEFINE_TRUNC_F2I(32, float,  i64,  int64_t,  (float)INT64_MIN,  (float) INT64_MAX + 1.0f, truncf)
DEFINE_TRUNC_F2I(32, float,  u64, uint64_t,                 0,  (float)UINT64_MAX + 1.0f, truncf)
DEFINE_TRUNC_F2I(64, double, i64,  int64_t, (double)INT64_MIN, (double) INT64_MAX + 1.0,  trunc)
DEFINE_TRUNC_F2I(64, double, u64, uint64_t,                 0, (double)UINT64_MAX + 1.0,  trunc)

static inline wah_error_t wah_type_stack_push(wah_type_stack_t *stack, wah_type_t type) {
    WAH_ENSURE(stack->sp < WAH_MAX_TYPE_STACK_SIZE, WAH_ERROR_VALIDATION_FAILED);
    stack->data[stack->sp++] = type;
    return WAH_OK;
}

static inline wah_error_t wah_type_stack_pop(wah_type_stack_t *stack, wah_type_t *out_type) {
    WAH_ENSURE(stack->sp > 0, WAH_ERROR_VALIDATION_FAILED);
    *out_type = stack->data[--stack->sp];
    return WAH_OK;
}

// Stack depth tracking helpers
static inline wah_error_t wah_validation_push_type(wah_validation_context_t *vctx, wah_type_t type) {
    WAH_CHECK(wah_type_stack_push(&vctx->type_stack, vctx->is_unreachable ? WAH_TYPE_ANY : type));
    vctx->current_stack_depth++;
    if (vctx->current_stack_depth > vctx->max_stack_depth) {
        vctx->max_stack_depth = vctx->current_stack_depth;
    }
    return WAH_OK;
}

static inline wah_error_t wah_validation_pop_type(wah_validation_context_t *vctx, wah_type_t *out_type) {
    if (vctx->is_unreachable) {
        *out_type = WAH_TYPE_ANY;
        // In an unreachable state, pop always succeeds conceptually, and stack height still changes.
        // We still decrement current_stack_depth to track the conceptual stack height.
        // We don't need to pop from type_stack.data as it's already filled with WAH_TYPE_ANY or ignored.
        if (vctx->current_stack_depth > 0) {
            vctx->current_stack_depth--;
        }
        return WAH_OK;
    }

    // If reachable, check for stack underflow
    WAH_ENSURE(vctx->current_stack_depth > 0, WAH_ERROR_VALIDATION_FAILED);
    WAH_CHECK(wah_type_stack_pop(&vctx->type_stack, out_type));
    vctx->current_stack_depth--;
    return WAH_OK;
}

// Helper function to validate if an actual type matches an expected type, considering WAH_TYPE_ANY
static inline wah_error_t wah_validate_type_match(wah_type_t actual, wah_type_t expected) {
    WAH_ENSURE(actual == expected || actual == WAH_TYPE_ANY, WAH_ERROR_VALIDATION_FAILED);
    return WAH_OK;
}

// Helper function to pop a type from the stack and validate it against an expected type
static inline wah_error_t wah_validation_pop_and_match_type(wah_validation_context_t *vctx, wah_type_t expected_type) {
    wah_type_t actual_type;
    WAH_CHECK(wah_validation_pop_type(vctx, &actual_type));
    return wah_validate_type_match(actual_type, expected_type);
}

// Helper to read a section header
static wah_error_t wah_read_section_header(const uint8_t **ptr, const uint8_t *end, uint8_t *id, uint32_t *size) {
    WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
    *id = *(*ptr)++;
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
        WAH_ENSURE(**ptr == 0x60, WAH_ERROR_VALIDATION_FAILED);
        (*ptr)++;

        // Parse parameter types
        uint32_t param_count_type;
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &param_count_type));

        module->types[i].param_count = param_count_type;
        WAH_MALLOC_ARRAY(module->types[i].param_types, param_count_type);
        for (uint32_t j = 0; j < param_count_type; ++j) {
            wah_type_t type;
            WAH_CHECK(wah_decode_val_type(ptr, section_end, &type));
            WAH_ENSURE(type == WAH_TYPE_I32 || type == WAH_TYPE_I64 ||
                       type == WAH_TYPE_F32 || type == WAH_TYPE_F64, WAH_ERROR_VALIDATION_FAILED);
            module->types[i].param_types[j] = type;
        }

        // Parse result types
        uint32_t result_count_type;
        WAH_CHECK(wah_decode_uleb128(ptr, section_end, &result_count_type));
        WAH_ENSURE(result_count_type <= 1, WAH_ERROR_VALIDATION_FAILED);

        module->types[i].result_count = result_count_type;
        WAH_MALLOC_ARRAY(module->types[i].result_types, result_count_type);

        for (uint32_t j = 0; j < result_count_type; ++j) {
            wah_type_t type;
            WAH_CHECK(wah_decode_val_type(ptr, section_end, &type));
            WAH_ENSURE(type == WAH_TYPE_I32 || type == WAH_TYPE_I64 ||
                       type == WAH_TYPE_F32 || type == WAH_TYPE_F64, WAH_ERROR_VALIDATION_FAILED);
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
        WAH_ENSURE(module->function_type_indices[i] < module->type_count, WAH_ERROR_VALIDATION_FAILED);
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
            WAH_ENSURE(align <= max_lg_align, WAH_ERROR_VALIDATION_FAILED); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); \
            return wah_validation_push_type(vctx, WAH_TYPE_##T); \
        }

#define STORE_OP(T, max_lg_align) { \
            uint32_t align, offset; \
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &align)); \
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &offset)); \
            WAH_ENSURE(align <= max_lg_align, WAH_ERROR_VALIDATION_FAILED); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_##T)); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); \
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
            WAH_ENSURE(mem_idx == 0, WAH_ERROR_VALIDATION_FAILED); // Only memory 0 supported
            return wah_validation_push_type(vctx, WAH_TYPE_I32); // Pushes current memory size in pages
        }
        case WAH_OP_MEMORY_GROW: {
            uint32_t mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &mem_idx)); // Expect 0x00 for memory index
            WAH_ENSURE(mem_idx == 0, WAH_ERROR_VALIDATION_FAILED); // Only memory 0 supported
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops pages to grow by (i32)
            return wah_validation_push_type(vctx, WAH_TYPE_I32); // Pushes old memory size in pages (i32)
        }
        case WAH_OP_MEMORY_FILL: {
            uint32_t mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &mem_idx)); // Expect 0x00 for memory index
            WAH_ENSURE(mem_idx == 0, WAH_ERROR_VALIDATION_FAILED); // Only memory 0 supported
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops size (i32)
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops value (i32)
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops destination address (i32)
            return WAH_OK; // Pushes nothing
        }
        case WAH_OP_MEMORY_INIT: {
            uint32_t data_idx, mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &data_idx));
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &mem_idx));
            WAH_ENSURE(mem_idx == 0, WAH_ERROR_VALIDATION_FAILED); // Only memory 0 supported
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops size (i32)
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops data segment offset (i32)
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops memory offset (i32)

            // Update min_data_segment_count_required
            if (data_idx + 1 > vctx->module->min_data_segment_count_required) {
                vctx->module->min_data_segment_count_required = data_idx + 1;
            }
            return WAH_OK;
        }
        case WAH_OP_MEMORY_COPY: {
            uint32_t dest_mem_idx, src_mem_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &dest_mem_idx));
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &src_mem_idx));
            WAH_ENSURE(dest_mem_idx == 0 && src_mem_idx == 0, WAH_ERROR_VALIDATION_FAILED); // Only memory 0 supported
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops size (i32)
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops src offset (i32)
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32)); // Pops dest offset (i32)
            return WAH_OK;
        }
        case WAH_OP_CALL: {
            uint32_t func_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &func_idx));
            WAH_ENSURE(func_idx < vctx->module->function_count, WAH_ERROR_VALIDATION_FAILED);
            // Type checking for call
            const wah_func_type_t *called_func_type = &vctx->module->types[vctx->module->function_type_indices[func_idx]];
            // Pop parameters from type stack
            for (int32_t j = called_func_type->param_count - 1; j >= 0; --j) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, called_func_type->param_types[j]));
            }
            // Push results onto type stack
            for (uint32_t j = 0; j < called_func_type->result_count; ++j) {
                WAH_CHECK(wah_validation_push_type(vctx, called_func_type->result_types[j]));
            }
            return WAH_OK;
        }
        case WAH_OP_CALL_INDIRECT: {
            uint32_t type_idx, table_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &type_idx));
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &table_idx));

            // Validate table index
            WAH_ENSURE(table_idx < vctx->module->table_count, WAH_ERROR_VALIDATION_FAILED);
            // Only funcref tables are supported for now
            WAH_ENSURE(vctx->module->tables[table_idx].elem_type == WAH_TYPE_FUNCREF, WAH_ERROR_VALIDATION_FAILED);

            // Validate type index
            WAH_ENSURE(type_idx < vctx->module->type_count, WAH_ERROR_VALIDATION_FAILED);

            // Pop function index (i32) from stack
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32));

            // Get the expected function type
            const wah_func_type_t *expected_func_type = &vctx->module->types[type_idx];

            // Pop parameters from type stack
            for (int32_t j = expected_func_type->param_count - 1; j >= 0; --j) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, expected_func_type->param_types[j]));
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

            WAH_ENSURE(local_idx < vctx->total_locals, WAH_ERROR_VALIDATION_FAILED);

            wah_type_t expected_type;
            if (local_idx < vctx->func_type->param_count) {
                expected_type = vctx->func_type->param_types[local_idx];
            } else {
                expected_type = code_body->local_types[local_idx - vctx->func_type->param_count];
            }

            if (opcode_val == WAH_OP_LOCAL_GET) {
                return wah_validation_push_type(vctx, expected_type);
            } else if (opcode_val == WAH_OP_LOCAL_SET) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, expected_type));
                return WAH_OK;
            } else { // WAH_OP_LOCAL_TEE
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, expected_type));
                return wah_validation_push_type(vctx, expected_type);
            }
            break;
        }

        case WAH_OP_GLOBAL_GET: {
            uint32_t global_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &global_idx));
            WAH_ENSURE(global_idx < vctx->module->global_count, WAH_ERROR_VALIDATION_FAILED);
            wah_type_t global_type = vctx->module->globals[global_idx].type;
            return wah_validation_push_type(vctx, global_type);
        }
        case WAH_OP_GLOBAL_SET: {
            uint32_t global_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &global_idx));
            WAH_ENSURE(global_idx < vctx->module->global_count, WAH_ERROR_VALIDATION_FAILED);
            WAH_ENSURE(vctx->module->globals[global_idx].is_mutable, WAH_ERROR_VALIDATION_FAILED); // Cannot set immutable global
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, vctx->module->globals[global_idx].type));
            return WAH_OK;
        }

        case WAH_OP_I32_CONST: {
            int32_t val;
            WAH_CHECK(wah_decode_sleb128_32(code_ptr, code_end, &val));
            return wah_validation_push_type(vctx, WAH_TYPE_I32);
        }
        case WAH_OP_I64_CONST: {
            int64_t val;
            WAH_CHECK(wah_decode_sleb128_64(code_ptr, code_end, &val));
            return wah_validation_push_type(vctx, WAH_TYPE_I64);
        }
        case WAH_OP_F32_CONST: {
            WAH_ENSURE(code_end - *code_ptr >= 4, WAH_ERROR_UNEXPECTED_EOF);
            *code_ptr += 4;
            return wah_validation_push_type(vctx, WAH_TYPE_F32);
        }
        case WAH_OP_F64_CONST: {
            WAH_ENSURE(code_end - *code_ptr >= 8, WAH_ERROR_UNEXPECTED_EOF);
            *code_ptr += 8;
            return wah_validation_push_type(vctx, WAH_TYPE_F64);
        }

#define NUM_OPS(N) \
        /* Binary i32/i64 operations */ \
        case WAH_OP_I##N##_ADD: case WAH_OP_I##N##_SUB: case WAH_OP_I##N##_MUL: \
        case WAH_OP_I##N##_DIV_S: case WAH_OP_I##N##_DIV_U: case WAH_OP_I##N##_REM_S: case WAH_OP_I##N##_REM_U: \
        case WAH_OP_I##N##_AND: case WAH_OP_I##N##_OR: case WAH_OP_I##N##_XOR: \
        case WAH_OP_I##N##_SHL: case WAH_OP_I##N##_SHR_S: case WAH_OP_I##N##_SHR_U: { \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I##N)); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I##N)); \
            return wah_validation_push_type(vctx, WAH_TYPE_I##N); \
        } \
        case WAH_OP_I##N##_EQ: case WAH_OP_I##N##_NE: \
        case WAH_OP_I##N##_LT_S: case WAH_OP_I##N##_LT_U: case WAH_OP_I##N##_GT_S: case WAH_OP_I##N##_GT_U: \
        case WAH_OP_I##N##_LE_S: case WAH_OP_I##N##_LE_U: case WAH_OP_I##N##_GE_S: case WAH_OP_I##N##_GE_U: { \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I##N)); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I##N)); \
            return wah_validation_push_type(vctx, WAH_TYPE_I32); /* Comparisons return i32 */ \
        } \
        \
        /* Unary i32/i64 operations */ \
        case WAH_OP_I##N##_EQZ: { \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I##N)); \
            return wah_validation_push_type(vctx, WAH_TYPE_I32); \
        } \
        \
        /* Binary f32/f64 operations */ \
        case WAH_OP_F##N##_ADD: case WAH_OP_F##N##_SUB: case WAH_OP_F##N##_MUL: case WAH_OP_F##N##_DIV: { \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_F##N)); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_F##N)); \
            return wah_validation_push_type(vctx, WAH_TYPE_F##N); \
        } \
        case WAH_OP_F##N##_EQ: case WAH_OP_F##N##_NE: \
        case WAH_OP_F##N##_LT: case WAH_OP_F##N##_GT: case WAH_OP_F##N##_LE: case WAH_OP_F##N##_GE: { \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_F##N)); \
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_F##N)); \
            return wah_validation_push_type(vctx, WAH_TYPE_I32); /* Comparisons return i32 */ \
        }

        NUM_OPS(32)
        NUM_OPS(64)

#undef NUM_OPS

#define UNARY_OP(T) { \
    WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_##T)); \
    return wah_validation_push_type(vctx, WAH_TYPE_##T); \
}

#define BINARY_OP(T) { \
    WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_##T)); \
    WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_##T)); \
    return wah_validation_push_type(vctx, WAH_TYPE_##T); \
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
    WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_##INPUT_TYPE)); \
    return wah_validation_push_type(vctx, WAH_TYPE_##OUTPUT_TYPE); \
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
            wah_type_t type;
            return wah_validation_pop_type(vctx, &type);
        }

        case WAH_OP_SELECT: {
            wah_type_t b_type, a_type;
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32));
            WAH_CHECK(wah_validation_pop_type(vctx, &b_type));
            WAH_CHECK(wah_validation_pop_type(vctx, &a_type));
            // If a_type and b_type are different, and neither is ANY, then it's an error.
            WAH_ENSURE(a_type == b_type || a_type == WAH_TYPE_ANY || b_type == WAH_TYPE_ANY, WAH_ERROR_VALIDATION_FAILED);
            // If either is ANY, the result is ANY. Otherwise, it's a_type (which equals b_type).
            if (a_type == WAH_TYPE_ANY || b_type == WAH_TYPE_ANY) {
                return wah_validation_push_type(vctx, WAH_TYPE_ANY);
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
            wah_type_t cond_type;
            if (opcode_val == WAH_OP_IF) {
                WAH_CHECK(wah_validation_pop_type(vctx, &cond_type));
                WAH_CHECK(wah_validate_type_match(cond_type, WAH_TYPE_I32));
            }

            int32_t block_type_val;
            WAH_CHECK(wah_decode_sleb128_32(code_ptr, code_end, &block_type_val));

            WAH_ENSURE(vctx->control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED);
            wah_validation_control_frame_t* frame = &vctx->control_stack[vctx->control_sp++];
            frame->opcode = (wah_opcode_t)opcode_val;
            frame->else_found = false;
            frame->is_unreachable = vctx->is_unreachable; // Initialize with current reachability
            frame->stack_height = vctx->current_stack_depth; // Store current stack height

            wah_func_type_t* bt = &frame->block_type;
            memset(bt, 0, sizeof(wah_func_type_t));

            if (block_type_val < 0) { // Value type
                wah_type_t result_type = 0; // Initialize to a default invalid value
                switch(block_type_val) {
                    case -1: result_type = WAH_TYPE_I32; break;
                    case -2: result_type = WAH_TYPE_I64; break;
                    case -3: result_type = WAH_TYPE_F32; break;
                    case -4: result_type = WAH_TYPE_F64; break;
                    case -0x40: break; // empty
                    default: return WAH_ERROR_VALIDATION_FAILED;
                }

                if (result_type != 0) { // If not empty
                    bt->result_count = 1;
                    WAH_MALLOC_ARRAY(bt->result_types, 1);
                    bt->result_types[0] = result_type;
                }
            } else { // Function type index
                uint32_t type_idx = (uint32_t)block_type_val;
                WAH_ENSURE(type_idx < vctx->module->type_count, WAH_ERROR_VALIDATION_FAILED);
                const wah_func_type_t* referenced_type = &vctx->module->types[type_idx];

                bt->param_count = referenced_type->param_count;
                if (bt->param_count > 0) {
                    WAH_MALLOC_ARRAY(bt->param_types, bt->param_count);
                    memcpy(bt->param_types, referenced_type->param_types, sizeof(wah_type_t) * bt->param_count);
                }

                bt->result_count = referenced_type->result_count;
                if (bt->result_count > 0) {
                    WAH_MALLOC_ARRAY(bt->result_types, bt->result_count);
                    memcpy(bt->result_types, referenced_type->result_types, sizeof(wah_type_t) * bt->result_count);
                }
            }

            // Pop params
            for (int32_t i = bt->param_count - 1; i >= 0; --i) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, bt->param_types[i]));
            }

            frame->type_stack_sp = vctx->type_stack.sp;
            return WAH_OK;
        }
        case WAH_OP_ELSE: {
            WAH_ENSURE(vctx->control_sp > 0, WAH_ERROR_VALIDATION_FAILED);
            wah_validation_control_frame_t* frame = &vctx->control_stack[vctx->control_sp - 1];
            WAH_ENSURE(frame->opcode == WAH_OP_IF && !frame->else_found, WAH_ERROR_VALIDATION_FAILED);
            frame->else_found = true;

            // Pop results of 'if' branch and verify
            for (int32_t i = frame->block_type.result_count - 1; i >= 0; --i) {
                // If the stack was unreachable, any type is fine. Otherwise, it must match.
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, frame->block_type.result_types[i]));
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
                    WAH_ENSURE(vctx->type_stack.sp == 0, WAH_ERROR_VALIDATION_FAILED);
                } else { // result_count == 1
                    WAH_ENSURE(vctx->type_stack.sp == 1, WAH_ERROR_VALIDATION_FAILED);
                    WAH_CHECK(wah_validation_pop_and_match_type(vctx, vctx->func_type->result_types[0]));
                }
                // Reset unreachable state for the function's end
                vctx->is_unreachable = false;
                return WAH_OK;
            }

            wah_validation_control_frame_t* frame = &vctx->control_stack[vctx->control_sp - 1];

            // Pop results from the executed branch and verify
            for (int32_t i = frame->block_type.result_count - 1; i >= 0; --i) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, frame->block_type.result_types[i]));
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
            WAH_ENSURE(label_idx < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED);

            wah_validation_control_frame_t *target_frame = &vctx->control_stack[vctx->control_sp - 1 - label_idx];

            // Pop the expected result types of the target block from the current stack
            // The stack must contain these values for the branch to be valid.
            for (int32_t i = target_frame->block_type.result_count - 1; i >= 0; --i) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, target_frame->block_type.result_types[i]));
            }

            // Discard any remaining values on the stack above the target frame's stack height
            while (vctx->current_stack_depth > target_frame->stack_height) {
                wah_type_t temp_type;
                WAH_CHECK(wah_validation_pop_type(vctx, &temp_type));
            }

            vctx->is_unreachable = true; // br makes the current path unreachable
            return WAH_OK;
        }

        case WAH_OP_BR_IF: {
            uint32_t label_idx;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &label_idx));
            WAH_ENSURE(label_idx < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED);

            // Pop condition (i32) from stack for br_if
            WAH_CHECK(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32));

            // If the current path is unreachable, the stack is polymorphic, so type checks are trivial.
            // We only need to ensure the conceptual stack height is maintained.
            if (vctx->is_unreachable) {
                return WAH_OK;
            }

            wah_validation_control_frame_t *target_frame = &vctx->control_stack[vctx->control_sp - 1 - label_idx];

            // Check if there are enough values on the stack for the target block's results
            // (which become parameters to the target block if branched to).
            WAH_ENSURE(vctx->current_stack_depth >= target_frame->block_type.result_count, WAH_ERROR_VALIDATION_FAILED);

            // Check the types of these values without popping them
            for (uint32_t i = 0; i < target_frame->block_type.result_count; ++i) {
                // Access the type stack directly to peek at values
                wah_type_t actual_type = vctx->type_stack.data[vctx->type_stack.sp - target_frame->block_type.result_count + i];
                WAH_CHECK(wah_validate_type_match(actual_type, target_frame->block_type.result_types[i]));
            }

            // The stack state for the fall-through path is now correct (condition popped).
            // The current path remains reachable.
            return WAH_OK;
        }

        case WAH_OP_BR_TABLE: {
            wah_error_t err = WAH_OK; // Declare err here for goto cleanup
            uint32_t num_targets;
            WAH_CHECK(wah_decode_uleb128(code_ptr, code_end, &num_targets));

            // Store all label_idx values to process them after decoding all
            uint32_t* label_indices = NULL;
            WAH_MALLOC_ARRAY_GOTO(label_indices, num_targets + 1, cleanup_br_table); // +1 for default target

            // Decode target table (vector of label_idx)
            for (uint32_t i = 0; i < num_targets; ++i) {
                WAH_CHECK_GOTO(wah_decode_uleb128(code_ptr, code_end, &label_indices[i]), cleanup_br_table);
                WAH_ENSURE_GOTO(label_indices[i] < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED, cleanup_br_table);
            }

            // Decode default target (label_idx)
            WAH_CHECK_GOTO(wah_decode_uleb128(code_ptr, code_end, &label_indices[num_targets]), cleanup_br_table); // Last element is default
            WAH_ENSURE_GOTO(label_indices[num_targets] < vctx->control_sp, WAH_ERROR_VALIDATION_FAILED, cleanup_br_table);

            // Pop index (i32) from stack
            WAH_CHECK_GOTO(wah_validation_pop_and_match_type(vctx, WAH_TYPE_I32), cleanup_br_table);

            // Get the block type of the default target as the reference
            wah_validation_control_frame_t *default_target_frame = &vctx->control_stack[vctx->control_sp - 1 - label_indices[num_targets]];
            const wah_func_type_t *expected_block_type = &default_target_frame->block_type;

            // Check type consistency for all targets
            for (uint32_t i = 0; i < num_targets + 1; ++i) {
                wah_validation_control_frame_t *current_target_frame = &vctx->control_stack[vctx->control_sp - 1 - label_indices[i]];
                const wah_func_type_t *current_block_type = &current_target_frame->block_type;

                // All target blocks must have the same result count and types
                WAH_ENSURE_GOTO(current_block_type->result_count == expected_block_type->result_count, WAH_ERROR_VALIDATION_FAILED, cleanup_br_table);
                for (uint32_t j = 0; j < expected_block_type->result_count; ++j) {
                    WAH_ENSURE_GOTO(current_block_type->result_types[j] == expected_block_type->result_types[j], WAH_ERROR_VALIDATION_FAILED, cleanup_br_table);
                }
            }

            // Pop the expected result types of the target block from the current stack
            // The stack must contain these values for the branch to be valid.
            for (int32_t i = expected_block_type->result_count - 1; i >= 0; --i) {
                WAH_CHECK_GOTO(wah_validation_pop_and_match_type(vctx, expected_block_type->result_types[i]), cleanup_br_table);
            }

            // Discard any remaining values on the stack above the target frame's stack height
            while (vctx->current_stack_depth > default_target_frame->stack_height) {
                wah_type_t temp_type;
                WAH_CHECK_GOTO(wah_validation_pop_type(vctx, &temp_type), cleanup_br_table);
            }

            vctx->is_unreachable = true; // br_table makes the current path unreachable
            err = WAH_OK; // Set err to WAH_OK before cleanup
        cleanup_br_table:
            free(label_indices);
            return err;
        }

        case WAH_OP_RETURN:
            // Pop the function's result types from the stack
            for (int32_t j = vctx->func_type->result_count - 1; j >= 0; --j) {
                WAH_CHECK(wah_validation_pop_and_match_type(vctx, vctx->func_type->result_types[j]));
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
    WAH_ENSURE_GOTO(count == module->function_count, WAH_ERROR_VALIDATION_FAILED, cleanup);
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
            WAH_ENSURE_GOTO(UINT32_MAX - current_local_count >= local_type_count, WAH_ERROR_TOO_LARGE, cleanup);
            current_local_count += local_type_count;
        }
        module->code_bodies[i].local_count = current_local_count;
        WAH_MALLOC_ARRAY_GOTO(module->code_bodies[i].local_types, current_local_count, cleanup);

        uint32_t local_idx = 0;
        for (uint32_t j = 0; j < num_local_entries; ++j) {
            uint32_t local_type_count;
            WAH_CHECK_GOTO(wah_decode_uleb128(ptr, code_body_end, &local_type_count), cleanup);
            wah_type_t type;
            WAH_CHECK_GOTO(wah_decode_val_type(ptr, code_body_end, &type), cleanup);
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
                        WAH_ENSURE_GOTO(vctx.type_stack.sp == 0, WAH_ERROR_VALIDATION_FAILED, cleanup); // Unmatched control frames
                    } else { // result_count == 1
                        // If unreachable, the stack is polymorphic, so we don't strictly check sp.
                        // We still pop to ensure the conceptual stack height is correct.
                        WAH_CHECK_GOTO(wah_validation_pop_and_match_type(&vctx, vctx.func_type->result_types[0]), cleanup);
                    }
                    break; // End of validation loop
                }
            }
            WAH_CHECK_GOTO(wah_validate_opcode(current_opcode_val, &code_ptr_validation, validation_end, &vctx, &module->code_bodies[i]), cleanup);
        }
        WAH_ENSURE_GOTO(vctx.control_sp == 0, WAH_ERROR_VALIDATION_FAILED, cleanup);
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
        wah_type_t global_declared_type;
        WAH_CHECK(wah_decode_val_type(ptr, section_end, &global_declared_type));
        module->globals[i].type = global_declared_type;

        // Mutability
        WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
        module->globals[i].is_mutable = (*(*ptr)++ == 1);

        // Init Expr (only const expressions are supported for initial values)
        WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
        wah_opcode_t opcode = (wah_opcode_t)*(*ptr)++;
        switch (opcode) {
            case WAH_OP_I32_CONST: {
                WAH_ENSURE(global_declared_type == WAH_TYPE_I32, WAH_ERROR_VALIDATION_FAILED);
                int32_t val;
                WAH_CHECK(wah_decode_sleb128_32(ptr, section_end, &val));
                module->globals[i].initial_value.i32 = val;
                break;
            }
            case WAH_OP_I64_CONST: {
                WAH_ENSURE(global_declared_type == WAH_TYPE_I64, WAH_ERROR_VALIDATION_FAILED);
                int64_t val;
                WAH_CHECK(wah_decode_sleb128_64(ptr, section_end, &val));
                module->globals[i].initial_value.i64 = val;
                break;
            }
            case WAH_OP_F32_CONST: {
                WAH_ENSURE(global_declared_type == WAH_TYPE_F32, WAH_ERROR_VALIDATION_FAILED);
                WAH_ENSURE(*ptr + 4 <= section_end, WAH_ERROR_UNEXPECTED_EOF);
                module->globals[i].initial_value.f32 = wah_canonicalize_f32(wah_read_f32_le(*ptr));
                *ptr += 4;
                break;
            }
            case WAH_OP_F64_CONST: {
                WAH_ENSURE(global_declared_type == WAH_TYPE_F64, WAH_ERROR_VALIDATION_FAILED);
                WAH_ENSURE(*ptr + 8 <= section_end, WAH_ERROR_UNEXPECTED_EOF);
                module->globals[i].initial_value.f64 = wah_canonicalize_f64(wah_read_f64_le(*ptr));
                *ptr += 8;
                break;
            }
            default: {
                // Only const expressions supported for global initializers for now
                return WAH_ERROR_VALIDATION_FAILED;
            }
        }
        WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
        WAH_ENSURE(*(*ptr)++ == WAH_OP_END, WAH_ERROR_VALIDATION_FAILED); // Expect END opcode after init_expr
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
            WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
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
            wah_type_t elem_type;
            WAH_CHECK(wah_decode_ref_type(ptr, section_end, &elem_type));
            module->tables[i].elem_type = elem_type;
            WAH_ENSURE(elem_type == WAH_TYPE_FUNCREF, WAH_ERROR_VALIDATION_FAILED); // Only funcref is supported for now

            WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
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
    wah_error_t err = WAH_OK;
    uint32_t count;
    WAH_CHECK_GOTO(wah_decode_uleb128(ptr, section_end, &count), cleanup);

    module->export_count = count;
    WAH_MALLOC_ARRAY_GOTO(module->exports, count, cleanup);
    memset(module->exports, 0, sizeof(wah_export_t) * count);

    for (uint32_t i = 0; i < count; ++i) {
        wah_export_t *export_entry = &module->exports[i];

        // Name length
        uint32_t name_len;
        WAH_CHECK_GOTO(wah_decode_uleb128(ptr, section_end, &name_len), cleanup);
        export_entry->name_len = name_len;

        // Name string
        WAH_ENSURE_GOTO(*ptr + name_len <= section_end, WAH_ERROR_UNEXPECTED_EOF, cleanup);

        // Allocate memory for the name and copy it, ensuring null-termination
        WAH_MALLOC_ARRAY_GOTO(export_entry->name, name_len + 1, cleanup);
        memcpy((void*)export_entry->name, *ptr, name_len);
        ((char*)export_entry->name)[name_len] = '\0';

        WAH_ENSURE_GOTO(wah_is_valid_utf8(export_entry->name, export_entry->name_len), WAH_ERROR_VALIDATION_FAILED, cleanup);

        // Check for duplicate export names
        for (uint32_t j = 0; j < i; ++j) {
            if (module->exports[j].name_len == export_entry->name_len &&
                strncmp(module->exports[j].name, export_entry->name, export_entry->name_len) == 0) {
                err = WAH_ERROR_VALIDATION_FAILED; // Duplicate export name
                goto cleanup;
            }
        }

        *ptr += name_len;

        // Export kind
        WAH_ENSURE_GOTO(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF, cleanup);
        export_entry->kind = *(*ptr)++;

        // Export index
        WAH_CHECK_GOTO(wah_decode_uleb128(ptr, section_end, &export_entry->index), cleanup);

        // Basic validation of index based on kind
        switch (export_entry->kind) {
            case 0: // Function
                WAH_ENSURE_GOTO(export_entry->index < module->function_count, WAH_ERROR_VALIDATION_FAILED, cleanup);
                break;
            case 1: // Table
                WAH_ENSURE_GOTO(export_entry->index < module->table_count, WAH_ERROR_VALIDATION_FAILED, cleanup);
                break;
            case 2: // Memory
                WAH_ENSURE_GOTO(export_entry->index < module->memory_count, WAH_ERROR_VALIDATION_FAILED, cleanup);
                break;
            case 3: // Global
                WAH_ENSURE_GOTO(export_entry->index < module->global_count, WAH_ERROR_VALIDATION_FAILED, cleanup);
                break;
            default:
                err = WAH_ERROR_VALIDATION_FAILED; // Unknown export kind
                goto cleanup;
        }
    }

cleanup:
    if (err != WAH_OK) {
        if (module->exports) {
            // Free names that were already allocated
            for (uint32_t k = 0; k < count; ++k) {
                if (module->exports[k].name) {
                    free((void*)module->exports[k].name);
                }
            }
            free(module->exports);
            module->exports = NULL;
            module->export_count = 0;
        }
    }
    return err;
}

static wah_error_t wah_parse_start_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->start_function_idx));
    WAH_ENSURE(module->start_function_idx < module->function_count, WAH_ERROR_VALIDATION_FAILED);
    module->has_start_function = true;
    return WAH_OK;
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
            WAH_ENSURE(segment->table_idx == 0, WAH_ERROR_VALIDATION_FAILED); // For now, only table 0 is supported

            // Parse offset_expr (expected to be i32.const X end)
            WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
            wah_opcode_t opcode = (wah_opcode_t)*(*ptr)++;
            WAH_ENSURE(opcode == WAH_OP_I32_CONST, WAH_ERROR_VALIDATION_FAILED);

            int32_t offset_val;
            WAH_CHECK(wah_decode_sleb128_32(ptr, section_end, &offset_val));
            segment->offset = (uint32_t)offset_val;

            WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
            WAH_ENSURE(*(*ptr)++ == WAH_OP_END, WAH_ERROR_VALIDATION_FAILED);

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->num_elems));

            // Validate that the segment fits within the table's limits
            WAH_ENSURE(segment->table_idx < module->table_count, WAH_ERROR_VALIDATION_FAILED);
            WAH_ENSURE((uint64_t)segment->offset + segment->num_elems <= module->tables[segment->table_idx].min_elements, WAH_ERROR_VALIDATION_FAILED);

            if (segment->num_elems > 0) {
                WAH_MALLOC_ARRAY(segment->func_indices, segment->num_elems);
                for (uint32_t j = 0; j < segment->num_elems; ++j) {
                    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->func_indices[j]));
                    WAH_ENSURE(segment->func_indices[j] < module->function_count, WAH_ERROR_VALIDATION_FAILED);
                }
            }
        }
    }
    return WAH_OK;
}

static wah_error_t wah_parse_data_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    uint32_t count;
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &count));

    // If a datacount section was present, validate that the data section count matches.
    // Otherwise, set the data_segment_count from this section.
    if (module->has_data_count_section) {
        WAH_ENSURE(count == module->data_segment_count, WAH_ERROR_VALIDATION_FAILED);
    } else {
        module->data_segment_count = count;
    }

    if (count > 0) {
        WAH_MALLOC_ARRAY(module->data_segments, count);
        memset(module->data_segments, 0, sizeof(wah_data_segment_t) * count);

        for (uint32_t i = 0; i < count; ++i) {
            wah_data_segment_t *segment = &module->data_segments[i];

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->flags));

            if (segment->flags == 0x00) { // Active segment, memory index 0
                segment->memory_idx = 0;
            } else if (segment->flags == 0x01) { // Passive segment
                // No memory index or offset expression for passive segments
            } else if (segment->flags == 0x02) { // Active segment, with memory index
                WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->memory_idx));
                WAH_ENSURE(segment->memory_idx == 0, WAH_ERROR_VALIDATION_FAILED); // Only memory 0 supported
            } else {
                return WAH_ERROR_VALIDATION_FAILED; // Unknown data segment flags
            }

            if (segment->flags == 0x00 || segment->flags == 0x02) { // Active segments have offset expression
                // Parse offset_expr (expected to be i32.const X end)
                WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
                wah_opcode_t opcode = (wah_opcode_t)*(*ptr)++;
                WAH_ENSURE(opcode == WAH_OP_I32_CONST, WAH_ERROR_VALIDATION_FAILED);

                int32_t offset_val;
                WAH_CHECK(wah_decode_sleb128_32(ptr, section_end, &offset_val));
                segment->offset = (uint32_t)offset_val;

                WAH_ENSURE(*ptr < section_end, WAH_ERROR_UNEXPECTED_EOF);
                WAH_ENSURE(*(*ptr)++ == WAH_OP_END, WAH_ERROR_VALIDATION_FAILED);
            }

            WAH_CHECK(wah_decode_uleb128(ptr, section_end, &segment->data_len));

            WAH_ENSURE(*ptr + segment->data_len <= section_end, WAH_ERROR_UNEXPECTED_EOF);
            segment->data = *ptr;
            *ptr += segment->data_len;
        }
    }
    return WAH_OK;
}

static wah_error_t wah_parse_datacount_section(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module) {
    WAH_CHECK(wah_decode_uleb128(ptr, section_end, &module->data_segment_count));
    module->has_data_count_section = true;
    return WAH_OK;
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
                WAH_ENSURE_GOTO(control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED, cleanup);
                uint32_t target_idx = block_target_count++;
                block_targets[target_idx] = preparsed_size; // To be overwritten for WAH_OP_IF
                control_stack[control_sp++] = (wah_control_frame_t){.opcode=(wah_opcode_t)opcode, .target_idx=target_idx};
                preparsed_instr_size = (opcode == WAH_OP_IF) ? sizeof(uint16_t) + sizeof(uint32_t) : 0;
                break;
            }
            case WAH_OP_ELSE: {
                WAH_ENSURE_GOTO(control_sp > 0 && control_stack[control_sp - 1].opcode == WAH_OP_IF, WAH_ERROR_VALIDATION_FAILED, cleanup);
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
            case WAH_OP_MEMORY_INIT: {
                uint32_t data_idx, mem_idx;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &data_idx), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &mem_idx), cleanup);
                preparsed_instr_size += sizeof(uint32_t) * 2; // For data_idx and mem_idx
                break;
            }
            case WAH_OP_MEMORY_COPY: {
                uint32_t dest_mem_idx, src_mem_idx;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &dest_mem_idx), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &src_mem_idx), cleanup);
                preparsed_instr_size += sizeof(uint32_t) * 2; // For dest_mem_idx and src_mem_idx
                break;
            }
        }
        preparsed_size += preparsed_instr_size;
        ptr = instr_ptr + (ptr - instr_ptr); // This is not a bug; it's to make it clear that ptr is advanced inside the switch
    }
    WAH_ENSURE_GOTO(control_sp == 0, WAH_ERROR_VALIDATION_FAILED, cleanup);

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
            WAH_ENSURE_GOTO(control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED, cleanup);
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
                WAH_ENSURE_GOTO(control_sp < WAH_MAX_CONTROL_DEPTH, WAH_ERROR_VALIDATION_FAILED, cleanup);
                control_stack[control_sp++] = (wah_control_frame_t){.opcode=WAH_OP_IF, .target_idx=current_block_idx};
                wah_write_u32_le(write_ptr, block_targets[current_block_idx++]);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_ELSE: {
                WAH_ENSURE_GOTO(control_sp > 0 && control_stack[control_sp - 1].opcode == WAH_OP_IF, WAH_ERROR_VALIDATION_FAILED, cleanup);
                control_stack[control_sp - 1] = (wah_control_frame_t){.opcode=WAH_OP_ELSE, .target_idx=current_block_idx};
                wah_write_u32_le(write_ptr, block_targets[current_block_idx++]);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_BR: case WAH_OP_BR_IF: {
                uint32_t relative_depth;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &relative_depth), cleanup);
                WAH_ENSURE_GOTO(relative_depth < control_sp, WAH_ERROR_VALIDATION_FAILED, cleanup);
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
                    WAH_ENSURE_GOTO(relative_depth < control_sp, WAH_ERROR_VALIDATION_FAILED, cleanup);
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
            case WAH_OP_MEMORY_INIT: {
                uint32_t data_idx, mem_idx;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &data_idx), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &mem_idx), cleanup);
                wah_write_u32_le(write_ptr, data_idx);
                write_ptr += sizeof(uint32_t);
                wah_write_u32_le(write_ptr, mem_idx);
                write_ptr += sizeof(uint32_t);
                break;
            }
            case WAH_OP_MEMORY_COPY: {
                uint32_t dest_mem_idx, src_mem_idx;
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &dest_mem_idx), cleanup);
                WAH_CHECK_GOTO(wah_decode_uleb128(&ptr, end, &src_mem_idx), cleanup);
                wah_write_u32_le(write_ptr, dest_mem_idx);
                write_ptr += sizeof(uint32_t);
                wah_write_u32_le(write_ptr, src_mem_idx);
                write_ptr += sizeof(uint32_t);
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
    WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
    uint8_t first_byte = *(*ptr)++;

    if (first_byte > 0xF0) { // Multi-byte opcode prefix is remapped: F1 23 -> 1023, FC DEF -> CDEF
        uint32_t sub_opcode_val;
        WAH_CHECK(wah_decode_uleb128(ptr, end, &sub_opcode_val));
        WAH_ENSURE(sub_opcode_val < 0x1000, WAH_ERROR_VALIDATION_FAILED); // Sub-opcode out of expected range
        *opcode_val = (uint16_t)(((first_byte & 0x0F) << 12) | sub_opcode_val);
    } else {
        *opcode_val = (uint16_t)first_byte;
    }
    return WAH_OK;
}

// Helper function to decode a raw byte representing a value type into a wah_type_t
static wah_error_t wah_decode_val_type(const uint8_t **ptr, const uint8_t *end, wah_type_t *out_type) {
    WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
    uint8_t byte = *(*ptr)++;
    switch (byte) {
        case 0x7F: *out_type = WAH_TYPE_I32; return WAH_OK;
        case 0x7E: *out_type = WAH_TYPE_I64; return WAH_OK;
        case 0x7D: *out_type = WAH_TYPE_F32; return WAH_OK;
        case 0x7C: *out_type = WAH_TYPE_F64; return WAH_OK;
        default: return WAH_ERROR_VALIDATION_FAILED; // Unknown value type
    }
}

// Helper function to decode a raw byte representing a reference type into a wah_type_t
static wah_error_t wah_decode_ref_type(const uint8_t **ptr, const uint8_t *end, wah_type_t *out_type) {
    WAH_ENSURE(*ptr < end, WAH_ERROR_UNEXPECTED_EOF);
    uint8_t byte = *(*ptr)++;
    switch (byte) {
        case 0x70: *out_type = WAH_TYPE_FUNCREF; return WAH_OK;
        default: return WAH_ERROR_VALIDATION_FAILED; // Unknown value type
    }
}

// Global array of section handlers, indexed by the section ID
static const struct wah_section_handler_s {
    int8_t order; // Expected order of the section (0 for custom, 1 for Type, etc.)
    wah_error_t (*parser_func)(const uint8_t **ptr, const uint8_t *section_end, wah_module_t *module);
} wah_section_handlers[] = {
    [0]  = { .order = 0,  .parser_func = wah_parse_custom_section },
    [1]  = { .order = 1,  .parser_func = wah_parse_type_section },
    [2]  = { .order = 2,  .parser_func = wah_parse_import_section },
    [3]  = { .order = 3,  .parser_func = wah_parse_function_section },
    [4]  = { .order = 4,  .parser_func = wah_parse_table_section },
    [5]  = { .order = 5,  .parser_func = wah_parse_memory_section },
    [6]  = { .order = 6,  .parser_func = wah_parse_global_section },
    [7]  = { .order = 7,  .parser_func = wah_parse_export_section },
    [8]  = { .order = 8,  .parser_func = wah_parse_start_section },
    [9]  = { .order = 9,  .parser_func = wah_parse_element_section },
    [12] = { .order = 10, .parser_func = wah_parse_datacount_section },
    [10] = { .order = 11, .parser_func = wah_parse_code_section },
    [11] = { .order = 12, .parser_func = wah_parse_data_section },
};

wah_error_t wah_parse_module(const uint8_t *wasm_binary, size_t binary_size, wah_module_t *module) {
    wah_error_t err = WAH_OK;
    WAH_ENSURE(wasm_binary && module && binary_size >= 8, WAH_ERROR_UNEXPECTED_EOF);

    memset(module, 0, sizeof(wah_module_t)); // Initialize module struct

    const uint8_t *ptr = wasm_binary;
    const uint8_t *end = wasm_binary + binary_size;

    // For section order validation
    int8_t last_parsed_order = 0; // Start with 0, as Type section is 1. Custom sections are 0 in map.

    // 1. Check Magic Number
    uint32_t magic = wah_read_u32_le(ptr);
    ptr += 4;
    WAH_ENSURE(magic == 0x6D736100, WAH_ERROR_INVALID_MAGIC_NUMBER);

    // 2. Check Version
    WAH_ENSURE(ptr + 4 <= end, WAH_ERROR_UNEXPECTED_EOF);
    uint32_t version = wah_read_u32_le(ptr);
    ptr += 4;
    WAH_ENSURE(version == 0x01, WAH_ERROR_INVALID_VERSION);

    // 3. Parse Sections
    while (ptr < end) {
        uint8_t section_id;
        uint32_t section_size;
        WAH_CHECK_GOTO(wah_read_section_header(&ptr, end, &section_id, &section_size), cleanup_parse);

        // Get section handler from lookup table
        WAH_ENSURE_GOTO(section_id < sizeof(wah_section_handlers) / sizeof(*wah_section_handlers), WAH_ERROR_UNKNOWN_SECTION, cleanup_parse);
        const struct wah_section_handler_s *handler = &wah_section_handlers[section_id];

        // Section order validation
        if (section_id != 0) { // Custom sections do not affect the order
            WAH_ENSURE_GOTO(handler->order > last_parsed_order, WAH_ERROR_VALIDATION_FAILED, cleanup_parse);
            last_parsed_order = handler->order;
        }

        const uint8_t *section_payload_end = ptr + section_size;
        WAH_ENSURE_GOTO(section_payload_end <= end, WAH_ERROR_UNEXPECTED_EOF, cleanup_parse);

        WAH_LOG("Parsing section ID: %d, size: %u", section_id, section_size);
        WAH_CHECK_GOTO(handler->parser_func(&ptr, section_payload_end, module), cleanup_parse);

        // Ensure we consumed exactly the section_size bytes
        WAH_ENSURE_GOTO(ptr == section_payload_end, WAH_ERROR_VALIDATION_FAILED, cleanup_parse); // Indicate a parsing error within the section
    }

    // After all sections are parsed, validate that function_count matches code_count
    WAH_ENSURE_GOTO(module->function_count == module->code_count, WAH_ERROR_VALIDATION_FAILED, cleanup_parse);

    // Validate data segment references
    WAH_ENSURE_GOTO(module->data_segment_count >= module->min_data_segment_count_required, WAH_ERROR_VALIDATION_FAILED, cleanup_parse);

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

    // Initialize active data segments
    for (uint32_t i = 0; i < module->data_segment_count; ++i) {
        const wah_data_segment_t *segment = &module->data_segments[i];
        if (segment->flags == 0x00 || segment->flags == 0x02) { // Active segments
            WAH_ENSURE_GOTO(segment->memory_idx == 0, WAH_ERROR_VALIDATION_FAILED, cleanup); // Only memory 0 supported
            WAH_ENSURE_GOTO((uint64_t)segment->offset + segment->data_len <= exec_ctx->memory_size, WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup);
            memcpy(exec_ctx->memory_base + segment->offset, segment->data, segment->data_len);
        }
    }

    // If a start function is defined, call it.
    if (module->has_start_function) {
        WAH_CHECK_GOTO(wah_call(exec_ctx, module, module->start_function_idx, NULL, 0, NULL), cleanup);
    }

    return WAH_OK;

cleanup:
    if (err != WAH_OK) wah_exec_context_destroy(exec_ctx);
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
    WAH_ENSURE(ctx->call_depth < ctx->max_call_depth, WAH_ERROR_CALL_STACK_OVERFLOW);

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

                WAH_CHECK(wah_push_frame(ctx, called_func_idx, new_locals_offset));

                uint32_t num_locals = called_code->local_count;
                if (num_locals > 0) {
                    WAH_ENSURE_GOTO(ctx->sp + num_locals <= ctx->value_stack_capacity, WAH_ERROR_CALL_STACK_OVERFLOW, cleanup);
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
                WAH_ENSURE_GOTO(table_idx < ctx->table_count, WAH_ERROR_TRAP, cleanup); // Table index out of bounds

                // Validate func_table_idx against table size, Use min_elements as current size <- ????
                WAH_ENSURE_GOTO(func_table_idx < ctx->module->tables[table_idx].min_elements, WAH_ERROR_TRAP, cleanup); // Function index out of table bounds

                // Validate actual_func_idx against module's function count
                uint32_t actual_func_idx = (uint32_t)ctx->tables[table_idx][func_table_idx].i32;
                WAH_ENSURE_GOTO(actual_func_idx < ctx->module->function_count, WAH_ERROR_TRAP, cleanup); // Invalid function index in table

                // Get expected function type (from instruction)
                const wah_func_type_t *expected_func_type = &ctx->module->types[type_idx];
                // Get actual function type (from module's function type indices) (function_type_indices stores type_idx for each function)
                const wah_func_type_t *actual_func_type = &ctx->module->types[ctx->module->function_type_indices[actual_func_idx]];

                // Type check: compare expected and actual function types
                WAH_ENSURE_GOTO(expected_func_type->param_count == actual_func_type->param_count &&
                                expected_func_type->result_count == actual_func_type->result_count,
                                WAH_ERROR_TRAP, cleanup); // Type mismatch (param/result count)
                for (uint32_t i = 0; i < expected_func_type->param_count; ++i) {
                    // Type mismatch (param type)
                    WAH_ENSURE_GOTO(expected_func_type->param_types[i] == actual_func_type->param_types[i], WAH_ERROR_TRAP, cleanup);
                }
                for (uint32_t i = 0; i < expected_func_type->result_count; ++i) {
                    // Type mismatch (result type)
                    WAH_ENSURE_GOTO(expected_func_type->result_types[i] == actual_func_type->result_types[i], WAH_ERROR_TRAP, cleanup);
                }
                // Perform the call using actual_func_idx
                const wah_code_body_t *called_code = &ctx->module->code_bodies[actual_func_idx];
                uint32_t new_locals_offset = ctx->sp - expected_func_type->param_count; // Use expected_func_type for stack manipulation

                frame->bytecode_ip = bytecode_ip;

                WAH_CHECK_GOTO(wah_push_frame(ctx, actual_func_idx, new_locals_offset), cleanup);

                uint32_t num_locals = called_code->local_count;
                if (num_locals > 0) {
                    WAH_ENSURE_GOTO(ctx->sp + num_locals <= ctx->value_stack_capacity, WAH_ERROR_CALL_STACK_OVERFLOW, cleanup);
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
                WAH_ENSURE_GOTO(VSTACK_B.i##N != 0, WAH_ERROR_TRAP, cleanup); \
                WAH_ENSURE_GOTO(VSTACK_A.i##N != INT##N##_MIN || VSTACK_B.i##N != -1, WAH_ERROR_TRAP, cleanup); \
                VSTACK_A.i##N /= VSTACK_B.i##N; ctx->sp--; break;  \
            } \
            case WAH_OP_I##N##_DIV_U: {  \
                WAH_ENSURE_GOTO(VSTACK_B.i##N != 0, WAH_ERROR_TRAP, cleanup); \
                VSTACK_A.i##N = (int##N##_t)((uint##N##_t)VSTACK_A.i##N / (uint##N##_t)VSTACK_B.i##N); ctx->sp--; break; \
            } \
            case WAH_OP_I##N##_REM_S: {  \
                WAH_ENSURE_GOTO(VSTACK_B.i##N != 0, WAH_ERROR_TRAP, cleanup); \
                if (VSTACK_A.i##N == INT##N##_MIN && VSTACK_B.i##N == -1) { VSTACK_A.i##N = 0; } else { VSTACK_A.i##N %= VSTACK_B.i##N; } \
                ctx->sp--; break;  \
            } \
            case WAH_OP_I##N##_REM_U: {  \
                WAH_ENSURE_GOTO(VSTACK_B.i##N != 0, WAH_ERROR_TRAP, cleanup); \
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
                WAH_ENSURE_GOTO(effective_addr < ctx->memory_size && ctx->memory_size - effective_addr >= N/8, \
                                WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup); \
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
                WAH_ENSURE_GOTO(effective_addr < ctx->memory_size && ctx->memory_size - effective_addr >= N/8, \
                                WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup); \
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

                WAH_ENSURE_GOTO((uint64_t)dst + size <= ctx->memory_size, WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup);
                memset(ctx->memory_base + dst, val, size);
                break;
            }
            case WAH_OP_MEMORY_INIT: {
                uint32_t data_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                uint32_t mem_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);

                WAH_ENSURE_GOTO(mem_idx == 0, WAH_ERROR_TRAP, cleanup); // Only memory 0 supported
                WAH_ENSURE_GOTO(data_idx < ctx->module->data_segment_count, WAH_ERROR_TRAP, cleanup);

                uint32_t size = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t src_offset = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t dest_offset = (uint32_t)ctx->value_stack[--ctx->sp].i32;

                const wah_data_segment_t *segment = &ctx->module->data_segments[data_idx];

                WAH_ENSURE_GOTO((uint64_t)dest_offset + size <= ctx->memory_size, WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup);
                WAH_ENSURE_GOTO((uint64_t)src_offset + size <= segment->data_len, WAH_ERROR_TRAP, cleanup); // Ensure source data is within segment bounds
                WAH_ENSURE_GOTO(size <= segment->data_len, WAH_ERROR_TRAP, cleanup); // Cannot initialize more than available data

                memcpy(ctx->memory_base + dest_offset, segment->data + src_offset, size);
                break;
            }
            case WAH_OP_MEMORY_COPY: {
                uint32_t dest_mem_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);
                uint32_t src_mem_idx = wah_read_u32_le(bytecode_ip);
                bytecode_ip += sizeof(uint32_t);

                WAH_ENSURE_GOTO(dest_mem_idx == 0 && src_mem_idx == 0, WAH_ERROR_TRAP, cleanup); // Only memory 0 supported

                uint32_t size = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t src = (uint32_t)ctx->value_stack[--ctx->sp].i32;
                uint32_t dest = (uint32_t)ctx->value_stack[--ctx->sp].i32;

                WAH_ENSURE_GOTO((uint64_t)dest + size <= ctx->memory_size, WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup);
                WAH_ENSURE_GOTO((uint64_t)src + size <= ctx->memory_size, WAH_ERROR_MEMORY_OUT_OF_BOUNDS, cleanup);

                memmove(ctx->memory_base + dest, ctx->memory_base + src, size);
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
    WAH_ENSURE(func_idx < module->function_count, WAH_ERROR_UNKNOWN_SECTION);

    const wah_func_type_t *func_type = &module->types[module->function_type_indices[func_idx]];
    WAH_ENSURE(param_count == func_type->param_count, WAH_ERROR_VALIDATION_FAILED);

    // Push initial params onto the value stack
    for (uint32_t i = 0; i < param_count; ++i) {
        WAH_ENSURE(exec_ctx->sp < exec_ctx->value_stack_capacity, WAH_ERROR_CALL_STACK_OVERFLOW); // Value stack overflow
        exec_ctx->value_stack[exec_ctx->sp++] = params[i];
    }

    // Push the first frame. Locals offset is the current stack pointer before parameters.
    WAH_CHECK(wah_push_frame(exec_ctx, func_idx, exec_ctx->sp - func_type->param_count));

    // Reserve space for the function's own locals and initialize them to zero
    uint32_t num_locals = exec_ctx->call_stack[0].code->local_count;
    if (num_locals > 0) {
        WAH_ENSURE(exec_ctx->sp + num_locals <= exec_ctx->value_stack_capacity, WAH_ERROR_OUT_OF_MEMORY);
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

    free(module->data_segments); // Free data segments

    if (module->exports) {
        for (uint32_t i = 0; i < module->export_count; ++i) {
            free((void*)module->exports[i].name);
        }
        free(module->exports); // Free exports
    }

    // Reset all fields to 0/NULL
    memset(module, 0, sizeof(wah_module_t));
}

// --- Export API Implementation ---
size_t wah_module_num_exports(const wah_module_t *module) {
    if (!module) return 0;
    return module->export_count;
}

wah_error_t wah_module_export(const wah_module_t *module, size_t idx, wah_entry_t *out) {
    WAH_ENSURE(module, WAH_ERROR_MISUSE);
    WAH_ENSURE(out, WAH_ERROR_MISUSE);
    WAH_ENSURE(idx < module->export_count, WAH_ERROR_NOT_FOUND);

    const wah_export_t *export_entry = &module->exports[idx];

    wah_entry_id_t entry_id;
    switch (export_entry->kind) {
        case 0: // Function
            entry_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_FUNCTION, export_entry->index);
            break;
        case 1: // Table
            entry_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_TABLE, export_entry->index);
            break;
        case 2: // Memory
            entry_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_MEMORY, export_entry->index);
            break;
        case 3: // Global
            entry_id = WAH_MAKE_ENTRY_ID(WAH_ENTRY_KIND_GLOBAL, export_entry->index);
            break;
        default:
            return WAH_ERROR_VALIDATION_FAILED; // Should not happen if parsing is correct
    }

    WAH_CHECK(wah_module_entry(module, entry_id, out));

    out->name = export_entry->name;
    out->name_len = export_entry->name_len;

    return WAH_OK;
}

wah_error_t wah_module_export_by_name(const wah_module_t *module, const char *name, wah_entry_t *out) {
    WAH_ENSURE(module, WAH_ERROR_MISUSE);
    WAH_ENSURE(name, WAH_ERROR_MISUSE);
    WAH_ENSURE(out, WAH_ERROR_MISUSE);

    size_t lookup_name_len = strlen(name);

    for (uint32_t i = 0; i < module->export_count; ++i) {
        const wah_export_t *export_entry = &module->exports[i];
        if (export_entry->name_len == lookup_name_len && strncmp(export_entry->name, name, lookup_name_len) == 0) {
            return wah_module_export(module, i, out);
        }
    }
    return WAH_ERROR_NOT_FOUND;
}

wah_error_t wah_module_entry(const wah_module_t *module, wah_entry_id_t entry_id, wah_entry_t *out) {
    WAH_ENSURE(module, WAH_ERROR_MISUSE);
    WAH_ENSURE(out, WAH_ERROR_MISUSE);

    uint32_t kind = WAH_GET_ENTRY_KIND(entry_id);
    uint32_t index = WAH_GET_ENTRY_INDEX(entry_id);

    out->id = entry_id;
    out->name = NULL; // No name for non-exported entries
    out->name_len = 0;
    out->is_mutable = false; // Default to false

    switch (kind) {
        case WAH_ENTRY_KIND_FUNCTION:
            WAH_ENSURE(index < module->function_count, WAH_ERROR_NOT_FOUND);
            out->type = WAH_TYPE_FUNCTION;
            const wah_func_type_t *func_type = &module->types[module->function_type_indices[index]];
            out->u.func.param_count = func_type->param_count;
            out->u.func.param_types = func_type->param_types;
            out->u.func.result_count = func_type->result_count;
            out->u.func.result_types = func_type->result_types;
            break;
        case WAH_ENTRY_KIND_TABLE:
            WAH_ENSURE(index < module->table_count, WAH_ERROR_NOT_FOUND);
            out->type = WAH_TYPE_TABLE;
            out->u.table.elem_type = module->tables[index].elem_type;
            out->u.table.min_elements = module->tables[index].min_elements;
            out->u.table.max_elements = module->tables[index].max_elements;
            break;
        case WAH_ENTRY_KIND_MEMORY:
            WAH_ENSURE(index < module->memory_count, WAH_ERROR_NOT_FOUND);
            out->type = WAH_TYPE_MEMORY;
            out->u.memory.min_pages = module->memories[index].min_pages;
            out->u.memory.max_pages = module->memories[index].max_pages;
            break;
        case WAH_ENTRY_KIND_GLOBAL:
            WAH_ENSURE(index < module->global_count, WAH_ERROR_NOT_FOUND);
            out->type = module->globals[index].type;
            out->is_mutable = module->globals[index].is_mutable;
            out->u.global_val = module->globals[index].initial_value;
            break;
        default:
            return WAH_ERROR_NOT_FOUND; // Unknown entry kind
    }
    return WAH_OK;
}

#endif // WAH_IMPLEMENTATION
#endif // WAH_H
