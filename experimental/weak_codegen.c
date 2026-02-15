#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

// T2.4.3: Weak Reference Code Generation
// Provides compiler integration for weak reference syntax and automatic conversions

// Weak reference code generation context
typedef struct {
    bool weak_codegen_enabled;
    size_t weak_conversions_generated;
    size_t null_checks_generated;
    size_t strong_promotions_generated;
    size_t weak_assignments_generated;
    pthread_mutex_t lock;
} WynWeakCodegenContext;

static WynWeakCodegenContext g_weak_codegen_context = {
    .weak_codegen_enabled = true,
    .weak_conversions_generated = 0,
    .null_checks_generated = 0,
    .strong_promotions_generated = 0,
    .weak_assignments_generated = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

// Initialize weak reference code generation
void wyn_weak_codegen_init(void) {
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    g_weak_codegen_context.weak_codegen_enabled = true;
    g_weak_codegen_context.weak_conversions_generated = 0;
    g_weak_codegen_context.null_checks_generated = 0;
    g_weak_codegen_context.strong_promotions_generated = 0;
    g_weak_codegen_context.weak_assignments_generated = 0;
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}

// Generate code for weak reference creation from strong reference
void* wyn_weak_codegen_create_weak(void* strong_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !strong_ref) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Create weak reference using existing weak reference system
    WynWeakRef* weak_ref = wyn_weak_create((WynObject*)strong_ref);
    
    if (weak_ref) {
        g_weak_codegen_context.weak_conversions_generated++;
    }
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
    
    return weak_ref;
}

// Generate code for strong reference promotion from weak reference
void* wyn_weak_codegen_promote_to_strong(void* weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_ref) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Promote weak reference to strong reference
    WynObject* strong_ref = wyn_weak_promote((WynWeakRef*)weak_ref);
    
    if (strong_ref) {
        g_weak_codegen_context.strong_promotions_generated++;
    }
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
    
    return strong_ref;
}

// Generate null check code for weak reference access
bool wyn_weak_codegen_null_check(void* weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_ref) {
        return false;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Check if weak reference is valid
    bool is_valid = wyn_weak_is_valid((WynWeakRef*)weak_ref);
    
    g_weak_codegen_context.null_checks_generated++;
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
    
    return is_valid;
}

// Generate code for weak reference assignment
void wyn_weak_codegen_assign_weak(void** weak_ref_ptr, void* source_weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_ref_ptr) {
        return;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Destroy existing weak reference if any
    if (*weak_ref_ptr) {
        wyn_weak_destroy((WynWeakRef*)*weak_ref_ptr);
    }
    
    // Assign new weak reference (create copy if source exists)
    if (source_weak_ref) {
        WynObject* obj = wyn_weak_access((WynWeakRef*)source_weak_ref);
        if (obj) {
            *weak_ref_ptr = wyn_weak_create(obj);
        } else {
            *weak_ref_ptr = NULL;
        }
    } else {
        *weak_ref_ptr = NULL;
    }
    
    g_weak_codegen_context.weak_assignments_generated++;
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}

// Generate automatic strong-to-weak conversion
void* wyn_weak_codegen_auto_convert_to_weak(void* strong_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !strong_ref) {
        return NULL;
    }
    
    // Automatic conversion: create weak reference without affecting strong reference count
    return wyn_weak_codegen_create_weak(strong_ref, code_location);
}

// Generate automatic weak-to-strong conversion with null safety
void* wyn_weak_codegen_auto_convert_to_strong(void* weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_ref) {
        return NULL;
    }
    
    // Automatic conversion: promote weak reference to strong with null check
    if (wyn_weak_codegen_null_check(weak_ref, code_location)) {
        return wyn_weak_codegen_promote_to_strong(weak_ref, code_location);
    }
    
    return NULL;
}

// Generate conditional weak access with null checking
void* wyn_weak_codegen_safe_access(void* weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_ref) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Safe access: check validity before accessing
    WynObject* obj = NULL;
    if (wyn_weak_is_valid((WynWeakRef*)weak_ref)) {
        obj = wyn_weak_access((WynWeakRef*)weak_ref);
        g_weak_codegen_context.null_checks_generated++;
    }
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
    
    return obj;
}

// Generate weak reference cleanup code
void wyn_weak_codegen_cleanup_weak(void* weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_ref) {
        return;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Clean up weak reference
    wyn_weak_destroy((WynWeakRef*)weak_ref);
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}

