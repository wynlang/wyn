#include "llvm_context.h"

#ifdef WITH_LLVM

#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <stdio.h>
#include <string.h>
#include "safe_memory.h"
#include "error.h"

// Symbol table structures
struct LLVMSymbolTableEntry {
    char* name;
    LLVMValueRef value;
    LLVMTypeRef type;  // Track the type
    LLVMSymbolTableEntry* next;
};

struct LLVMSymbolTable {
    LLVMSymbolTableEntry* entries;
    LLVMSymbolTable* parent;
};

// Context lifecycle management
LLVMCodegenContext* llvm_context_create(const char* module_name) {
    if (!module_name) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Module name cannot be null");
        return NULL;
    }
    
    // Allocate context with safe memory
    LLVMCodegenContext* ctx = safe_malloc(sizeof(LLVMCodegenContext));
    if (!ctx) {
        report_error(ERR_OUT_OF_MEMORY, NULL, 0, 0, "Failed to allocate LLVM context");
        return NULL;
    }
    
    // Initialize thread safety
    if (pthread_mutex_init(&ctx->context_mutex, NULL) != 0) {
        safe_free(ctx);
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to initialize context mutex");
        return NULL;
    }
    
    // Initialize LLVM components
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    
    // Create LLVM context
    ctx->context = LLVMContextCreate();
    if (!ctx->context) {
        pthread_mutex_destroy(&ctx->context_mutex);
        safe_free(ctx);
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to create LLVM context");
        return NULL;
    }
    
    // Create module
    ctx->module = LLVMModuleCreateWithNameInContext(module_name, ctx->context);
    if (!ctx->module) {
        LLVMContextDispose(ctx->context);
        pthread_mutex_destroy(&ctx->context_mutex);
        safe_free(ctx);
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to create LLVM module");
        return NULL;
    }
    
    // Create builder
    ctx->builder = LLVMCreateBuilderInContext(ctx->context);
    if (!ctx->builder) {
        LLVMDisposeModule(ctx->module);
        LLVMContextDispose(ctx->context);
        pthread_mutex_destroy(&ctx->context_mutex);
        safe_free(ctx);
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to create LLVM builder");
        return NULL;
    }
    
    // Initialize type cache
    ctx->int_type = LLVMInt32TypeInContext(ctx->context);
    ctx->float_type = LLVMDoubleTypeInContext(ctx->context);
    ctx->bool_type = LLVMInt1TypeInContext(ctx->context);
    ctx->void_type = LLVMVoidTypeInContext(ctx->context);
    
    // String type as i8* (char pointer)
    ctx->string_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    
    // Initialize state
    ctx->current_function = NULL;
    ctx->current_block = NULL;
    ctx->target_machine = NULL;
    ctx->is_initialized = true;
    ctx->has_errors = false;
    ctx->last_error = NULL;
    ctx->current_loop_end = NULL;
    ctx->current_loop_header = NULL;
    ctx->spawn_counter = 0;
    
    // Initialize symbol table
    ctx->symbol_table = symbol_table_create(NULL);
    
    // Store module name
    ctx->module_name = safe_strdup(module_name);
    if (!ctx->module_name) {
        llvm_context_destroy(ctx);
        report_error(ERR_OUT_OF_MEMORY, NULL, 0, 0, "Failed to store module name");
        return NULL;
    }
    
    // Initialize runtime functions to NULL
    ctx->runtime_functions.malloc_fn = NULL;
    ctx->runtime_functions.free_fn = NULL;
    ctx->runtime_functions.retain_fn = NULL;
    ctx->runtime_functions.release_fn = NULL;
    ctx->runtime_functions.printf_fn = NULL;
    
    // Declare runtime functions
    if (!llvm_context_declare_runtime_functions(ctx)) {
        llvm_context_destroy(ctx);
        return NULL;
    }
    
    return ctx;
}

