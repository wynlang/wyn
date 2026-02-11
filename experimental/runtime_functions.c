#include "runtime_functions.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include "safe_memory.h"
#include "error.h"

// Declare ARC runtime functions
bool declare_arc_runtime_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    // ARC retain: WynObject* wyn_arc_retain(WynObject* obj)
    LLVMTypeRef obj_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef retain_params[] = {obj_ptr_type};
    LLVMTypeRef retain_type = create_runtime_function_type(obj_ptr_type, retain_params, 1, false);
    ctx->runtime_functions.retain_fn = declare_external_function(ctx, "wyn_arc_retain", retain_type);
    
    // ARC release: void wyn_arc_release(WynObject* obj)
    LLVMTypeRef release_params[] = {obj_ptr_type};
    LLVMTypeRef release_type = create_runtime_function_type(ctx->void_type, release_params, 1, false);
    ctx->runtime_functions.release_fn = declare_external_function(ctx, "wyn_arc_release", release_type);
    
    // ARC alloc: WynObject* wyn_arc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*))
    LLVMTypeRef size_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef type_id_type = LLVMInt32TypeInContext(ctx->context);
    LLVMTypeRef destructor_type = LLVMPointerType(
        LLVMFunctionType(ctx->void_type, &obj_ptr_type, 1, false), 0
    );
    LLVMTypeRef alloc_params[] = {size_type, type_id_type, destructor_type};
    LLVMTypeRef alloc_type = create_runtime_function_type(obj_ptr_type, alloc_params, 3, false);
    LLVMValueRef arc_alloc_fn = declare_external_function(ctx, "wyn_arc_alloc", alloc_type);
    
    // ARC dealloc: void wyn_arc_dealloc(WynObject* obj)
    LLVMTypeRef dealloc_params[] = {obj_ptr_type};
    LLVMTypeRef dealloc_type = create_runtime_function_type(ctx->void_type, dealloc_params, 1, false);
    LLVMValueRef arc_dealloc_fn = declare_external_function(ctx, "wyn_arc_dealloc", dealloc_type);
    
    return (ctx->runtime_functions.retain_fn != NULL && 
            ctx->runtime_functions.release_fn != NULL &&
            arc_alloc_fn != NULL && arc_dealloc_fn != NULL);
}

// Declare standard library functions
bool declare_stdlib_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    // printf: int printf(const char* format, ...)
    LLVMTypeRef char_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef printf_params[] = {char_ptr_type};
    LLVMTypeRef printf_type = create_runtime_function_type(ctx->int_type, printf_params, 1, true);
    ctx->runtime_functions.printf_fn = declare_external_function(ctx, "printf", printf_type);
    
    // malloc: void* malloc(size_t size)
    LLVMTypeRef size_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef void_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef malloc_params[] = {size_type};
    LLVMTypeRef malloc_type = create_runtime_function_type(void_ptr_type, malloc_params, 1, false);
    ctx->runtime_functions.malloc_fn = declare_external_function(ctx, "malloc", malloc_type);
    
    // free: void free(void* ptr)
    LLVMTypeRef free_params[] = {void_ptr_type};
    LLVMTypeRef free_type = create_runtime_function_type(ctx->void_type, free_params, 1, false);
    ctx->runtime_functions.free_fn = declare_external_function(ctx, "free", free_type);
    
    // memcpy: void* memcpy(void* dest, const void* src, size_t n)
    LLVMTypeRef memcpy_params[] = {void_ptr_type, void_ptr_type, size_type};
    LLVMTypeRef memcpy_type = create_runtime_function_type(void_ptr_type, memcpy_params, 3, false);
    LLVMValueRef memcpy_fn = declare_external_function(ctx, "memcpy", memcpy_type);
    
    // memset: void* memset(void* s, int c, size_t n)
    LLVMTypeRef memset_params[] = {void_ptr_type, ctx->int_type, size_type};
    LLVMTypeRef memset_type = create_runtime_function_type(void_ptr_type, memset_params, 3, false);
    LLVMValueRef memset_fn = declare_external_function(ctx, "memset", memset_type);
    
    return (ctx->runtime_functions.printf_fn != NULL &&
            ctx->runtime_functions.malloc_fn != NULL &&
            ctx->runtime_functions.free_fn != NULL &&
            memcpy_fn != NULL && memset_fn != NULL);
}

