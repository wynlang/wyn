#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

// T2.4.2: Automatic Retain/Release Insertion
// Insert ARC calls at appropriate points and handle function parameters/returns

// Insertion point information
typedef struct InsertionPoint {
    InsertionPointType type;
    ARCOperation operation;
    void* code_location;        // Where to insert the code
    void* object_reference;     // Object being retained/released
    bool is_optimizable;        // Can this be optimized away?
    struct InsertionPoint* next;
} InsertionPoint;

// ARC insertion context
typedef struct {
    InsertionPoint* insertion_points;
    size_t total_insertions;
    size_t optimized_away;
    size_t function_param_insertions;
    size_t function_return_insertions;
    size_t scope_exit_insertions;
    bool auto_insertion_enabled;
    pthread_mutex_t lock;
} ARCInsertionContext;

static ARCInsertionContext g_insertion_context = {0};

// Initialize ARC insertion system
void wyn_arc_insertion_init(void) {
    if (g_insertion_context.auto_insertion_enabled) return;
    
    g_insertion_context.insertion_points = NULL;
    g_insertion_context.total_insertions = 0;
    g_insertion_context.optimized_away = 0;
    g_insertion_context.function_param_insertions = 0;
    g_insertion_context.function_return_insertions = 0;
    g_insertion_context.scope_exit_insertions = 0;
    g_insertion_context.auto_insertion_enabled = true;
    
    pthread_mutex_init(&g_insertion_context.lock, NULL);
}

// Register insertion point
InsertionPoint* wyn_arc_register_insertion(InsertionPointType type, ARCOperation operation, 
                                          void* code_location, void* object_reference) {
    if (!g_insertion_context.auto_insertion_enabled) return NULL;
    
    InsertionPoint* point = malloc(sizeof(InsertionPoint));
    if (!point) return NULL;
    
    point->type = type;
    point->operation = operation;
    point->code_location = code_location;
    point->object_reference = object_reference;
    point->is_optimizable = false;
    point->next = NULL;
    
    pthread_mutex_lock(&g_insertion_context.lock);
    
    // Add to linked list
    point->next = g_insertion_context.insertion_points;
    g_insertion_context.insertion_points = point;
    g_insertion_context.total_insertions++;
    
    // Update type-specific counters
    switch (type) {
        case INSERT_POINT_FUNCTION_PARAM:
            g_insertion_context.function_param_insertions++;
            break;
        case INSERT_POINT_FUNCTION_RETURN:
            g_insertion_context.function_return_insertions++;
            break;
        case INSERT_POINT_SCOPE_EXIT:
            g_insertion_context.scope_exit_insertions++;
            break;
        default:
            break;
    }
    
    pthread_mutex_unlock(&g_insertion_context.lock);
    
    return point;
}

// Analyze insertion point for optimization opportunities
static bool analyze_insertion_optimization(InsertionPoint* point) {
    if (!point) return false;
    
    // Simple optimization heuristics
    switch (point->type) {
        case INSERT_POINT_ASSIGNMENT:
            // Local assignments often don't need retain/release
            return true;
            
        case INSERT_POINT_FUNCTION_PARAM:
            // Parameters passed by value may not need retain
            return (point->operation == ARC_OP_RETAIN);
            
        case INSERT_POINT_FUNCTION_RETURN:
            // Return values often transfer ownership
            return (point->operation == ARC_OP_RELEASE);
            
        case INSERT_POINT_SCOPE_EXIT:
            // Scope exits always need cleanup
            return false;
            
        case INSERT_POINT_CONDITIONAL:
        case INSERT_POINT_LOOP_ENTRY:
        case INSERT_POINT_LOOP_EXIT:
            // Control flow points need careful handling
            return false;
    }
    
    return false;
}