void llvm_context_destroy(LLVMCodegenContext* ctx) {
    if (!ctx) return;
    
    // Thread-safe cleanup
    llvm_context_lock(ctx);
    
    // Clean up LLVM resources
    if (ctx->builder) {
        LLVMDisposeBuilder(ctx->builder);
        ctx->builder = NULL;
    }
    
    if (ctx->target_machine) {
        LLVMDisposeTargetMachine(ctx->target_machine);
        ctx->target_machine = NULL;
    }
    
    if (ctx->module) {
        LLVMDisposeModule(ctx->module);
        ctx->module = NULL;
    }
    
    if (ctx->context) {
        LLVMContextDispose(ctx->context);
        ctx->context = NULL;
    }
    
    // Clean up strings
    if (ctx->module_name) {
        safe_free(ctx->module_name);
        ctx->module_name = NULL;
    }
    
    if (ctx->last_error) {
        safe_free(ctx->last_error);
        ctx->last_error = NULL;
    }
    
    ctx->is_initialized = false;
    
    llvm_context_unlock(ctx);
    pthread_mutex_destroy(&ctx->context_mutex);
    safe_free(ctx);
}

bool llvm_context_verify_module(LLVMCodegenContext* ctx) {
    if (!ctx || !ctx->module) {
        return false;
    }
    
    if (!llvm_context_lock(ctx)) {
        return false;
    }
    
    char* error_msg = NULL;
    bool is_valid = !LLVMVerifyModule(ctx->module, LLVMReturnStatusAction, &error_msg);
    
    if (!is_valid && error_msg) {
        llvm_context_set_error(ctx, error_msg);
        LLVMDisposeMessage(error_msg);
    }
    
    llvm_context_unlock(ctx);
    return is_valid;
}

// Thread-safe context operations
bool llvm_context_lock(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    return pthread_mutex_lock(&ctx->context_mutex) == 0;
}

void llvm_context_unlock(LLVMCodegenContext* ctx) {
    if (!ctx) return;
    pthread_mutex_unlock(&ctx->context_mutex);
}

// Module management
bool llvm_context_set_module_name(LLVMCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return false;
    
    if (!llvm_context_lock(ctx)) return false;
    
    if (ctx->module_name) {
        safe_free(ctx->module_name);
    }
    
    ctx->module_name = safe_strdup(name);
    bool success = (ctx->module_name != NULL);
    
    if (success && ctx->module) {
        LLVMSetModuleIdentifier(ctx->module, name, strlen(name));
    }
    
    llvm_context_unlock(ctx);
    return success;
}

bool llvm_context_add_global(LLVMCodegenContext* ctx, const char* name, LLVMTypeRef type) {
    if (!ctx || !name || !type) return false;
    
    if (!llvm_context_lock(ctx)) return false;
    
    LLVMValueRef global = LLVMAddGlobal(ctx->module, type, name);
    bool success = (global != NULL);
    
    llvm_context_unlock(ctx);
    return success;
}

// Builder lifecycle management
bool llvm_context_set_insert_point(LLVMCodegenContext* ctx, LLVMBasicBlockRef block) {
    if (!ctx || !block) return false;
    
    if (!llvm_context_lock(ctx)) return false;
    
    LLVMPositionBuilderAtEnd(ctx->builder, block);
    ctx->current_block = block;
    
    llvm_context_unlock(ctx);
    return true;
}

LLVMBasicBlockRef llvm_context_create_block(LLVMCodegenContext* ctx, const char* name) {
    if (!ctx || !ctx->current_function) return NULL;
    
    if (!llvm_context_lock(ctx)) return NULL;
    
    LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, name ? name : "block"
    );
    
    llvm_context_unlock(ctx);
    return block;
}

bool llvm_context_position_at_end(LLVMCodegenContext* ctx, LLVMBasicBlockRef block) {
    if (!ctx || !block) return false;
    
    if (!llvm_context_lock(ctx)) return false;
    
    LLVMPositionBuilderAtEnd(ctx->builder, block);
    ctx->current_block = block;
    
    llvm_context_unlock(ctx);
    return true;
}