// Declare platform-specific functions
bool declare_platform_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    LLVMTypeRef size_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef void_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    
#ifdef __APPLE__
    // macOS specific: use malloc_zone functions for better performance
    LLVMTypeRef zone_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef zone_malloc_params[] = {zone_ptr_type, size_type};
    LLVMTypeRef zone_malloc_type = create_runtime_function_type(void_ptr_type, zone_malloc_params, 2, false);
    LLVMValueRef zone_malloc_fn = declare_external_function(ctx, "malloc_zone_malloc", zone_malloc_type);
    
    LLVMTypeRef zone_free_params[] = {zone_ptr_type, void_ptr_type};
    LLVMTypeRef zone_free_type = create_runtime_function_type(ctx->void_type, zone_free_params, 2, false);
    LLVMValueRef zone_free_fn = declare_external_function(ctx, "malloc_zone_free", zone_free_type);
    
    return (zone_malloc_fn != NULL && zone_free_fn != NULL);
    
#elif defined(__linux__)
    // Linux specific: use aligned_alloc for better performance
    LLVMTypeRef aligned_alloc_params[] = {size_type, size_type};
    LLVMTypeRef aligned_alloc_type = create_runtime_function_type(void_ptr_type, aligned_alloc_params, 2, false);
    LLVMValueRef aligned_alloc_fn = declare_external_function(ctx, "aligned_alloc", aligned_alloc_type);
    
    return (aligned_alloc_fn != NULL);
    
#elif defined(_WIN32)
    // Windows specific: use _aligned_malloc
    LLVMTypeRef aligned_malloc_params[] = {size_type, size_type};
    LLVMTypeRef aligned_malloc_type = create_runtime_function_type(void_ptr_type, aligned_malloc_params, 2, false);
    LLVMValueRef aligned_malloc_fn = declare_external_function(ctx, "_aligned_malloc", aligned_malloc_type);
    
    LLVMTypeRef aligned_free_params[] = {void_ptr_type};
    LLVMTypeRef aligned_free_type = create_runtime_function_type(ctx->void_type, aligned_free_params, 1, false);
    LLVMValueRef aligned_free_fn = declare_external_function(ctx, "_aligned_free", aligned_free_type);
    
    return (aligned_malloc_fn != NULL && aligned_free_fn != NULL);
#else
    // Generic platform: just use standard malloc/free
    return true;
#endif
}

// Declare string runtime functions
bool declare_string_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    LLVMTypeRef char_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    LLVMTypeRef size_type = LLVMInt64TypeInContext(ctx->context);
    
    // string_concat: char* wyn_string_concat(const char* str1, const char* str2)
    LLVMTypeRef concat_params[] = {char_ptr_type, char_ptr_type};
    LLVMTypeRef concat_type = create_runtime_function_type(char_ptr_type, concat_params, 2, false);
    LLVMValueRef concat_fn = declare_external_function(ctx, "wyn_string_concat", concat_type);
    
    // string_length: size_t wyn_string_length(const char* str)
    LLVMTypeRef length_params[] = {char_ptr_type};
    LLVMTypeRef length_type = create_runtime_function_type(size_type, length_params, 1, false);
    LLVMValueRef length_fn = declare_external_function(ctx, "wyn_string_length", length_type);
    
    // string_compare: int wyn_string_compare(const char* str1, const char* str2)
    LLVMTypeRef compare_params[] = {char_ptr_type, char_ptr_type};
    LLVMTypeRef compare_type = create_runtime_function_type(ctx->int_type, compare_params, 2, false);
    LLVMValueRef compare_fn = declare_external_function(ctx, "wyn_string_compare", compare_type);
    
    return (concat_fn != NULL && length_fn != NULL && compare_fn != NULL);
}

