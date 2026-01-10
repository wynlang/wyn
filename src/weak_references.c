#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// T2.3.3: Weak Reference System
// Comprehensive weak reference implementation with automatic nullification

// Weak reference structure
typedef struct WynWeakRef {
    WynObject* target;           // Target object (can become NULL)
    struct WynWeakRef* next;     // Next weak reference in chain
    struct WynWeakRef* prev;     // Previous weak reference in chain
    _Atomic bool is_valid;       // Atomic flag for validity
    pthread_mutex_t lock;        // Per-reference lock for thread safety
} WynWeakRef;

// Global weak reference registry
typedef struct {
    WynWeakRef** buckets;        // Hash table of weak reference chains
    size_t bucket_count;         // Number of hash buckets
    pthread_rwlock_t global_lock; // Global read-write lock
    _Atomic size_t total_weak_refs; // Total weak references count
    _Atomic size_t nullified_refs;  // Count of nullified references
} WeakRefRegistry;

static WeakRefRegistry g_weak_registry = {0};
static bool g_registry_initialized = false;

// Hash function for object pointers
static size_t hash_object_ptr(WynObject* obj) {
    uintptr_t addr = (uintptr_t)obj;
    return (addr >> 3) ^ (addr >> 16); // Simple hash mixing
}

// Initialize weak reference registry
static void init_weak_registry(void) {
    if (g_registry_initialized) return;
    
    g_weak_registry.bucket_count = 1024; // Power of 2 for fast modulo
    g_weak_registry.buckets = calloc(g_weak_registry.bucket_count, sizeof(WynWeakRef*));
    
    if (!g_weak_registry.buckets) {
        wyn_panic("Failed to initialize weak reference registry");
    }
    
    pthread_rwlock_init(&g_weak_registry.global_lock, NULL);
    atomic_store(&g_weak_registry.total_weak_refs, 0);
    atomic_store(&g_weak_registry.nullified_refs, 0);
    
    g_registry_initialized = true;
}

// Create weak reference to object
WynWeakRef* wyn_weak_create(WynObject* obj) {
    if (!obj) return NULL;
    
    if (!wyn_arc_is_valid(obj)) {
        report_error(ERR_INVALID_EXPRESSION, __FILE__, __LINE__, 0, "Cannot create weak reference to invalid object");
        return NULL;
    }
    
    init_weak_registry();
    
    // Allocate weak reference
    WynWeakRef* weak_ref = malloc(sizeof(WynWeakRef));
    if (!weak_ref) {
        report_error(ERR_OUT_OF_MEMORY, __FILE__, __LINE__, 0, "Failed to allocate weak reference");
        return NULL;
    }
    
    // Initialize weak reference
    weak_ref->target = obj;
    weak_ref->next = NULL;
    weak_ref->prev = NULL;
    atomic_store(&weak_ref->is_valid, true);
    pthread_mutex_init(&weak_ref->lock, NULL);
    
    // Add to registry
    size_t bucket = hash_object_ptr(obj) % g_weak_registry.bucket_count;
    
    pthread_rwlock_wrlock(&g_weak_registry.global_lock);
    
    // Insert at head of chain
    if (g_weak_registry.buckets[bucket]) {
        g_weak_registry.buckets[bucket]->prev = weak_ref;
    }
    weak_ref->next = g_weak_registry.buckets[bucket];
    g_weak_registry.buckets[bucket] = weak_ref;
    
    pthread_rwlock_unlock(&g_weak_registry.global_lock);
    
    atomic_fetch_add(&g_weak_registry.total_weak_refs, 1);
    
    return weak_ref;
}

// Access weak reference (returns NULL if target was deallocated)
WynObject* wyn_weak_access(WynWeakRef* weak_ref) {
    if (!weak_ref) return NULL;
    
    pthread_mutex_lock(&weak_ref->lock);
    
    bool is_valid = atomic_load(&weak_ref->is_valid);
    WynObject* target = is_valid ? weak_ref->target : NULL;
    
    // If target exists, validate it's still alive
    if (target && !wyn_arc_is_valid(target)) {
        // Target was deallocated but not nullified yet
        atomic_store(&weak_ref->is_valid, false);
        weak_ref->target = NULL;
        target = NULL;
    }
    
    pthread_mutex_unlock(&weak_ref->lock);
    
    return target;
}

// Try to promote weak reference to strong reference
WynObject* wyn_weak_promote(WynWeakRef* weak_ref) {
    if (!weak_ref) return NULL;
    
    pthread_mutex_lock(&weak_ref->lock);
    
    WynObject* target = NULL;
    bool is_valid = atomic_load(&weak_ref->is_valid);
    
    if (is_valid && weak_ref->target) {
        // Try to retain the target
        if (wyn_arc_is_valid(weak_ref->target)) {
            target = wyn_arc_retain_optimized(weak_ref->target);
        } else {
            // Target was deallocated
            atomic_store(&weak_ref->is_valid, false);
            weak_ref->target = NULL;
        }
    }
    
    pthread_mutex_unlock(&weak_ref->lock);
    
    return target;
}

// Check if weak reference is still valid
bool wyn_weak_is_valid(WynWeakRef* weak_ref) {
    if (!weak_ref) return false;
    
    pthread_mutex_lock(&weak_ref->lock);
    bool is_valid = atomic_load(&weak_ref->is_valid);
    pthread_mutex_unlock(&weak_ref->lock);
    
    return is_valid;
}

