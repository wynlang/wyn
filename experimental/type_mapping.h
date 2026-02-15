#ifndef TYPE_MAPPING_H
#define TYPE_MAPPING_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include <stdbool.h>
#include "types.h"
#include "llvm_context.h"

// Type mapping functions
LLVMTypeRef map_wyn_type_to_llvm(Type* wyn_type, LLVMCodegenContext* ctx);
LLVMTypeRef get_basic_llvm_type(TypeKind kind, LLVMCodegenContext* ctx);

// Struct layout compatibility
LLVMTypeRef create_struct_type(StructType* struct_type, LLVMCodegenContext* ctx);
bool validate_struct_layout(StructType* struct_type, LLVMTypeRef llvm_type);

// Function signature conversion
LLVMTypeRef create_function_type(FunctionType* fn_type, LLVMCodegenContext* ctx);
LLVMValueRef declare_function(const char* name, FunctionType* fn_type, LLVMCodegenContext* ctx);

// Array type mapping
LLVMTypeRef create_array_type(Type* element_type, int length, LLVMCodegenContext* ctx);

// Optional type mapping (for T2.5.1 integration)
LLVMTypeRef create_optional_type(OptionalType* opt_type, LLVMCodegenContext* ctx);

// Type size and alignment utilities
size_t get_type_size(Type* wyn_type, LLVMCodegenContext* ctx);
size_t get_type_alignment(Type* wyn_type, LLVMCodegenContext* ctx);

// Type compatibility checking
bool are_types_compatible(Type* type1, Type* type2);
bool can_convert_types(Type* from, Type* to);

// Initialize type mapping system
bool init_type_mapping(LLVMCodegenContext* ctx);
void cleanup_type_mapping(LLVMCodegenContext* ctx);

#else
// Fallback when LLVM is not available
static inline bool init_type_mapping(void* ctx) { return false; }
static inline void cleanup_type_mapping(void* ctx) { (void)ctx; }
#endif

#endif // TYPE_MAPPING_H