// Declare all runtime functions
bool declare_all_runtime_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    bool arc_ok = declare_arc_runtime_functions(ctx);
    bool stdlib_ok = declare_stdlib_functions(ctx);
    bool platform_ok = declare_platform_functions(ctx);
    bool string_ok = declare_string_functions(ctx);
    
    return arc_ok && stdlib_ok && platform_ok && string_ok;
}

// Get runtime function by name
LLVMValueRef get_runtime_function(LLVMCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    
    return LLVMGetNamedFunction(ctx->module, name);
}

// Check if runtime function is declared
bool is_runtime_function_declared(LLVMCodegenContext* ctx, const char* name) {
    return get_runtime_function(ctx, name) != NULL;
}

// Helper functions
LLVMTypeRef create_runtime_function_type(LLVMTypeRef return_type, LLVMTypeRef* param_types, unsigned param_count, bool is_vararg) {
    return LLVMFunctionType(return_type, param_types, param_count, is_vararg);
}

LLVMValueRef declare_external_function(LLVMCodegenContext* ctx, const char* name, LLVMTypeRef function_type) {
    if (!ctx || !name || !function_type) return NULL;
    
    // Check if function already exists
    LLVMValueRef existing = LLVMGetNamedFunction(ctx->module, name);
    if (existing) return existing;
    
    // Declare new function
    return LLVMAddFunction(ctx->module, name, function_type);
}

// Getter functions for specific runtime functions
LLVMValueRef get_arc_retain_function(LLVMCodegenContext* ctx) {
    return ctx ? ctx->runtime_functions.retain_fn : NULL;
}

LLVMValueRef get_arc_release_function(LLVMCodegenContext* ctx) {
    return ctx ? ctx->runtime_functions.release_fn : NULL;
}

LLVMValueRef get_arc_alloc_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "wyn_arc_alloc");
}

LLVMValueRef get_arc_dealloc_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "wyn_arc_dealloc");
}

LLVMValueRef get_printf_function(LLVMCodegenContext* ctx) {
    return ctx ? ctx->runtime_functions.printf_fn : NULL;
}

LLVMValueRef get_malloc_function(LLVMCodegenContext* ctx) {
    return ctx ? ctx->runtime_functions.malloc_fn : NULL;
}

LLVMValueRef get_free_function(LLVMCodegenContext* ctx) {
    return ctx ? ctx->runtime_functions.free_fn : NULL;
}

LLVMValueRef get_memcpy_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "memcpy");
}

LLVMValueRef get_memset_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "memset");
}

LLVMValueRef get_platform_alloc_function(LLVMCodegenContext* ctx) {
#ifdef __APPLE__
    return get_runtime_function(ctx, "malloc_zone_malloc");
#elif defined(__linux__)
    return get_runtime_function(ctx, "aligned_alloc");
#elif defined(_WIN32)
    return get_runtime_function(ctx, "_aligned_malloc");
#else
    return get_malloc_function(ctx);
#endif
}

LLVMValueRef get_platform_free_function(LLVMCodegenContext* ctx) {
#ifdef __APPLE__
    return get_runtime_function(ctx, "malloc_zone_free");
#elif defined(_WIN32)
    return get_runtime_function(ctx, "_aligned_free");
#else
    return get_free_function(ctx);
#endif
}

LLVMValueRef get_string_concat_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "wyn_string_concat");
}

LLVMValueRef get_string_length_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "wyn_string_length");
}

LLVMValueRef get_string_compare_function(LLVMCodegenContext* ctx) {
    return get_runtime_function(ctx, "wyn_string_compare");
}

#endif // WITH_LLVM