// Generate weak reference comparison code
bool wyn_weak_codegen_compare_weak(void* weak_ref1, void* weak_ref2, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled) {
        return false;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    bool result = false;
    
    // Compare weak references by comparing their target objects
    if (!weak_ref1 && !weak_ref2) {
        result = true; // Both null
    } else if (weak_ref1 && weak_ref2) {
        WynObject* obj1 = wyn_weak_access((WynWeakRef*)weak_ref1);
        WynObject* obj2 = wyn_weak_access((WynWeakRef*)weak_ref2);
        result = (obj1 == obj2);
    }
    // else: one null, one not null -> false
    
    g_weak_codegen_context.null_checks_generated += 2; // Two null checks performed
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
    
    return result;
}

// Generate weak reference array handling
void wyn_weak_codegen_array_set_weak(void** weak_array, size_t index, void* weak_ref, void* code_location) {
    if (!g_weak_codegen_context.weak_codegen_enabled || !weak_array) {
        return;
    }
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    // Set weak reference in array with proper cleanup
    if (weak_array[index]) {
        wyn_weak_destroy((WynWeakRef*)weak_array[index]);
    }
    
    // Create copy of weak reference for array storage
    if (weak_ref) {
        WynObject* obj = wyn_weak_access((WynWeakRef*)weak_ref);
        if (obj) {
            weak_array[index] = wyn_weak_create(obj);
        } else {
            weak_array[index] = NULL;
        }
    } else {
        weak_array[index] = NULL;
    }
    
    g_weak_codegen_context.weak_assignments_generated++;
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}

// Get weak reference code generation statistics
WynWeakCodegenStats wyn_weak_codegen_get_stats(void) {
    WynWeakCodegenStats stats = {0};
    
    if (!g_weak_codegen_context.weak_codegen_enabled) return stats;
    
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    stats.weak_conversions_generated = g_weak_codegen_context.weak_conversions_generated;
    stats.null_checks_generated = g_weak_codegen_context.null_checks_generated;
    stats.strong_promotions_generated = g_weak_codegen_context.strong_promotions_generated;
    stats.weak_assignments_generated = g_weak_codegen_context.weak_assignments_generated;
    
    // Calculate ratios
    size_t total_operations = stats.weak_conversions_generated + stats.strong_promotions_generated + stats.weak_assignments_generated;
    if (total_operations > 0) {
        stats.null_safety_ratio = (double)stats.null_checks_generated / total_operations;
        stats.conversion_efficiency = (double)(stats.weak_conversions_generated + stats.strong_promotions_generated) / total_operations;
    }
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
    
    return stats;
}

// Print weak reference code generation statistics
void wyn_weak_codegen_print_stats(void) {
    WynWeakCodegenStats stats = wyn_weak_codegen_get_stats();
    
    printf("=== Weak Reference Code Generation Statistics ===\n");
    printf("Weak conversions generated: %zu\n", stats.weak_conversions_generated);
    printf("Strong promotions generated: %zu\n", stats.strong_promotions_generated);
    printf("Null checks generated: %zu\n", stats.null_checks_generated);
    printf("Weak assignments generated: %zu\n", stats.weak_assignments_generated);
    printf("Null safety ratio: %.2f%%\n", stats.null_safety_ratio * 100.0);
    printf("Conversion efficiency: %.2f%%\n", stats.conversion_efficiency * 100.0);
    printf("================================================\n");
}

// Reset weak reference code generation statistics
void wyn_weak_codegen_reset(void) {
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    g_weak_codegen_context.weak_conversions_generated = 0;
    g_weak_codegen_context.null_checks_generated = 0;
    g_weak_codegen_context.strong_promotions_generated = 0;
    g_weak_codegen_context.weak_assignments_generated = 0;
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}

// Cleanup weak reference code generation system
void wyn_weak_codegen_cleanup(void) {
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    
    g_weak_codegen_context.weak_codegen_enabled = false;
    
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}

// Enable/disable weak reference code generation
void wyn_weak_codegen_set_enabled(bool enabled) {
    pthread_mutex_lock(&g_weak_codegen_context.lock);
    g_weak_codegen_context.weak_codegen_enabled = enabled;
    pthread_mutex_unlock(&g_weak_codegen_context.lock);
}
