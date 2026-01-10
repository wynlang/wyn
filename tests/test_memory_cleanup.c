#include "memory.h"
#include "security.h"
#include <stdio.h>
#include <assert.h>

// Test memory cleanup functions
int main() {
    printf("🧠 Testing Memory Cleanup Implementation\n");
    printf("========================================\n\n");
    
    // Initialize memory tracking
    init_memory_tracking();
    
    // Test 1: Create and free a simple expression
    printf("Test 1: Simple expression cleanup... ");
    Expr* expr = safe_calloc(1, sizeof(Expr));
    expr->type = EXPR_INT;
    free_expr(expr);
    printf("✅ PASSED\n");
    
    // Test 2: Create and free a binary expression
    printf("Test 2: Binary expression cleanup... ");
    Expr* binary = safe_calloc(1, sizeof(Expr));
    binary->type = EXPR_BINARY;
    binary->binary.left = safe_calloc(1, sizeof(Expr));
    binary->binary.left->type = EXPR_INT;
    binary->binary.right = safe_calloc(1, sizeof(Expr));
    binary->binary.right->type = EXPR_INT;
    free_expr(binary);
    printf("✅ PASSED\n");
    
    // Test 3: Create and free a program
    printf("Test 3: Program cleanup... ");
    Program* prog = safe_calloc(1, sizeof(Program));
    prog->stmts = safe_malloc(sizeof(Stmt*) * 2);
    prog->count = 2;
    
    // Add two simple statements
    prog->stmts[0] = safe_calloc(1, sizeof(Stmt));
    prog->stmts[0]->type = STMT_EXPR;
    prog->stmts[0]->expr = safe_calloc(1, sizeof(Expr));
    prog->stmts[0]->expr->type = EXPR_INT;
    
    prog->stmts[1] = safe_calloc(1, sizeof(Stmt));
    prog->stmts[1]->type = STMT_EXPR;
    prog->stmts[1]->expr = safe_calloc(1, sizeof(Expr));
    prog->stmts[1]->expr->type = EXPR_INT;
    
    free_program(prog);
    printf("✅ PASSED\n");
    
    // Check for memory leaks
    printf("\nMemory Leak Check:\n");
    if (has_memory_leaks()) {
        printf("❌ Memory leaks detected!\n");
        print_memory_leaks();
        cleanup_memory_tracking();
        return 1;
    } else {
        printf("✅ No memory leaks detected!\n");
    }
    
    printf("\nTotal allocated memory: %zu bytes\n", get_allocated_memory());
    printf("Total allocations: %zu\n", get_allocation_count());
    
    cleanup_memory_tracking();
    
    printf("\n🎉 All memory cleanup tests PASSED!\n");
    printf("T1.1.3: Fix Memory Leaks - COMPLETED ✅\n");
    
    return 0;
}
