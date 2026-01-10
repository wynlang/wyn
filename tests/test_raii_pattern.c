#include "memory.h"
#include "safe_memory.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s... ", #name); \
        tests_run++; \
        if (test_##name()) { \
            printf("✅ PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("❌ FAILED\n"); \
        } \
    } while(0)

// Test auto_free_cleanup function
bool test_auto_free_cleanup() {
    void* ptr = safe_malloc(100);
    if (!ptr) return false;
    
    // Test cleanup function
    auto_free_cleanup(&ptr);
    
    // Pointer should be NULL after cleanup
    return ptr == NULL;
}

// Test auto cleanup creation
bool test_create_auto_cleanup() {
    void* resource = safe_malloc(50);
    if (!resource) return false;
    
    AutoCleanup* cleanup = create_auto_cleanup(resource, safe_free);
    if (!cleanup) {
        safe_free(resource);
        return false;
    }
    
    bool result = (cleanup->resource == resource) && (cleanup->cleanup_fn == safe_free);
    
    // Clean up manually for this test
    safe_free(resource);
    safe_free(cleanup);
    
    return result;
}

// Test scoped cleanup registration and execution
bool test_scoped_cleanup() {
    // Create some resources
    void* resource1 = safe_malloc(100);
    void* resource2 = safe_malloc(200);
    
    if (!resource1 || !resource2) {
        safe_free(resource1);
        safe_free(resource2);
        return false;
    }
    
    // Register cleanups
    AutoCleanup* cleanup1 = create_auto_cleanup(resource1, safe_free);
    AutoCleanup* cleanup2 = create_auto_cleanup(resource2, safe_free);
    
    if (!cleanup1 || !cleanup2) {
        safe_free(resource1);
        safe_free(resource2);
        safe_free(cleanup1);
        safe_free(cleanup2);
        return false;
    }
    
    register_cleanup(cleanup1);
    register_cleanup(cleanup2);
    
    // Cleanup scope should free all registered resources
    cleanup_scope();
    
    return true; // If we get here without crashes, test passed
}

// Test RAII with expressions (simplified)
bool test_raii_expressions() {
    // This would normally use the AUTO_CLEANUP macro in real code
    // For testing, we'll simulate the pattern
    
    // Create a mock expression
    Expr* expr = safe_malloc(sizeof(Expr));
    if (!expr) return false;
    
    expr->type = EXPR_INT;
    
    // Test auto cleanup
    auto_expr_cleanup(&expr);
    
    // Expression should be NULL after cleanup
    return expr == NULL;
}

// Test edge cases
bool test_edge_cases() {
    // Test with NULL inputs
    auto_free_cleanup(NULL);
    auto_expr_cleanup(NULL);
    auto_stmt_cleanup(NULL);
    auto_program_cleanup(NULL);
    
    // Test create_auto_cleanup with NULL
    AutoCleanup* cleanup1 = create_auto_cleanup(NULL, safe_free);
    AutoCleanup* cleanup2 = create_auto_cleanup(safe_malloc(10), NULL);
    
    if (cleanup1 != NULL || cleanup2 != NULL) {
        safe_free(cleanup1);
        safe_free(cleanup2);
        return false;
    }
    
    // Test register_cleanup with NULL
    register_cleanup(NULL);
    
    // Test cleanup_scope with empty stack
    cleanup_scope();
    
    return true;
}

// Test RAII pattern demonstration
bool test_raii_demonstration() {
    printf("\n--- RAII Pattern Demonstration ---\n");
    
    // Simulate a function that uses RAII pattern
    {
        // In real code, this would use: AUTO_FREE(buffer);
        void* buffer = safe_malloc(1024);
        if (!buffer) return false;
        
        AutoCleanup* cleanup = create_auto_cleanup(buffer, safe_free);
        if (!cleanup) {
            safe_free(buffer);
            return false;
        }
        
        register_cleanup(cleanup);
        
        printf("   Resource allocated and registered for cleanup\n");
        
        // Simulate some work with the buffer
        memset(buffer, 0, 1024);
        
        // At scope exit, cleanup_scope() would be called
        cleanup_scope();
        
        printf("   Resources automatically cleaned up at scope exit\n");
    }
    
    printf("--- End RAII Demonstration ---\n");
    
    return true;
}

// Main test runner
int main() {
    printf("🧪 Testing T1.1.5: RAII Pattern Implementation\n");
    printf("==============================================\n\n");
    
    TEST(auto_free_cleanup);
    TEST(create_auto_cleanup);
    TEST(scoped_cleanup);
    TEST(raii_expressions);
    TEST(edge_cases);
    TEST(raii_demonstration);
    
    printf("\n==============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✅ All T1.1.5 RAII pattern tests PASSED!\n");
        printf("T1.1.5: RAII Pattern Implementation - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.1.5 tests FAILED!\n");
        return 1;
    }
}
