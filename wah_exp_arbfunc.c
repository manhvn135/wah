#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <stddef.h> // For offsetof 
#include <assert.h> // For assert 
#include <stdbool.h> // For bool 
#include <stdint.h> // For int32_t, uint32_t, int64_t, uint64_t 

// --- Minimal WAH-like definitions for this experiment --- 

// Define wah_value_type_t 
typedef enum { 
 WAH_TYPE_BOOL, 
 WAH_TYPE_INT, 
 WAH_TYPE_UINT, 
 WAH_TYPE_LONG, 
 WAH_TYPE_ULONG, 
 WAH_TYPE_LLONG, 
 WAH_TYPE_ULLONG, 
 WAH_TYPE_I32, 
 WAH_TYPE_U32, 
 WAH_TYPE_I64, 
 WAH_TYPE_U64, 
 WAH_TYPE_FLOAT, 
 WAH_TYPE_DOUBLE, 
 WAH_TYPE_VOID, // For no return 
 WAH_TYPE_UNKNOWN 
} wah_value_type_t; 

// Untyped wah_value_t 
typedef union { 
 bool b; 
 int i; 
 unsigned int I; 
 long l; 
 unsigned long L; 
 long long q; 
 unsigned long long Q; 
 int32_t i32; 
 uint32_t u32; 
 int64_t i64; 
 uint64_t u64; 
 float f; 
 double d; 
} wah_value_t; 

// C struct field descriptor 
typedef struct { 
 wah_value_type_t wasm_type; 
 size_t offset; 
 size_t size; // Size of the C type 
} wah_c_struct_field_desc_t; 

// C struct descriptor 
typedef struct { 
 wah_c_struct_field_desc_t* fields; 
 size_t num_fields; 
 size_t struct_size; 
} wah_c_struct_desc_t; 

// Minimal wah_exec_context_t for host functions 
typedef struct { 
 // Placeholder for actual context 
 int dummy; 
} wah_exec_context_t; 

// Host function pointer type (generic for the internal wrapper) 
// The user's function will be cast to this. 
typedef int (*experiment_host_fn_ptr_t)(wah_exec_context_t* ctx, void* params_struct, void* returns_struct, void* user_data); 

// --- Mini-language parsing --- 

typedef struct { 
 wah_value_type_t* param_types; 
 size_t num_params; 
 wah_value_type_t* return_types; 
 size_t num_returns; 
} parsed_signature_t; 

// Helper to get size of a wah_value_type_t 
size_t get_type_size(wah_value_type_t type) { 
 switch (type) { 
 case WAH_TYPE_BOOL: return sizeof(bool); 
 case WAH_TYPE_INT: return sizeof(int); 
 case WAH_TYPE_UINT: return sizeof(unsigned int); 
 case WAH_TYPE_LONG: return sizeof(long); 
 case WAH_TYPE_ULONG: return sizeof(unsigned long); 
 case WAH_TYPE_LLONG: return sizeof(long long); 
 case WAH_TYPE_ULLONG: return sizeof(unsigned long long); 
 case WAH_TYPE_I32: return sizeof(int32_t); 
 case WAH_TYPE_U32: return sizeof(uint32_t); 
 case WAH_TYPE_I64: return sizeof(int64_t); 
 case WAH_TYPE_U64: return sizeof(uint64_t); 
 case WAH_TYPE_FLOAT: return sizeof(float); 
 case WAH_TYPE_DOUBLE: return sizeof(double); 
 case WAH_TYPE_VOID: return 0; 
 case WAH_TYPE_UNKNOWN: return 0; 
 } 
 return 0; 
} 

// Helper to get alignment of a wah_value_type_t (simplified for common types) 
size_t get_type_alignment(wah_value_type_t type) { 
 // For simplicity, assume alignment equals size for basic types, 
 // or a common alignment like 8 for larger types. 
 // In a real scenario, this would need to be more precise. 
 size_t size = get_type_size(type); 
 if (size == 0) return 1; // Or some default 
 if (size <= 4) return size; 
 return 8; // Common alignment for 64-bit types 
} 