// Type system integration
LLVMTypeRef llvm_context_get_type(LLVMCodegenContext* ctx, const char* type_name) {
    if (!ctx || !type_name) return NULL;
    
    if (!llvm_context_lock(ctx)) return NULL;
    
    LLVMTypeRef type = NULL;
    
    if (strcmp(type_name, "int") == 0) {
        type = ctx->int_type;
    } else if (strcmp(type_name, "float") == 0) {
        type = ctx->float_type;
    } else if (strcmp(type_name, "bool") == 0) {
        type = ctx->bool_type;
    } else if (strcmp(type_name, "string") == 0) {
        type = ctx->string_type;
    } else if (strcmp(type_name, "void") == 0) {
        type = ctx->void_type;
    }
    
    llvm_context_unlock(ctx);
    return type;
}

bool llvm_context_cache_type(LLVMCodegenContext* ctx, const char* name, LLVMTypeRef type) {
    if (!ctx || !name || !type) return false;
    
    // For now, we only support the basic types in the cache
    // This can be extended with a hash table for custom types
    return true;
}

// Runtime function management
bool llvm_context_declare_runtime_functions(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    if (!llvm_context_lock(ctx)) return false;
    
    // Declare malloc: i8* malloc(i64)
    LLVMTypeRef malloc_args[] = { LLVMInt64TypeInContext(ctx->context) };
    LLVMTypeRef malloc_type = LLVMFunctionType(
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
        malloc_args, 1, false
    );
    ctx->runtime_functions.malloc_fn = LLVMAddFunction(ctx->module, "malloc", malloc_type);
    
    // Declare free: void free(i8*)
    LLVMTypeRef free_args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
    LLVMTypeRef free_type = LLVMFunctionType(ctx->void_type, free_args, 1, false);
    ctx->runtime_functions.free_fn = LLVMAddFunction(ctx->module, "free", free_type);
    
    // Declare printf: i32 printf(i8*, ...)
    LLVMTypeRef printf_args[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
    LLVMTypeRef printf_type = LLVMFunctionType(ctx->int_type, printf_args, 1, true);
    ctx->runtime_functions.printf_fn = LLVMAddFunction(ctx->module, "printf", printf_type);
    
    // ARC functions will be declared later in T2.3
    ctx->runtime_functions.retain_fn = NULL;
    ctx->runtime_functions.release_fn = NULL;
    
    bool success = (ctx->runtime_functions.malloc_fn != NULL &&
                   ctx->runtime_functions.free_fn != NULL &&
                   ctx->runtime_functions.printf_fn != NULL);
    
    llvm_context_unlock(ctx);
    return success;
}

LLVMValueRef llvm_context_get_runtime_function(LLVMCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    
    if (!llvm_context_lock(ctx)) return NULL;
    
    LLVMValueRef func = NULL;
    
    if (strcmp(name, "malloc") == 0) {
        func = ctx->runtime_functions.malloc_fn;
    } else if (strcmp(name, "free") == 0) {
        func = ctx->runtime_functions.free_fn;
    } else if (strcmp(name, "printf") == 0) {
        func = ctx->runtime_functions.printf_fn;
    } else if (strcmp(name, "retain") == 0) {
        func = ctx->runtime_functions.retain_fn;
    } else if (strcmp(name, "release") == 0) {
        func = ctx->runtime_functions.release_fn;
    }
    
    llvm_context_unlock(ctx);
    return func;
}

// Error handling integration
void llvm_context_set_error(LLVMCodegenContext* ctx, const char* error_msg) {
    if (!ctx || !error_msg) return;
    
    if (ctx->last_error) {
        safe_free(ctx->last_error);
    }
    
    ctx->last_error = safe_strdup(error_msg);
    ctx->has_errors = true;
    
    // Also report to global error system
    report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, error_msg);
}

