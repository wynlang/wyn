#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include "safe_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for integration
void wyn_weak_nullify_all(WynObject* obj);
void wyn_cycle_add_candidate(WynObject* obj);
void wyn_cycle_check_collection_trigger(void);

// T2.3.1: Object Header Design Implementation
// Efficient object header layout with reference counting operations

// Global statistics (thread-safe)
static _Atomic size_t g_total_allocations = 0;
static _Atomic size_t g_total_deallocations = 0;
static _Atomic size_t g_current_objects = 0;
static _Atomic size_t g_peak_objects = 0;
static _Atomic size_t g_total_memory = 0;
static _Atomic size_t g_peak_memory = 0;

// Panic function for critical errors
void wyn_panic(const char* message) {
    fprintf(stderr, "PANIC: %s\n", message);
    abort();
}

// Allocate new ARC-managed object
WynObject* wyn_arc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*)) {
    if (size == 0) {
        report_error(ERR_INVALID_EXPRESSION, __FILE__, __LINE__, 0, "Cannot allocate zero-sized object");
        return NULL;
    }
    
    // Calculate total size including header
    size_t total_size = sizeof(WynObjectHeader) + size;
    
    // Allocate memory using safe allocator
    WynObject* obj = (WynObject*)safe_malloc(total_size);
    if (!obj) {
        report_error(ERR_OUT_OF_MEMORY, __FILE__, __LINE__, 0, "Failed to allocate ARC object");
        return NULL;
    }
    
    // Initialize header
    obj->header.magic = ARC_MAGIC;
    atomic_store(&obj->header.ref_count, 1);  // Start with reference count of 1
    obj->header.type_id = type_id;
    obj->header.size = (uint32_t)size;
    obj->header.destructor = destructor;
    
    // Zero-initialize data area
    memset(obj->data, 0, size);
    
    // Add to cycle detection candidates (disabled for now to avoid test issues)
    // wyn_cycle_add_candidate(obj);
    
    // Check if cycle collection should be triggered (disabled for now)
    // wyn_cycle_check_collection_trigger();
    
    // Update statistics
    atomic_fetch_add(&g_total_allocations, 1);
    size_t current = atomic_fetch_add(&g_current_objects, 1) + 1;
    size_t current_memory = atomic_fetch_add(&g_total_memory, total_size) + total_size;
    
    // Update peak counters
    size_t peak = atomic_load(&g_peak_objects);
    while (current > peak && !atomic_compare_exchange_weak(&g_peak_objects, &peak, current)) {
        // Retry if another thread updated peak
    }
    
    size_t peak_mem = atomic_load(&g_peak_memory);
    while (current_memory > peak_mem && !atomic_compare_exchange_weak(&g_peak_memory, &peak_mem, current_memory)) {
        // Retry if another thread updated peak memory
    }
    
    return obj;
}

// Validate object header
bool wyn_arc_is_valid(WynObject* obj) {
    if (!obj) return false;
    return obj->header.magic == ARC_MAGIC;
}

// Retain object (increment reference count)
WynObject* wyn_arc_retain(WynObject* obj) {
    if (!obj) return NULL;
    
    // Validate object magic number
    if (!wyn_arc_is_valid(obj)) {
        wyn_panic("Invalid object passed to wyn_arc_retain");
    }
    
    // Atomic increment with overflow check
    uint32_t old_count = atomic_load(&obj->header.ref_count);
    
    do {
        // Check for overflow (excluding weak flag)
        if ((old_count & ARC_COUNT_MASK) == ARC_COUNT_MASK) {
            wyn_panic("Reference count overflow");
        }
        
        // Preserve weak flag, increment count
        uint32_t new_count = (old_count & ARC_WEAK_FLAG) | 
                           ((old_count & ARC_COUNT_MASK) + 1);
        
        if (atomic_compare_exchange_weak(&obj->header.ref_count, &old_count, new_count)) {
            break;
        }
    } while (true);
    
    return obj;
}

// Release object (decrement reference count, deallocate if zero)
void wyn_arc_release(WynObject* obj) {
    if (!obj) return;
    
    if (!wyn_arc_is_valid(obj)) {
        wyn_panic("Invalid object passed to wyn_arc_release");
    }
    
    uint32_t old_count = atomic_load(&obj->header.ref_count);
    
    do {
        uint32_t count_part = old_count & ARC_COUNT_MASK;
        if (count_part == 0) {
            wyn_panic("Double release detected");
        }
        
        // Preserve weak flag, decrement count
        uint32_t new_count = (old_count & ARC_WEAK_FLAG) | (count_part - 1);
        
        if (atomic_compare_exchange_weak(&obj->header.ref_count, &old_count, new_count)) {
            // If count reached zero, deallocate
            if ((new_count & ARC_COUNT_MASK) == 0) {
                wyn_arc_deallocate(obj);
            }
            break;
        }
    } while (true);
}