wah_value_type_t char_to_wah_type(const char* s, int* len) { 
 if (s[0] == 'b') { *len = 1; return WAH_TYPE_BOOL; } 
 if (s[0] == 'i' && s[1] == '3' && s[2] == '2') { *len = 3; return WAH_TYPE_I32; } 
 if (s[0] == 'u' && s[1] == '3' && s[2] == '2') { *len = 3; return WAH_TYPE_U32; } 
 if (s[0] == 'i' && s[1] == '6' && s[2] == '4') { *len = 3; return WAH_TYPE_I64; } 
 if (s[0] == 'u' && s[1] == '6' && s[2] == '4') { *len = 3; return WAH_TYPE_U64; } 
 if (s[0] == 'i') { *len = 1; return WAH_TYPE_INT; } 
 if (s[0] == 'I') { *len = 1; return WAH_TYPE_UINT; } 
 if (s[0] == 'l') { *len = 1; return WAH_TYPE_LONG; } 
 if (s[0] == 'L') { *len = 1; return WAH_TYPE_ULONG; } 
 if (s[0] == 'q') { *len = 1; return WAH_TYPE_LLONG; } 
 if (s[0] == 'Q') { *len = 1; return WAH_TYPE_ULLONG; } 
 if (s[0] == 'f') { *len = 1; return WAH_TYPE_FLOAT; } 
 if (s[0] == 'd') { *len = 1; return WAH_TYPE_DOUBLE; } 
 *len = 0; 
 return WAH_TYPE_UNKNOWN; 
} 

parsed_signature_t parse_signature_string(const char* signature_str) { 
 parsed_signature_t sig = {0}; 
 const char* p = signature_str; 
 const char* returns_start = NULL; 
 int count = 0; 
 int len; 

 // First pass: count params and find '>' 
 const char* temp_p = signature_str; 
 while (*temp_p) { 
 if (*temp_p == '>') { 
 returns_start = temp_p + 1; 
 break; 
 } 
 if (*temp_p == ' ' || *temp_p == '\t') { temp_p++; continue; } // Ignore whitespace 
 wah_value_type_t type = char_to_wah_type(temp_p, &len); 
 if (type == WAH_TYPE_UNKNOWN) { 
 fprintf(stderr, "Error: Unknown type in signature string: %s\n", temp_p); 
 return (parsed_signature_t){0}; 
 } 
 sig.num_params++; 
 temp_p += len; 
 } 

 // Allocate param_types 
 if (sig.num_params > 0) { 
 sig.param_types = (wah_value_type_t*)malloc(sig.num_params * sizeof(wah_value_type_t)); 
 assert(sig.param_types); 
 } 

 // Second pass: populate param_types 
 p = signature_str; 
 count = 0; 
 while (*p) { 
 if (*p == '>') break; 
 if (*p == ' ' || *p == '\t') { p++; continue; } 
 sig.param_types[count++] = char_to_wah_type(p, &len); 
 p += len; 
 } 

 // Parse return types 
 if (returns_start) { 
 p = returns_start; 
 // First pass: count returns 
 temp_p = returns_start; 
 while (*temp_p) { 
 if (*temp_p == ' ' || *temp_p == '\t') { temp_p++; continue; } 
 wah_value_type_t type = char_to_wah_type(temp_p, &len); 
 if (type == WAH_TYPE_UNKNOWN) { 
 fprintf(stderr, "Error: Unknown type in signature string: %s\n", temp_p); 
 free(sig.param_types); 
 return (parsed_signature_t){0}; 
 } 
 sig.num_returns++; 
 temp_p += len; 
 } 
 // Allocate return_types 
 if (sig.num_returns > 0) { 
 sig.return_types = (wah_value_type_t*)malloc(sig.num_returns * sizeof(wah_value_type_t)); 
 assert(sig.return_types); 
 } 
 // Second pass: populate return_types 
 p = returns_start; 
 count = 0; 
 while (*p) { 
 if (*p == ' ' || *p == '\t') { p++; continue; } 
 sig.return_types[count++] = char_to_wah_type(p, &len); 
 p += len; 
 } 
 } else { 
 // No '>' means no return types 
 sig.num_returns = 0; 
 sig.return_types = NULL; 
 } 

 return sig; 
} 

// --- C struct descriptor generation --- 

