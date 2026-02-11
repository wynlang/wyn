#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

// T2.3.2: Reference Counting Operations
// Enhanced atomic reference counting with performance optimizations

// Performance counters for monitoring
static _Atomic size_t g_retain_operations = 0;
static _Atomic size_t g_release_operations = 0;
static _Atomic size_t g_retain_fast_path = 0;
static _Atomic size_t g_release_fast_path = 0;
static _Atomic size_t g_overflow_checks = 0;
static _Atomic size_t g_double_release_checks = 0;

// Fast path optimization: inline retain for common case
static inline WynObject* wyn_arc_retain_fast(WynObject* obj) {
    if (!obj) return NULL;
    
    // Fast path: try simple atomic increment first
    uint32_t old_count = atomic_load_explicit(&obj->header.ref_count, memory_order_relaxed);
    
    // Check if this is a simple case (no weak flag, no overflow risk)
    if (!(old_count & ARC_WEAK_FLAG) && (old_count & ARC_COUNT_MASK) < (ARC_COUNT_MASK >> 1)) {
        uint32_t new_count = old_count + 1;
        if (atomic_compare_exchange_weak_explicit(&obj->header.ref_count, &old_count, new_count,
                                                memory_order_acq_rel, memory_order_relaxed)) {
            atomic_fetch_add(&g_retain_fast_path, 1);
            return obj;
        }
    }
    
    // Fall back to full implementation
    return NULL; // Indicates fast path failed
}

// Fast path optimization: inline release for common case
static inline bool wyn_arc_release_fast(WynObject* obj) {
    if (!obj) return true; // NULL is always handled fast
    
    // Fast path: try simple atomic decrement first
    uint32_t old_count = atomic_load_explicit(&obj->header.ref_count, memory_order_relaxed);
    
    // Check if this is a simple case (no weak flag, count > 1)
    if (!(old_count & ARC_WEAK_FLAG) && (old_count & ARC_COUNT_MASK) > 1) {
        uint32_t new_count = old_count - 1;
        if (atomic_compare_exchange_weak_explicit(&obj->header.ref_count, &old_count, new_count,
                                                memory_order_acq_rel, memory_order_relaxed)) {
            atomic_fetch_add(&g_release_fast_path, 1);
            return true; // Successfully handled
        }
    }
    
    return false; // Fast path failed, need full implementation
}

// Enhanced retain with performance optimizations
WynObject* wyn_arc_retain_optimized(WynObject* obj) {
    atomic_fetch_add(&g_retain_operations, 1);
    
    if (!obj) return NULL;
    
    // Validate object magic number
    if (!wyn_arc_is_valid(obj)) {
        wyn_panic("Invalid object passed to wyn_arc_retain_optimized");
    }
    
    // Try fast path first
    WynObject* fast_result = wyn_arc_retain_fast(obj);
    if (fast_result) {
        return fast_result;
    }
    
    // Slow path: full atomic operation with all checks
    uint32_t old_count = atomic_load_explicit(&obj->header.ref_count, memory_order_acquire);
    
    do {
        uint32_t count_part = old_count & ARC_COUNT_MASK;
        
        // Enhanced overflow detection
        if (count_part >= (ARC_COUNT_MASK - 1000)) { // Leave safety margin
            atomic_fetch_add(&g_overflow_checks, 1);
            if (count_part == ARC_COUNT_MASK) {
                wyn_panic("Reference count overflow");
            }
            // Log warning for approaching overflow
            if (count_part > (ARC_COUNT_MASK - 100)) {
                fprintf(stderr, "WARNING: Reference count approaching overflow: %u\n", count_part);
            }
        }
        
        // Preserve weak flag, increment count
        uint32_t new_count = (old_count & ARC_WEAK_FLAG) | (count_part + 1);
        
        if (atomic_compare_exchange_weak_explicit(&obj->header.ref_count, &old_count, new_count,
                                                memory_order_acq_rel, memory_order_acquire)) {
            break;
        }
        
        // Exponential backoff for contention
        for (volatile int i = 0; i < 10; i++) {
            // Small delay to reduce contention
        }
    } while (true);
    
    return obj;
}