bool llvm_context_has_errors(LLVMCodegenContext* ctx) {
    return ctx ? ctx->has_errors : true;
}

const char* llvm_context_get_last_error(LLVMCodegenContext* ctx) {
    return ctx ? ctx->last_error : "Invalid context";
}

void llvm_context_clear_errors(LLVMCodegenContext* ctx) {
    if (!ctx) return;
    
    if (ctx->last_error) {
        safe_free(ctx->last_error);
        ctx->last_error = NULL;
    }
    
    ctx->has_errors = false;
}

// Validation and debugging
bool llvm_context_validate_state(LLVMCodegenContext* ctx) {
    if (!ctx) return false;
    
    if (!llvm_context_lock(ctx)) return false;
    
    bool valid = (ctx->is_initialized &&
                 ctx->context != NULL &&
                 ctx->module != NULL &&
                 ctx->builder != NULL &&
                 ctx->module_name != NULL);
    
    llvm_context_unlock(ctx);
    return valid;
}

void llvm_context_dump_module(LLVMCodegenContext* ctx) {
    if (!ctx || !ctx->module) return;
    
    if (!llvm_context_lock(ctx)) return;
    
    char* ir_string = LLVMPrintModuleToString(ctx->module);
    if (ir_string) {
        printf("=== LLVM IR for module '%s' ===\n", ctx->module_name);
        printf("%s\n", ir_string);
        printf("=== End LLVM IR ===\n");
        LLVMDisposeMessage(ir_string);
    }
    
    llvm_context_unlock(ctx);
}