// Function to calculate struct layout 
wah_c_struct_desc_t generate_struct_desc(const wah_value_type_t* types, size_t num_types) { 
 wah_c_struct_desc_t desc = {0}; 
 if (num_types == 0) { 
 desc.struct_size = 1; // Smallest possible struct for empty 
 return desc; 
 } 

 desc.fields = (wah_c_struct_field_desc_t*)malloc(num_types * sizeof(wah_c_struct_field_desc_t)); 
 assert(desc.fields); 
 desc.num_fields = num_types; 

 size_t current_offset = 0; 
 size_t max_alignment = 1; 

 for (size_t i = 0; i < num_types; ++i) { 
 wah_value_type_t type = types[i]; 
 size_t type_size = get_type_size(type); 
 size_t type_alignment = get_type_alignment(type); 

 // Apply padding for alignment 
 current_offset = (current_offset + type_alignment - 1) & ~(type_alignment - 1); 

 desc.fields[i].wasm_type = type; 
 desc.fields[i].offset = current_offset; 
 desc.fields[i].size = type_size; 

 current_offset += type_size; 
 if (type_alignment > max_alignment) { 
 max_alignment = type_alignment; 
 } 
 } 

 // Apply padding for overall struct alignment 
 desc.struct_size = (current_offset + max_alignment - 1) & ~(max_alignment - 1); 
 if (desc.struct_size == 0 && num_types > 0) { // Handle case where all types are 0-size (e.g., void) 
 desc.struct_size = 1; // Smallest possible struct 
 } 

 return desc; 
} 

// --- Test values generation --- 

wah_value_t get_test_value(wah_value_type_t type, int index) { 
 wah_value_t val = {0}; 
 switch (type) { 
 case WAH_TYPE_BOOL: val.b = (index % 2 == 0); break; 
 case WAH_TYPE_INT: val.i = 100 + index; break; 
 case WAH_TYPE_UINT: val.I = 200 + index; break; 
 case WAH_TYPE_LONG: val.l = 300L + index; break; 
 case WAH_TYPE_ULONG: val.L = 400UL + index; break; 
 case WAH_TYPE_LLONG: val.q = 500LL + index; break; 
 case WAH_TYPE_ULLONG: val.Q = 600ULL + index; break; 
 case WAH_TYPE_I32: val.i32 = 700 + index; break; 
 case WAH_TYPE_U32: val.u32 = 800 + index; break; 
 case WAH_TYPE_I64: val.i64 = 900LL + index; break; 
 case WAH_TYPE_U64: val.u64 = 1000ULL + index; break; 
 case WAH_TYPE_FLOAT: val.f = 10.5f + (float)index; break; 
 case WAH_TYPE_DOUBLE: val.d = 20.7 + (double)index; break; 
 case WAH_TYPE_VOID: break; 
 case WAH_TYPE_UNKNOWN: break; 
 } 
 return val; 
} 

void print_wah_value(wah_value_t val, wah_value_type_t type) { 
 switch (type) { 
 case WAH_TYPE_BOOL: printf("%s", val.b ? "true" : "false"); break; 
 case WAH_TYPE_INT: printf("%d", val.i); break; 
 case WAH_TYPE_UINT: printf("%u", val.I); break; 
 case WAH_TYPE_LONG: printf("%ld", val.l); break; 
 case WAH_TYPE_ULONG: printf("%lu", val.L); break; 
 case WAH_TYPE_LLONG: printf("%lld", val.q); break; 
 case WAH_TYPE_ULLONG: printf("%llu", val.Q); break; 
 case WAH_TYPE_I32: printf("%d", val.i32); break; 
 case WAH_TYPE_U32: printf("%u", val.u32); break; 
 case WAH_TYPE_I64: printf("%lld", val.i64); break; 
 case WAH_TYPE_U64: printf("%llu", val.u64); break; 
 case WAH_TYPE_FLOAT: printf("%f", val.f); break; 
 case WAH_TYPE_DOUBLE: printf("%lf", val.d); break; 
 case WAH_TYPE_VOID: printf("void"); break; 
 case WAH_TYPE_UNKNOWN: printf("UNKNOWN"); break; 
 } 
} 

// --- The core test function --- 

