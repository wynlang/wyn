#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include "../src/llvm_context.h"
#include "../src/safe_memory.h"
#include "../src/error.h"

#ifdef WITH_LLVM

// Test data for thread safety test
typedef struct {
    LLVMCodegenContext* ctx;
    int thread_id;
    bool success;
} ThreadTestData;

void* thread_test_function(void* arg) {
    ThreadTestData* data = (ThreadTestData*)arg;
    
    // Test thread-safe operations
    for (int i = 0; i < 10; i++) {
        if (!llvm_context_lock(data->ctx)) {
            data->success = false;
            return NULL;
        }
        
        // Simulate some work
        usleep(1000); // 1ms
        
        llvm_context_unlock(data->ctx);
        usleep(1000); // 1ms
    }
    
    data->success = true;
    return NULL;
}

void test_llvm_context_comprehensive(void) {
    printf("=== Comprehensive LLVM Context Test ===\n");
    
    // Test 1: Basic context creation and destruction
    printf("Test 1: Context creation and destruction...\n");
    LLVMCodegenContext* ctx = llvm_context_create("test_comprehensive");
    assert(ctx != NULL);
    assert(llvm_context_validate_state(ctx));
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 2: Error handling
    printf("Test 2: Error handling...\n");
    ctx = llvm_context_create("test_errors");
    assert(ctx != NULL);
    
    llvm_context_set_error(ctx, "Test error message");
    assert(llvm_context_has_errors(ctx));
    assert(strcmp(llvm_context_get_last_error(ctx), "Test error message") == 0);
    
    llvm_context_clear_errors(ctx);
    assert(!llvm_context_has_errors(ctx));
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 3: Type system integration
    printf("Test 3: Type system integration...\n");
    ctx = llvm_context_create("test_types");
    assert(ctx != NULL);
    
    LLVMTypeRef int_type = llvm_context_get_type(ctx, "int");
    LLVMTypeRef float_type = llvm_context_get_type(ctx, "float");
    LLVMTypeRef bool_type = llvm_context_get_type(ctx, "bool");
    LLVMTypeRef string_type = llvm_context_get_type(ctx, "string");
    LLVMTypeRef void_type = llvm_context_get_type(ctx, "void");
    
    assert(int_type != NULL);
    assert(float_type != NULL);
    assert(bool_type != NULL);
    assert(string_type != NULL);
    assert(void_type != NULL);
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 4: Runtime function management
    printf("Test 4: Runtime function management...\n");
    ctx = llvm_context_create("test_runtime");
    assert(ctx != NULL);
    
    LLVMValueRef malloc_fn = llvm_context_get_runtime_function(ctx, "malloc");
    LLVMValueRef free_fn = llvm_context_get_runtime_function(ctx, "free");
    LLVMValueRef printf_fn = llvm_context_get_runtime_function(ctx, "printf");
    
    assert(malloc_fn != NULL);
    assert(free_fn != NULL);
    assert(printf_fn != NULL);
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 5: Module management
    printf("Test 5: Module management...\n");
    ctx = llvm_context_create("test_module");
    assert(ctx != NULL);
    
    assert(llvm_context_set_module_name(ctx, "new_test_module"));
    
    LLVMTypeRef int_type_for_global = llvm_context_get_type(ctx, "int");
    assert(llvm_context_add_global(ctx, "test_global", int_type_for_global));
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 6: Builder lifecycle
    printf("Test 6: Builder lifecycle...\n");
    ctx = llvm_context_create("test_builder");
    assert(ctx != NULL);
    
    // Create a function to work with
    LLVMTypeRef func_type = LLVMFunctionType(llvm_context_get_type(ctx, "void"), NULL, 0, false);
    
    if (llvm_context_lock(ctx)) {
        ctx->current_function = LLVMAddFunction(ctx->module, "test_func", func_type);
        llvm_context_unlock(ctx);
    }
    
    LLVMBasicBlockRef block = llvm_context_create_block(ctx, "test_block");
    assert(block != NULL);
    
    assert(llvm_context_position_at_end(ctx, block));
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 7: Thread safety
    printf("Test 7: Thread safety...\n");
    ctx = llvm_context_create("test_threading");
    assert(ctx != NULL);
    
    const int num_threads = 4;
    pthread_t threads[num_threads];
    ThreadTestData thread_data[num_threads];
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].thread_id = i;
        thread_data[i].success = false;
        
        int result = pthread_create(&threads[i], NULL, thread_test_function, &thread_data[i]);
        assert(result == 0);
    }
    
    // Wait for threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        assert(thread_data[i].success);
    }
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    // Test 8: Module verification
    printf("Test 8: Module verification...\n");
    ctx = llvm_context_create("test_verification");
    assert(ctx != NULL);
    
    // Create a simple valid function
    LLVMTypeRef main_type = LLVMFunctionType(llvm_context_get_type(ctx, "int"), NULL, 0, false);
    
    if (llvm_context_lock(ctx)) {
        LLVMValueRef main_func = LLVMAddFunction(ctx->module, "main", main_type);
        ctx->current_function = main_func;
        
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, main_func, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        
        LLVMValueRef zero = LLVMConstInt(llvm_context_get_type(ctx, "int"), 0, false);
        LLVMBuildRet(ctx->builder, zero);
        
        llvm_context_unlock(ctx);
    }
    
    assert(llvm_context_verify_module(ctx));
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    printf("=== All LLVM Context Tests Passed! ===\n");
}

void test_llvm_context_edge_cases(void) {
    printf("=== LLVM Context Edge Cases Test ===\n");
    
    // Test NULL parameter handling
    printf("Test: NULL parameter handling...\n");
    assert(llvm_context_create(NULL) == NULL);
    assert(!llvm_context_verify_module(NULL));
    assert(!llvm_context_lock(NULL));
    llvm_context_unlock(NULL); // Should not crash
    llvm_context_destroy(NULL); // Should not crash
    printf("✅ PASS\n");
    
    // Test invalid operations
    printf("Test: Invalid operations...\n");
    LLVMCodegenContext* ctx = llvm_context_create("edge_test");
    assert(ctx != NULL);
    
    // Try to create block without current function
    LLVMBasicBlockRef block = llvm_context_create_block(ctx, "invalid_block");
    assert(block == NULL);
    
    llvm_context_destroy(ctx);
    printf("✅ PASS\n");
    
    printf("=== Edge Cases Tests Passed! ===\n");
}

int main(void) {
    printf("Starting LLVM Context Management Tests (T2.1.2)\n");
    printf("==============================================\n");
    
    // Run comprehensive tests
    test_llvm_context_comprehensive();
    
    // Run edge case tests
    test_llvm_context_edge_cases();
    
    // Run the built-in test suite
    #ifdef WYN_TESTING
    run_llvm_context_tests();
    #endif
    
    printf("\n🎉 All T2.1.2 tests completed successfully!\n");
    printf("LLVM Context Management implementation is working correctly.\n");
    
    return 0;
}

#else

int main(void) {
    printf("LLVM not available - skipping LLVM context tests\n");
    return 0;
}

#endif