// Destroy weak reference
void wyn_weak_destroy(WynWeakRef* weak_ref) {
    if (!weak_ref) return;
    
    init_weak_registry();
    
    // Lock the weak reference first
    pthread_mutex_lock(&weak_ref->lock);
    WynObject* target = weak_ref->target;
    atomic_store(&weak_ref->is_valid, false);
    weak_ref->target = NULL;
    pthread_mutex_unlock(&weak_ref->lock);
    
    // Remove from registry if it was in there
    if (target) {
        size_t bucket = hash_object_ptr(target) % g_weak_registry.bucket_count;
        
        pthread_rwlock_wrlock(&g_weak_registry.global_lock);
        
        // Remove from chain
        if (weak_ref->prev) {
            weak_ref->prev->next = weak_ref->next;
        } else {
            g_weak_registry.buckets[bucket] = weak_ref->next;
        }
        
        if (weak_ref->next) {
            weak_ref->next->prev = weak_ref->prev;
        }
        
        pthread_rwlock_unlock(&g_weak_registry.global_lock);
    }
    
    // Cleanup
    pthread_mutex_destroy(&weak_ref->lock);
    free(weak_ref);
    
    atomic_fetch_sub(&g_weak_registry.total_weak_refs, 1);
}

// Nullify all weak references to an object (called during deallocation)
void wyn_weak_nullify_all(WynObject* obj) {
    if (!obj || !g_registry_initialized) return;
    
    size_t bucket = hash_object_ptr(obj) % g_weak_registry.bucket_count;
    
    pthread_rwlock_wrlock(&g_weak_registry.global_lock);
    
    WynWeakRef* current = g_weak_registry.buckets[bucket];
    size_t nullified_count = 0;
    
    while (current) {
        WynWeakRef* next = current->next;
        
        if (current->target == obj) {
            pthread_mutex_lock(&current->lock);
            
            // Nullify the reference
            atomic_store(&current->is_valid, false);
            current->target = NULL;
            nullified_count++;
            
            pthread_mutex_unlock(&current->lock);
        }
        
        current = next;
    }
    
    pthread_rwlock_unlock(&g_weak_registry.global_lock);
    
    atomic_fetch_add(&g_weak_registry.nullified_refs, nullified_count);
}

// Batch operations for weak references
void wyn_weak_create_batch(WynObject** objects, WynWeakRef** weak_refs, size_t count) {
    if (!objects || !weak_refs || count == 0) return;
    
    for (size_t i = 0; i < count; i++) {
        weak_refs[i] = wyn_weak_create(objects[i]);
    }
}

void wyn_weak_destroy_batch(WynWeakRef** weak_refs, size_t count) {
    if (!weak_refs || count == 0) return;
    
    for (size_t i = 0; i < count; i++) {
        if (weak_refs[i]) {
            wyn_weak_destroy(weak_refs[i]);
            weak_refs[i] = NULL;
        }
    }
}

// Weak reference statistics
WynWeakRefStats wyn_weak_get_stats(void) {
    WynWeakRefStats stats;
    stats.total_weak_refs = atomic_load(&g_weak_registry.total_weak_refs);
    stats.nullified_refs = atomic_load(&g_weak_registry.nullified_refs);
    stats.active_refs = stats.total_weak_refs; // Active = total - destroyed
    stats.nullification_rate = stats.total_weak_refs > 0 ? 
        (double)stats.nullified_refs / stats.total_weak_refs : 0.0;
    
    return stats;
}

void wyn_weak_reset_stats(void) {
    atomic_store(&g_weak_registry.nullified_refs, 0);
}

void wyn_weak_print_stats(void) {
    WynWeakRefStats stats = wyn_weak_get_stats();
    printf("=== Weak Reference Statistics ===\n");
    printf("Total weak references: %zu\n", stats.total_weak_refs);
    printf("Nullified references: %zu\n", stats.nullified_refs);
    printf("Active references: %zu\n", stats.active_refs);
    printf("Nullification rate: %.1f%%\n", stats.nullification_rate * 100);
    printf("=================================\n");
}

// Cleanup weak reference registry (for shutdown)
void wyn_weak_cleanup_registry(void) {
    if (!g_registry_initialized) return;
    
    pthread_rwlock_wrlock(&g_weak_registry.global_lock);
    
    // Clean up all remaining weak references
    for (size_t i = 0; i < g_weak_registry.bucket_count; i++) {
        WynWeakRef* current = g_weak_registry.buckets[i];
        while (current) {
            WynWeakRef* next = current->next;
            
            // Lock the weak reference before cleanup
            pthread_mutex_lock(&current->lock);
            atomic_store(&current->is_valid, false);
            current->target = NULL;
            pthread_mutex_unlock(&current->lock);
            
            pthread_mutex_destroy(&current->lock);
            free(current);
            current = next;
        }
        g_weak_registry.buckets[i] = NULL;
    }
    
    free(g_weak_registry.buckets);
    g_weak_registry.buckets = NULL;
    
    pthread_rwlock_unlock(&g_weak_registry.global_lock);
    pthread_rwlock_destroy(&g_weak_registry.global_lock);
    
    g_registry_initialized = false;
}
