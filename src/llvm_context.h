#ifndef LLVM_CONTEXT_H
#define LLVM_CONTEXT_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdbool.h>
#include <pthread.h>
#include "ast.h"
#include "error.h"

// Forward declarations
typedef struct SymbolTable SymbolTable;

// LLVM Context Management Structure
typedef struct {
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    
    // Type cache for performance
    LLVMTypeRef int_type;
    LLVMTypeRef float_type;
    LLVMTypeRef bool_type;
    LLVMTypeRef string_type;
    LLVMTypeRef void_type;
    
    // Runtime function registry
    struct {
        LLVMValueRef malloc_fn;
        LLVMValueRef free_fn;
        LLVMValueRef retain_fn;
        LLVMValueRef release_fn;
        LLVMValueRef printf_fn;
    } runtime_functions;
    
    // Current compilation state
    LLVMValueRef current_function;
    LLVMBasicBlockRef current_block;
    
    // Thread safety
    pthread_mutex_t context_mutex;
    bool is_initialized;
    
    // Error handling integration
    bool has_errors;
    char* last_error;
    
    // Module name for identification
    char* module_name;
} LLVMCodegenContext;

// Context lifecycle management
LLVMCodegenContext* llvm_context_create(const char* module_name);
void llvm_context_destroy(LLVMCodegenContext* ctx);
bool llvm_context_verify_module(LLVMCodegenContext* ctx);

// Thread-safe context operations
bool llvm_context_lock(LLVMCodegenContext* ctx);
void llvm_context_unlock(LLVMCodegenContext* ctx);

// Module management
bool llvm_context_set_module_name(LLVMCodegenContext* ctx, const char* name);
bool llvm_context_add_global(LLVMCodegenContext* ctx, const char* name, LLVMTypeRef type);

// Builder lifecycle management
bool llvm_context_set_insert_point(LLVMCodegenContext* ctx, LLVMBasicBlockRef block);
LLVMBasicBlockRef llvm_context_create_block(LLVMCodegenContext* ctx, const char* name);
bool llvm_context_position_at_end(LLVMCodegenContext* ctx, LLVMBasicBlockRef block);

// Type system integration
LLVMTypeRef llvm_context_get_type(LLVMCodegenContext* ctx, const char* type_name);
bool llvm_context_cache_type(LLVMCodegenContext* ctx, const char* name, LLVMTypeRef type);

// Runtime function management
bool llvm_context_declare_runtime_functions(LLVMCodegenContext* ctx);
LLVMValueRef llvm_context_get_runtime_function(LLVMCodegenContext* ctx, const char* name);

// Error handling integration
void llvm_context_set_error(LLVMCodegenContext* ctx, const char* error_msg);
bool llvm_context_has_errors(LLVMCodegenContext* ctx);
const char* llvm_context_get_last_error(LLVMCodegenContext* ctx);
void llvm_context_clear_errors(LLVMCodegenContext* ctx);

// Validation and debugging
bool llvm_context_validate_state(LLVMCodegenContext* ctx);
void llvm_context_dump_module(LLVMCodegenContext* ctx);

#ifdef WYN_TESTING
// Test functions
void test_llvm_context_create(void);
void test_llvm_context_thread_safety(void);
void test_llvm_context_module_management(void);
void test_llvm_context_builder_lifecycle(void);
void run_llvm_context_tests(void);
#endif

#else
// Fallback when LLVM is not available
typedef struct {
    bool dummy;
} LLVMCodegenContext;

static inline LLVMCodegenContext* llvm_context_create(const char* module_name) { return NULL; }
static inline void llvm_context_destroy(LLVMCodegenContext* ctx) { (void)ctx; }
static inline bool llvm_context_verify_module(LLVMCodegenContext* ctx) { return false; }
#endif

#endif // LLVM_CONTEXT_H