#ifdef WYN_TESTING
// Test functions
void test_llvm_context_create(void) {
    printf("Testing LLVM context creation...\n");
    
    LLVMCodegenContext* ctx = llvm_context_create("test_module");
    if (!ctx) {
        printf("FAIL: Could not create LLVM context\n");
        return;
    }
    
    if (!llvm_context_validate_state(ctx)) {
        printf("FAIL: Context state validation failed\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    if (strcmp(ctx->module_name, "test_module") != 0) {
        printf("FAIL: Module name mismatch\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    llvm_context_destroy(ctx);
    printf("PASS: LLVM context creation test\n");
}

void test_llvm_context_thread_safety(void) {
    printf("Testing LLVM context thread safety...\n");
    
    LLVMCodegenContext* ctx = llvm_context_create("thread_test");
    if (!ctx) {
        printf("FAIL: Could not create context for thread test\n");
        return;
    }
    
    // Test locking mechanism
    if (!llvm_context_lock(ctx)) {
        printf("FAIL: Could not acquire context lock\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    llvm_context_unlock(ctx);
    
    llvm_context_destroy(ctx);
    printf("PASS: LLVM context thread safety test\n");
}

void test_llvm_context_module_management(void) {
    printf("Testing LLVM context module management...\n");
    
    LLVMCodegenContext* ctx = llvm_context_create("module_test");
    if (!ctx) {
        printf("FAIL: Could not create context for module test\n");
        return;
    }
    
    // Test module name change
    if (!llvm_context_set_module_name(ctx, "new_module_name")) {
        printf("FAIL: Could not set module name\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    // Test global addition
    LLVMTypeRef int_type = llvm_context_get_type(ctx, "int");
    if (!int_type) {
        printf("FAIL: Could not get int type\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    if (!llvm_context_add_global(ctx, "test_global", int_type)) {
        printf("FAIL: Could not add global variable\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    llvm_context_destroy(ctx);
    printf("PASS: LLVM context module management test\n");
}

void test_llvm_context_builder_lifecycle(void) {
    printf("Testing LLVM context builder lifecycle...\n");
    
    LLVMCodegenContext* ctx = llvm_context_create("builder_test");
    if (!ctx) {
        printf("FAIL: Could not create context for builder test\n");
        return;
    }
    
    // Create a test function to work with
    LLVMTypeRef func_type = LLVMFunctionType(ctx->void_type, NULL, 0, false);
    ctx->current_function = LLVMAddFunction(ctx->module, "test_func", func_type);
    
    // Test block creation
    LLVMBasicBlockRef block = llvm_context_create_block(ctx, "entry");
    if (!block) {
        printf("FAIL: Could not create basic block\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    // Test positioning builder
    if (!llvm_context_position_at_end(ctx, block)) {
        printf("FAIL: Could not position builder at end of block\n");
        llvm_context_destroy(ctx);
        return;
    }
    
    llvm_context_destroy(ctx);
    printf("PASS: LLVM context builder lifecycle test\n");
}

void run_llvm_context_tests(void) {
    printf("=== Running LLVM Context Tests ===\n");
    test_llvm_context_create();
    test_llvm_context_thread_safety();
    test_llvm_context_module_management();
    test_llvm_context_builder_lifecycle();
    printf("=== LLVM Context Tests Complete ===\n");
}
#endif // WYN_TESTING

// Symbol table operations
LLVMSymbolTable* symbol_table_create(LLVMSymbolTable* parent) {
    LLVMSymbolTable* table = (LLVMSymbolTable*)malloc(sizeof(LLVMSymbolTable));
    if (!table) return NULL;
    table->entries = NULL;
    table->parent = parent;
    return table;
}

void symbol_table_destroy(LLVMSymbolTable* table) {
    if (!table) return;
    
    LLVMSymbolTableEntry* entry = table->entries;
    while (entry) {
        LLVMSymbolTableEntry* next = entry->next;
        free(entry->name);
        free(entry);
        entry = next;
    }
    free(table);
}

void symbol_table_insert(LLVMSymbolTable* table, const char* name, LLVMValueRef value) {
    symbol_table_insert_typed(table, name, value, NULL);
}

void symbol_table_insert_typed(LLVMSymbolTable* table, const char* name, LLVMValueRef value, LLVMTypeRef type) {
    if (!table || !name) return;
    
    LLVMSymbolTableEntry* entry = (LLVMSymbolTableEntry*)malloc(sizeof(LLVMSymbolTableEntry));
    if (!entry) return;
    
    entry->name = strdup(name);
    entry->value = value;
    entry->type = type;
    entry->next = table->entries;
    table->entries = entry;
}

LLVMTypeRef symbol_table_lookup_type(LLVMSymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    // Search current scope
    for (LLVMSymbolTableEntry* entry = table->entries; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry->type;
        }
    }
    
    // Search parent scope
    if (table->parent) {
        return symbol_table_lookup_type(table->parent, name);
    }
    
    return NULL;
}

LLVMValueRef symbol_table_lookup(LLVMSymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    // Search current scope
    for (LLVMSymbolTableEntry* entry = table->entries; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry->value;
        }
    }
    
    // Search parent scope
    if (table->parent) {
        return symbol_table_lookup(table->parent, name);
    }
    
    return NULL;
}

void symbol_table_push_scope(LLVMCodegenContext* ctx) {
    if (!ctx) return;
    ctx->symbol_table = symbol_table_create(ctx->symbol_table);
}

void symbol_table_pop_scope(LLVMCodegenContext* ctx) {
    if (!ctx || !ctx->symbol_table) return;
    
    LLVMSymbolTable* old_table = ctx->symbol_table;
    ctx->symbol_table = old_table->parent;
    symbol_table_destroy(old_table);
}

#else
// Fallback implementations when LLVM is not available
LLVMCodegenContext* llvm_context_create(const char* module_name) {
    (void)module_name;
    return NULL;
}

void llvm_context_destroy(LLVMCodegenContext* ctx) {
    (void)ctx;
}

bool llvm_context_verify_module(LLVMCodegenContext* ctx) {
    (void)ctx;
    return false;
}
#endif