// Enhanced release with performance optimizations
void wyn_arc_release_optimized(WynObject* obj) {
    atomic_fetch_add(&g_release_operations, 1);
    
    if (!obj) return;
    
    if (!wyn_arc_is_valid(obj)) {
        wyn_panic("Invalid object passed to wyn_arc_release_optimized");
    }
    
    // Try fast path first
    if (wyn_arc_release_fast(obj)) {
        return;
    }
    
    // Slow path: full atomic operation with all checks
    uint32_t old_count = atomic_load_explicit(&obj->header.ref_count, memory_order_acquire);
    
    do {
        uint32_t count_part = old_count & ARC_COUNT_MASK;
        
        // Enhanced double-release detection
        if (count_part == 0) {
            atomic_fetch_add(&g_double_release_checks, 1);
            wyn_panic("Double release detected");
        }
        
        // Preserve weak flag, decrement count
        uint32_t new_count = (old_count & ARC_WEAK_FLAG) | (count_part - 1);
        
        if (atomic_compare_exchange_weak_explicit(&obj->header.ref_count, &old_count, new_count,
                                                memory_order_acq_rel, memory_order_acquire)) {
            // If count reached zero, deallocate
            if ((new_count & ARC_COUNT_MASK) == 0) {
                wyn_arc_deallocate(obj);
            }
            break;
        }
        
        // Exponential backoff for contention
        for (volatile int i = 0; i < 10; i++) {
            // Small delay to reduce contention
        }
    } while (true);
}

// Batch operations for performance
void wyn_arc_retain_batch(WynObject** objects, size_t count) {
    if (!objects || count == 0) return;
    
    for (size_t i = 0; i < count; i++) {
        if (objects[i]) {
            wyn_arc_retain_optimized(objects[i]);
        }
    }
}

void wyn_arc_release_batch(WynObject** objects, size_t count) {
    if (!objects || count == 0) return;
    
    for (size_t i = 0; i < count; i++) {
        if (objects[i]) {
            wyn_arc_release_optimized(objects[i]);
        }
    }
}

// Conditional retain/release for optimization
WynObject* wyn_arc_retain_if_not_null(WynObject* obj) {
    return obj ? wyn_arc_retain_optimized(obj) : NULL;
}

void wyn_arc_release_if_not_null(WynObject* obj) {
    if (obj) {
        wyn_arc_release_optimized(obj);
    }
}

// Move semantics support
WynObject* wyn_arc_move(WynObject** obj_ptr) {
    if (!obj_ptr || !*obj_ptr) return NULL;
    
    WynObject* obj = *obj_ptr;
    *obj_ptr = NULL; // Clear source pointer
    return obj; // Transfer ownership without retain/release
}

// Performance monitoring
WynARCPerformanceStats wyn_arc_get_performance_stats(void) {
    WynARCPerformanceStats stats;
    stats.total_retains = atomic_load(&g_retain_operations);
    stats.total_releases = atomic_load(&g_release_operations);
    stats.fast_path_retains = atomic_load(&g_retain_fast_path);
    stats.fast_path_releases = atomic_load(&g_release_fast_path);
    stats.overflow_checks = atomic_load(&g_overflow_checks);
    stats.double_release_checks = atomic_load(&g_double_release_checks);
    
    stats.fast_path_ratio_retain = stats.total_retains > 0 ? 
        (double)stats.fast_path_retains / stats.total_retains : 0.0;
    stats.fast_path_ratio_release = stats.total_releases > 0 ? 
        (double)stats.fast_path_releases / stats.total_releases : 0.0;
    
    return stats;
}

void wyn_arc_reset_performance_stats(void) {
    atomic_store(&g_retain_operations, 0);
    atomic_store(&g_release_operations, 0);
    atomic_store(&g_retain_fast_path, 0);
    atomic_store(&g_release_fast_path, 0);
    atomic_store(&g_overflow_checks, 0);
    atomic_store(&g_double_release_checks, 0);
}

void wyn_arc_print_performance_stats(void) {
    WynARCPerformanceStats stats = wyn_arc_get_performance_stats();
    printf("=== ARC Performance Statistics ===\n");
    printf("Total retains: %zu\n", stats.total_retains);
    printf("Total releases: %zu\n", stats.total_releases);
    printf("Fast path retains: %zu (%.1f%%)\n", stats.fast_path_retains, stats.fast_path_ratio_retain * 100);
    printf("Fast path releases: %zu (%.1f%%)\n", stats.fast_path_releases, stats.fast_path_ratio_release * 100);
    printf("Overflow checks: %zu\n", stats.overflow_checks);
    printf("Double release checks: %zu\n", stats.double_release_checks);
    printf("==================================\n");
}