// Weak retain (for weak references)
WynObject* wyn_arc_weak_retain(WynObject* obj) {
    if (!obj) return NULL;
    
    if (!wyn_arc_is_valid(obj)) {
        wyn_panic("Invalid object passed to wyn_arc_weak_retain");
    }
    
    // Set weak flag
    uint32_t old_count = atomic_load(&obj->header.ref_count);
    uint32_t new_count = old_count | ARC_WEAK_FLAG;
    
    while (!atomic_compare_exchange_weak(&obj->header.ref_count, &old_count, new_count)) {
        new_count = old_count | ARC_WEAK_FLAG;
    }
    
    return obj;
}

// Weak release (clear weak flag)
void wyn_arc_weak_release(WynObject* obj) {
    if (!obj) return;
    
    if (!wyn_arc_is_valid(obj)) {
        wyn_panic("Invalid object passed to wyn_arc_weak_release");
    }
    
    // Clear weak flag
    uint32_t old_count = atomic_load(&obj->header.ref_count);
    uint32_t new_count = old_count & ~ARC_WEAK_FLAG;
    
    while (!atomic_compare_exchange_weak(&obj->header.ref_count, &old_count, new_count)) {
        new_count = old_count & ~ARC_WEAK_FLAG;
    }
}

// Deallocate object (internal function)
void wyn_arc_deallocate(WynObject* obj) {
    if (!obj) return;
    
    // Nullify all weak references to this object before deallocation
    // wyn_weak_nullify_all(obj); // Disabled for now - not implemented
    
    // Call custom destructor if provided
    if (obj->header.destructor) {
        obj->header.destructor(obj->data);
    }
    
    // Update statistics
    atomic_fetch_add(&g_total_deallocations, 1);
    atomic_fetch_sub(&g_current_objects, 1);
    atomic_fetch_sub(&g_total_memory, sizeof(WynObjectHeader) + obj->header.size);
    
    // Clear magic number for safety
    obj->header.magic = 0;
    
    // Free memory
    safe_free(obj);
}

// Utility functions
uint32_t wyn_arc_get_ref_count(WynObject* obj) {
    if (!wyn_arc_is_valid(obj)) return 0;
    return atomic_load(&obj->header.ref_count) & ARC_COUNT_MASK;
}

uint32_t wyn_arc_get_type_id(WynObject* obj) {
    if (!wyn_arc_is_valid(obj)) return WYN_TYPE_UNKNOWN;
    return obj->header.type_id;
}

size_t wyn_arc_get_size(WynObject* obj) {
    if (!wyn_arc_is_valid(obj)) return 0;
    return obj->header.size;
}

void* wyn_arc_get_data(WynObject* obj) {
    if (!wyn_arc_is_valid(obj)) return NULL;
    return obj->data;
}

// Statistics functions
WynARCStats wyn_arc_get_stats(void) {
    WynARCStats stats;
    stats.total_allocations = atomic_load(&g_total_allocations);
    stats.total_deallocations = atomic_load(&g_total_deallocations);
    stats.current_objects = atomic_load(&g_current_objects);
    stats.peak_objects = atomic_load(&g_peak_objects);
    stats.total_memory = atomic_load(&g_total_memory);
    stats.peak_memory = atomic_load(&g_peak_memory);
    return stats;
}

void wyn_arc_reset_stats(void) {
    atomic_store(&g_total_allocations, 0);
    atomic_store(&g_total_deallocations, 0);
    atomic_store(&g_current_objects, 0);
    atomic_store(&g_peak_objects, 0);
    atomic_store(&g_total_memory, 0);
    atomic_store(&g_peak_memory, 0);
}

void wyn_arc_print_stats(void) {
    WynARCStats stats = wyn_arc_get_stats();
    printf("=== ARC Runtime Statistics ===\n");
    printf("Total allocations: %zu\n", stats.total_allocations);
    printf("Total deallocations: %zu\n", stats.total_deallocations);
    printf("Current objects: %zu\n", stats.current_objects);
    printf("Peak objects: %zu\n", stats.peak_objects);
    printf("Total memory: %zu bytes\n", stats.total_memory);
    printf("Peak memory: %zu bytes\n", stats.peak_memory);
    printf("==============================\n");
}