// Optimize insertion points
void wyn_arc_optimize_insertions(void) {
    if (!g_insertion_context.auto_insertion_enabled) return;
    
    pthread_mutex_lock(&g_insertion_context.lock);
    
    InsertionPoint* current = g_insertion_context.insertion_points;
    while (current) {
        current->is_optimizable = analyze_insertion_optimization(current);
        if (current->is_optimizable) {
            g_insertion_context.optimized_away++;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&g_insertion_context.lock);
}

// Insert retain call at assignment
void wyn_arc_insert_assignment_retain(void* code_location, WynObject* obj) {
    if (!obj) return;
    
    InsertionPoint* point = wyn_arc_register_insertion(INSERT_POINT_ASSIGNMENT, ARC_OP_RETAIN,
                                                      code_location, obj);
    
    // Check if this can be optimized away
    if (point && !point->is_optimizable) {
        wyn_arc_retain(obj);
    }
}

// Insert release call at scope exit
void wyn_arc_insert_scope_exit_release(void* code_location, WynObject* obj) {
    if (!obj) return;
    
    InsertionPoint* point = wyn_arc_register_insertion(INSERT_POINT_SCOPE_EXIT, ARC_OP_RELEASE,
                                                      code_location, obj);
    
    // Scope exits always need cleanup
    if (point) {
        wyn_arc_release(obj);
    }
}

// Handle function parameter passing
WynObject* wyn_arc_handle_function_param(WynObject* obj, bool transfer_ownership) {
    if (!obj) return NULL;
    
    InsertionPoint* point = wyn_arc_register_insertion(INSERT_POINT_FUNCTION_PARAM, 
                                                      transfer_ownership ? ARC_OP_MOVE : ARC_OP_RETAIN,
                                                      __builtin_return_address(0), obj);
    
    if (transfer_ownership) {
        // Move semantics - no retain needed
        return obj;
    } else {
        // Copy semantics - retain needed unless optimized away
        if (point && !point->is_optimizable) {
            return wyn_arc_retain(obj);
        }
        return obj;
    }
}

// Handle function return
WynObject* wyn_arc_handle_function_return(WynObject* obj, bool transfer_ownership) {
    if (!obj) return NULL;
    
    InsertionPoint* point = wyn_arc_register_insertion(INSERT_POINT_FUNCTION_RETURN,
                                                      transfer_ownership ? ARC_OP_MOVE : ARC_OP_RETAIN,
                                                      __builtin_return_address(0), obj);
    
    if (transfer_ownership) {
        // Transfer ownership to caller
        return obj;
    } else {
        // Caller gets a retained reference
        if (point && !point->is_optimizable) {
            return wyn_arc_retain(obj);
        }
        return obj;
    }
}

// Insert retain/release for conditional branches
void wyn_arc_handle_conditional_branch(WynObject* obj, bool entering_branch) {
    if (!obj) return;
    
    ARCOperation op = entering_branch ? ARC_OP_RETAIN : ARC_OP_RELEASE;
    InsertionPoint* point = wyn_arc_register_insertion(INSERT_POINT_CONDITIONAL, op,
                                                      __builtin_return_address(0), obj);
    
    if (point && !point->is_optimizable) {
        if (entering_branch) {
            wyn_arc_retain(obj);
        } else {
            wyn_arc_release(obj);
        }
    }
}

// Insert retain/release for loop constructs
void wyn_arc_handle_loop(WynObject* obj, bool entering_loop) {
    if (!obj) return;
    
    InsertionPointType type = entering_loop ? INSERT_POINT_LOOP_ENTRY : INSERT_POINT_LOOP_EXIT;
    ARCOperation op = entering_loop ? ARC_OP_RETAIN : ARC_OP_RELEASE;
    
    InsertionPoint* point = wyn_arc_register_insertion(type, op,
                                                      __builtin_return_address(0), obj);
    
    if (point && !point->is_optimizable) {
        if (entering_loop) {
            wyn_arc_retain(obj);
        } else {
            wyn_arc_release(obj);
        }
    }
}

// Optimize common patterns
void wyn_arc_optimize_common_patterns(void) {
    if (!g_insertion_context.auto_insertion_enabled) return;
    
    pthread_mutex_lock(&g_insertion_context.lock);
    
    // Pattern 1: Retain followed immediately by release - can be eliminated
    InsertionPoint* current = g_insertion_context.insertion_points;
    while (current && current->next) {
        if (current->operation == ARC_OP_RETAIN && 
            current->next->operation == ARC_OP_RELEASE &&
            current->object_reference == current->next->object_reference) {
            
            // Mark both as optimizable
            current->is_optimizable = true;
            current->next->is_optimizable = true;
            g_insertion_context.optimized_away += 2;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&g_insertion_context.lock);
}

// Generate insertion code (placeholder for actual code generation)
void wyn_arc_generate_insertion_code(InsertionPoint* point) {
    if (!point || point->is_optimizable) return;
    
    // In a real implementation, this would generate LLVM IR or C code
    switch (point->operation) {
        case ARC_OP_RETAIN:
            // Generate: wyn_arc_retain(object)
            break;
        case ARC_OP_RELEASE:
            // Generate: wyn_arc_release(object)
            break;
        case ARC_OP_WEAK_RETAIN:
            // Generate: wyn_weak_create(object)
            break;
        case ARC_OP_WEAK_RELEASE:
            // Generate: wyn_weak_destroy(weak_ref)
            break;
        case ARC_OP_MOVE:
            // Generate: move semantics (no retain/release)
            break;
    }
}

// Apply all insertions
void wyn_arc_apply_insertions(void) {
    if (!g_insertion_context.auto_insertion_enabled) return;
    
    pthread_mutex_lock(&g_insertion_context.lock);
    
    InsertionPoint* current = g_insertion_context.insertion_points;
    while (current) {
        wyn_arc_generate_insertion_code(current);
        current = current->next;
    }
    
    pthread_mutex_unlock(&g_insertion_context.lock);
}

// ARC insertion statistics
WynARCInsertionStats wyn_arc_insertion_get_stats(void) {
    WynARCInsertionStats stats = {0};
    
    if (!g_insertion_context.auto_insertion_enabled) return stats;
    
    pthread_mutex_lock(&g_insertion_context.lock);
    
    stats.total_insertions = g_insertion_context.total_insertions;
    stats.optimized_away = g_insertion_context.optimized_away;
    stats.function_param_insertions = g_insertion_context.function_param_insertions;
    stats.function_return_insertions = g_insertion_context.function_return_insertions;
    stats.scope_exit_insertions = g_insertion_context.scope_exit_insertions;
    
    if (stats.total_insertions > 0) {
        stats.optimization_ratio = (double)stats.optimized_away / stats.total_insertions;
    }
    
    pthread_mutex_unlock(&g_insertion_context.lock);
    
    return stats;
}

void wyn_arc_insertion_print_stats(void) {
    WynARCInsertionStats stats = wyn_arc_insertion_get_stats();
    
    printf("=== ARC Insertion Statistics ===\n");
    printf("Total insertions: %zu\n", stats.total_insertions);
    printf("Optimized away: %zu\n", stats.optimized_away);
    printf("Function param insertions: %zu\n", stats.function_param_insertions);
    printf("Function return insertions: %zu\n", stats.function_return_insertions);
    printf("Scope exit insertions: %zu\n", stats.scope_exit_insertions);
    printf("Optimization ratio: %.2f%%\n", stats.optimization_ratio * 100);
    printf("================================\n");
}

// Reset insertion system
void wyn_arc_insertion_reset(void) {
    if (!g_insertion_context.auto_insertion_enabled) return;
    
    pthread_mutex_lock(&g_insertion_context.lock);
    
    // Free all insertion points
    InsertionPoint* current = g_insertion_context.insertion_points;
    while (current) {
        InsertionPoint* next = current->next;
        free(current);
        current = next;
    }
    
    g_insertion_context.insertion_points = NULL;
    g_insertion_context.total_insertions = 0;
    g_insertion_context.optimized_away = 0;
    g_insertion_context.function_param_insertions = 0;
    g_insertion_context.function_return_insertions = 0;
    g_insertion_context.scope_exit_insertions = 0;
    
    pthread_mutex_unlock(&g_insertion_context.lock);
}

// Cleanup insertion system
void wyn_arc_insertion_cleanup(void) {
    if (!g_insertion_context.auto_insertion_enabled) return;
    
    wyn_arc_insertion_reset();
    
    pthread_mutex_destroy(&g_insertion_context.lock);
    g_insertion_context.auto_insertion_enabled = false;
}

// Enable/disable automatic insertion
void wyn_arc_insertion_set_enabled(bool enabled) {
    if (enabled && !g_insertion_context.auto_insertion_enabled) {
        wyn_arc_insertion_init();
    } else if (!enabled && g_insertion_context.auto_insertion_enabled) {
        wyn_arc_insertion_cleanup();
    }
}