int call_arb_func(const char* signature_str, experiment_host_fn_ptr_t user_fn_ptr, void* user_data) { 
 printf("\n--- Testing signature: \"%s\" ---\n", signature_str); 

 parsed_signature_t sig = parse_signature_string(signature_str); 
 if (sig.param_types == NULL && sig.num_params > 0) { // Error during parsing 
 return 1; 
 } 

 wah_c_struct_desc_t params_desc = generate_struct_desc(sig.param_types, sig.num_params); 
 wah_c_struct_desc_t returns_desc = generate_struct_desc(sig.return_types, sig.num_returns); 

 printf("  Parsed Params: %zu types, Struct Size: %zu\n", sig.num_params, params_desc.struct_size); 
 for (size_t i = 0; i < sig.num_params; ++i) { 
 printf("    [%zu] Type: %d, Offset: %zu, Size: %zu\n", i, sig.param_types[i], params_desc.fields[i].offset, params_desc.fields[i].size); 
 } 
 printf("  Parsed Returns: %zu types, Struct Size: %zu\n", sig.num_returns, returns_desc.struct_size); 
 for (size_t i = 0; i < sig.num_returns; ++i) { 
 printf("    [%zu] Type: %d, Offset: %zu, Size: %zu\n", i, sig.return_types[i], returns_desc.fields[i].offset, returns_desc.fields[i].size); 
 } 

 // --- Simulate WAH's internal wrapper logic --- 

 // 1. Allocate and populate parameter struct 
 void* params_struct_ptr = NULL; 
 if (params_desc.struct_size > 0) { 
 params_struct_ptr = calloc(1, params_desc.struct_size); 
 assert(params_struct_ptr); 
 } 

 printf("  Input values (WAH_value_t -> C struct):\n"); 
 for (size_t i = 0; i < sig.num_params; ++i) { 
 wah_value_t test_val = get_test_value(sig.param_types[i], i); 
 printf("    Param %zu (Type %d): ", i, sig.param_types[i]); 
 print_wah_value(test_val, sig.param_types[i]); 
 printf(" -> "); 

 // Copy test_val into the correct offset in params_struct_ptr 
 switch (sig.param_types[i]) { 
 case WAH_TYPE_BOOL: *(bool*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.b; break; 
 case WAH_TYPE_INT: *(int*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.i; break; 
 case WAH_TYPE_UINT: *(unsigned int*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.I; break; 
 case WAH_TYPE_LONG: *(long*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.l; break; 
 case WAH_TYPE_ULONG: *(unsigned long*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.L; break; 
 case WAH_TYPE_LLONG: *(long long*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.q; break; 
 case WAH_TYPE_ULLONG: *(unsigned long long*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.Q; break; 
 case WAH_TYPE_I32: *(int32_t*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.i32; break; 
 case WAH_TYPE_U32: *(uint32_t*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.u32; break; 
 case WAH_TYPE_I64: *(int64_t*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.i64; break; 
 case WAH_TYPE_U64: *(uint64_t*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.u64; break; 
 case WAH_TYPE_FLOAT: *(float*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.f; break; 
 case WAH_TYPE_DOUBLE: *(double*)((char*)params_struct_ptr + params_desc.fields[i].offset) = test_val.d; break; 
 case WAH_TYPE_VOID: case WAH_TYPE_UNKNOWN: break; 
 } 
 printf("Stored at offset %zu\n", params_desc.fields[i].offset); 
 } 

 // 2. Allocate return struct 
 void* returns_struct_ptr = NULL; 
 if (returns_desc.struct_size > 0) { 
 returns_struct_ptr = calloc(1, returns_desc.struct_size); 
 assert(returns_struct_ptr); 
 } 

 // 3. Call user's host function 
 wah_exec_context_t ctx = {0}; // Dummy context 
 int host_fn_result = user_fn_ptr(&ctx, params_struct_ptr, returns_struct_ptr, user_data); 

 printf("  Host function returned: %d (0=success)\n", host_fn_result); 

 // 4. Verify return values (C struct -> WAH_value_t) 
 printf("  Output values (C struct -> WAH_value_t):\n"); 
 for (size_t i = 0; i < sig.num_returns; ++i) { 
 wah_value_t actual_return_val = {0}; 
 printf("    Return %zu (Type %d): ", i, sig.return_types[i]); 

 // Copy from returns_struct_ptr into actual_return_val 
 switch (sig.return_types[i]) { 
 case WAH_TYPE_BOOL: actual_return_val.b = *(bool*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_INT: actual_return_val.i = *(int*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_UINT: actual_return_val.I = *(unsigned int*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_LONG: actual_return_val.l = *(long*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_ULONG: actual_return_val.L = *(unsigned long*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_LLONG: actual_return_val.q = *(long long*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_ULLONG: actual_return_val.Q = *(unsigned long long*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_I32: actual_return_val.i32 = *(int32_t*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_U32: actual_return_val.u32 = *(uint32_t*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_I64: actual_return_val.i64 = *(int64_t*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_U64: actual_return_val.u64 = *(uint64_t*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_FLOAT: actual_return_val.f = *(float*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_DOUBLE: actual_return_val.d = *(double*)((char*)returns_struct_ptr + returns_desc.fields[i].offset); break; 
 case WAH_TYPE_VOID: case WAH_TYPE_UNKNOWN: break; 
 } 
 print_wah_value(actual_return_val, sig.return_types[i]); 
 printf("\n"); 
 } 


 // --- Cleanup --- 
 free(sig.param_types); 
 free(sig.return_types); 
 free(params_desc.fields); 
 free(returns_desc.fields); 
 free(params_struct_ptr); 
 free(returns_struct_ptr); 

 return host_fn_result; 
} 

// --- Example Host Functions --- 

// Example 1: i32, f -> i32 
typedef struct { int32_t p0; float p1; } Ex1Params; 
typedef struct { int32_t r0; } Ex1Returns; 
int ex1_host_func(wah_exec_context_t* ctx, const Ex1Params* params, Ex1Returns* returns, void* user_data) { 
 (void)ctx; (void)user_data; 
 printf("  [Host] Ex1: p0=%d, p1=%f\n", params->p0, params->p1); 
 returns->r0 = params->p0 + (int32_t)params->p1; 
 return 0; 
} 

// Example 2: I, Q -> d 
typedef struct { unsigned int p0; unsigned long long p1; } Ex2Params; 
typedef struct { double r0; } Ex2Returns; 
int ex2_host_func(wah_exec_context_t* ctx, const Ex2Params* params, Ex2Returns* returns, void* user_data) { 
 (void)ctx; (void)user_data; 
 printf("  [Host] Ex2: p0=%u, p1=%llu\n", params->p0, params->p1); 
 returns->r0 = (double)params->p0 + (double)params->p1; 
 return 0; 
} 

// Example 3: b, i, l, q -> I 
typedef struct { bool p0; int p1; long p2; long long p3; } Ex3Params; 
typedef struct { unsigned int r0; } Ex3Returns; 
int ex3_host_func(wah_exec_context_t* ctx, const Ex3Params* params, Ex3Returns* returns, void* user_data) { 
 (void)ctx; (void)user_data; 
 printf("  [Host] Ex3: p0=%s, p1=%d, p2=%ld, p3=%lld\n", params->p0 ? "true" : "false", params->p1, params->p2, params->p3); 
 returns->r0 = (unsigned int)(params->p0 + params->p1 + params->p2 + params->p3); 
 return 0; 
} 

// Example 4: No params, no returns 
typedef struct {} Ex4Params; 
typedef struct {} Ex4Returns; 
int ex4_host_func(wah_exec_context_t* ctx, const Ex4Params* params, Ex4Returns* returns, void* user_data) { 
 (void)ctx; (void)params; (void)returns; (void)user_data; 
 printf("  [Host] Ex4: No params, no returns. \n"); 
 return 0; 
} 

// Example 5: All types 
typedef struct { 
 bool b_val; int i_val; unsigned int I_val; long l_val; unsigned long L_val; 
 long long q_val; unsigned long long Q_val; 
 int32_t i32_val; uint32_t u32_val; int64_t i64_val; uint64_t u64_val; 
 float f_val; double d_val; 
} Ex5Params; 
typedef struct { double r0; } Ex5Returns; 
int ex5_host_func(wah_exec_context_t* ctx, const Ex5Params* params, Ex5Returns* returns, void* user_data) { 
 (void)ctx; (void)user_data; 
 printf("  [Host] Ex5: All types received. \n"); 
 printf("    b:%s i:%d I:%u l:%ld L:%lu q:%lld Q:%llu\n", params->b_val?"T":"F", params->i_val, params->I_val, params->l_val, params->L_val, params->q_val, params->Q_val); 
 printf("    i32:%d u32:%u i64:%lld u64:%llu f:%f d:%lf\n", params->i32_val, params->u32_val, params->i64_val, params->u64_val, params->f_val, params->d_val); 
 returns->r0 = params->d_val + params->f_val; 
 return 0; 
} 


int main() { 
 // Test cases 
 call_arb_func("i32 f > i32", (experiment_host_fn_ptr_t)ex1_host_func, NULL); 
 call_arb_func("I Q > d", (experiment_host_fn_ptr_t)ex2_host_func, NULL); 
 call_arb_func("b i l q > I", (experiment_host_fn_ptr_t)ex3_host_func, NULL); 
 call_arb_func(" > ", (experiment_host_fn_ptr_t)ex4_host_func, NULL); // No params, no returns 
 call_arb_func("b i I l L q Q i32 u32 i64 u64 f d > d", (experiment_host_fn_ptr_t)ex5_host_func, NULL); 

 return 0; 
} 
