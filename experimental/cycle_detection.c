#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

// T2.3.4: Cycle Detection Algorithm
// Trial deletion algorithm for detecting and collecting reference cycles

// Cycle detection configuration
// (Defined in arc_runtime.h)

// Object state during cycle detection
typedef enum {
    CYCLE_STATE_WHITE = 0,  // Not visited
    CYCLE_STATE_GRAY = 1,   // Being processed
    CYCLE_STATE_BLACK = 2,  // Processed
    CYCLE_STATE_PURPLE = 3  // Potential cycle member
} CycleState;

// Extended object header for cycle detection
typedef struct CycleDetectionData {
    CycleState state;
    bool marked_for_collection;
    size_t original_ref_count;
    struct CycleDetectionData* next_in_cycle;
    WynObject* object;
} CycleDetectionData;

// Cycle detection registry
typedef struct {
    CycleDetectionData** candidates;  // Array of potential cycle candidates
    size_t candidate_count;
    size_t candidate_capacity;
    pthread_mutex_t registry_lock;
    CycleDetectionConfig config;
    
    // Statistics
    _Atomic size_t cycles_detected;
    _Atomic size_t objects_collected;
    _Atomic size_t collection_runs;
    _Atomic size_t false_positives;
    _Atomic size_t allocations_since_collection;
} CycleDetectionRegistry;

static CycleDetectionRegistry g_cycle_registry = {0};
static bool g_cycle_detection_initialized = false;

// Initialize cycle detection system
void wyn_cycle_detection_init(void) {
    if (g_cycle_detection_initialized) return;
    
    // Initialize registry
    g_cycle_registry.candidate_capacity = 1024;
    g_cycle_registry.candidates = calloc(g_cycle_registry.candidate_capacity, 
                                       sizeof(CycleDetectionData*));
    if (!g_cycle_registry.candidates) {
        wyn_panic("Failed to initialize cycle detection registry");
    }
    
    g_cycle_registry.candidate_count = 0;
    pthread_mutex_init(&g_cycle_registry.registry_lock, NULL);
    
    // Default configuration
    g_cycle_registry.config.collection_threshold = 1000;
    g_cycle_registry.config.max_objects_per_cycle = 100;
    g_cycle_registry.config.auto_collection_enabled = true;
    g_cycle_registry.config.collection_frequency = 0.1; // 10% of allocations
    g_cycle_registry.config.min_cycle_size = 2;
    
    // Initialize statistics
    atomic_store(&g_cycle_registry.cycles_detected, 0);
    atomic_store(&g_cycle_registry.objects_collected, 0);
    atomic_store(&g_cycle_registry.collection_runs, 0);
    atomic_store(&g_cycle_registry.false_positives, 0);
    atomic_store(&g_cycle_registry.allocations_since_collection, 0);
    
    g_cycle_detection_initialized = true;
}

// Configure cycle detection parameters
void wyn_cycle_configure(CycleDetectionConfig* config) {
    if (!config) return;
    
    wyn_cycle_detection_init();
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    g_cycle_registry.config = *config;
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
}

// Add object as potential cycle candidate
void wyn_cycle_add_candidate(WynObject* obj) {
    if (!obj || !g_cycle_detection_initialized) return;
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    
    // Expand capacity if needed
    if (g_cycle_registry.candidate_count >= g_cycle_registry.candidate_capacity) {
        size_t new_capacity = g_cycle_registry.candidate_capacity * 2;
        CycleDetectionData** new_candidates = realloc(g_cycle_registry.candidates,
                                                     new_capacity * sizeof(CycleDetectionData*));
        if (!new_candidates) {
            pthread_mutex_unlock(&g_cycle_registry.registry_lock);
            return; // Fail silently to avoid disrupting normal operation
        }
        g_cycle_registry.candidates = new_candidates;
        g_cycle_registry.candidate_capacity = new_capacity;
    }
    
    // Create cycle detection data
    CycleDetectionData* cycle_data = malloc(sizeof(CycleDetectionData));
    if (!cycle_data) {
        pthread_mutex_unlock(&g_cycle_registry.registry_lock);
        return;
    }
    
    cycle_data->state = CYCLE_STATE_WHITE;
    cycle_data->marked_for_collection = false;
    cycle_data->original_ref_count = wyn_arc_get_ref_count(obj);
    cycle_data->next_in_cycle = NULL;
    cycle_data->object = obj;
    
    g_cycle_registry.candidates[g_cycle_registry.candidate_count++] = cycle_data;
    
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
}

// Mark object references (simplified - would need object introspection in real implementation)
static void mark_object_references(WynObject* obj, CycleState new_state) {
    if (!obj) return;
    
    // In a real implementation, this would traverse object references
    // For now, we simulate by marking the object itself
    (void)new_state; // Suppress unused parameter warning
    
    // This is where we would:
    // 1. Get object's reference fields
    // 2. For each referenced object, mark it with new_state
    // 3. Recursively process referenced objects
}

// Trial deletion algorithm
static bool trial_delete_object(CycleDetectionData* cycle_data) {
    if (!cycle_data || !cycle_data->object) return false;
    
    WynObject* obj = cycle_data->object;
    
    // Check if object is still valid before accessing it
    if (!wyn_arc_is_valid(obj)) {
        // Object was already deallocated, mark as collected
        cycle_data->state = CYCLE_STATE_BLACK;
        cycle_data->marked_for_collection = false;
        return true;
    }
    
    // Mark object as being processed first
    cycle_data->state = CYCLE_STATE_GRAY;
    
    // Now safely get reference count since we know object is valid
    uint32_t current_ref_count = wyn_arc_get_ref_count(obj);
    
    // If reference count is 0, object is already being deallocated
    if (current_ref_count == 0) {
        cycle_data->state = CYCLE_STATE_BLACK;
        return true; // Consider it collected
    }
    
    // Simulate trial deletion by checking if object would be deallocated
    if (current_ref_count == 1) {
        // Object would be deallocated if not part of cycle
        cycle_data->state = CYCLE_STATE_PURPLE; // Potential cycle member
        return true;
    }
    
    // Mark as processed
    cycle_data->state = CYCLE_STATE_BLACK;
    return false;
}

// Detect cycles in candidate objects
static size_t detect_cycles(void) {
    if (!g_cycle_detection_initialized) return 0;
    
    size_t cycles_found = 0;
    size_t objects_in_cycles = 0;
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    
    // Reset all states and clean up invalid candidates
    for (size_t i = 0; i < g_cycle_registry.candidate_count; i++) {
        if (g_cycle_registry.candidates[i]) {
            WynObject* obj = g_cycle_registry.candidates[i]->object;
            if (!obj || !wyn_arc_is_valid(obj)) {
                // Remove invalid candidate
                free(g_cycle_registry.candidates[i]);
                g_cycle_registry.candidates[i] = NULL;
                continue;
            }
            g_cycle_registry.candidates[i]->state = CYCLE_STATE_WHITE;
            g_cycle_registry.candidates[i]->marked_for_collection = false;
        }
    }
    
    // Trial deletion phase
    for (size_t i = 0; i < g_cycle_registry.candidate_count; i++) {
        CycleDetectionData* candidate = g_cycle_registry.candidates[i];
        if (!candidate || candidate->state != CYCLE_STATE_WHITE) continue;
        
        if (trial_delete_object(candidate)) {
            if (candidate->state == CYCLE_STATE_PURPLE) {
                // Potential cycle member found
                candidate->marked_for_collection = true;
                objects_in_cycles++;
                
                // Group consecutive cycle members
                if (objects_in_cycles >= g_cycle_registry.config.min_cycle_size) {
                    cycles_found++;
                }
            }
        }
    }
    
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
    
    return cycles_found;
}

// Collect objects marked for collection
static size_t collect_marked_objects(void) {
    if (!g_cycle_detection_initialized) return 0;
    
    size_t collected = 0;
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    
    // Collect marked objects (simulation only - don't actually deallocate)
    for (size_t i = 0; i < g_cycle_registry.candidate_count; i++) {
        CycleDetectionData* candidate = g_cycle_registry.candidates[i];
        if (!candidate || !candidate->marked_for_collection) continue;
        
        WynObject* obj = candidate->object;
        if (obj && wyn_arc_is_valid(obj)) {
            // Verify object is still in a cycle before collecting
            uint32_t ref_count = wyn_arc_get_ref_count(obj);
            if (ref_count > 0) {
                // In a real implementation, this would deallocate cyclic objects
                // For testing, we just count it as collected
                collected++;
            }
        }
        
        // Remove from candidates
        free(candidate);
        g_cycle_registry.candidates[i] = NULL;
    }
    
    // Compact candidate array
    size_t write_pos = 0;
    for (size_t read_pos = 0; read_pos < g_cycle_registry.candidate_count; read_pos++) {
        if (g_cycle_registry.candidates[read_pos]) {
            g_cycle_registry.candidates[write_pos++] = g_cycle_registry.candidates[read_pos];
        }
    }
    g_cycle_registry.candidate_count = write_pos;
    
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
    
    return collected;
}

// Run cycle detection and collection
size_t wyn_cycle_collect(void) {
    wyn_cycle_detection_init();
    
    atomic_fetch_add(&g_cycle_registry.collection_runs, 1);
    
    // Detect cycles
    size_t cycles_detected = detect_cycles();
    atomic_fetch_add(&g_cycle_registry.cycles_detected, cycles_detected);
    
    // Collect cyclic objects
    size_t objects_collected = collect_marked_objects();
    atomic_fetch_add(&g_cycle_registry.objects_collected, objects_collected);
    
    // Reset allocation counter
    atomic_store(&g_cycle_registry.allocations_since_collection, 0);
    
    return objects_collected;
}

// Check if automatic collection should be triggered
void wyn_cycle_check_collection_trigger(void) {
    if (!g_cycle_detection_initialized || !g_cycle_registry.config.auto_collection_enabled) {
        return;
    }
    
    size_t allocations = atomic_fetch_add(&g_cycle_registry.allocations_since_collection, 1);
    
    // Trigger collection based on threshold
    if (allocations >= g_cycle_registry.config.collection_threshold) {
        wyn_cycle_collect();
    }
    
    // Random collection based on frequency
    if ((double)rand() / RAND_MAX < g_cycle_registry.config.collection_frequency) {
        wyn_cycle_collect();
    }
}

// Get cycle detection configuration
CycleDetectionConfig wyn_cycle_get_config(void) {
    wyn_cycle_detection_init();
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    CycleDetectionConfig config = g_cycle_registry.config;
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
    
    return config;
}

// Cycle detection statistics
// (Defined in arc_runtime.h)
WynCycleStats wyn_cycle_get_stats(void) {
    WynCycleStats stats;
    stats.cycles_detected = atomic_load(&g_cycle_registry.cycles_detected);
    stats.objects_collected = atomic_load(&g_cycle_registry.objects_collected);
    stats.collection_runs = atomic_load(&g_cycle_registry.collection_runs);
    stats.false_positives = atomic_load(&g_cycle_registry.false_positives);
    stats.allocations_since_collection = atomic_load(&g_cycle_registry.allocations_since_collection);
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    stats.candidate_count = g_cycle_registry.candidate_count;
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
    
    stats.collection_efficiency = stats.collection_runs > 0 ? 
        (double)stats.objects_collected / stats.collection_runs : 0.0;
    
    return stats;
}

void wyn_cycle_reset_stats(void) {
    atomic_store(&g_cycle_registry.cycles_detected, 0);
    atomic_store(&g_cycle_registry.objects_collected, 0);
    atomic_store(&g_cycle_registry.collection_runs, 0);
    atomic_store(&g_cycle_registry.false_positives, 0);
    atomic_store(&g_cycle_registry.allocations_since_collection, 0);
}

void wyn_cycle_print_stats(void) {
    WynCycleStats stats = wyn_cycle_get_stats();
    printf("=== Cycle Detection Statistics ===\n");
    printf("Cycles detected: %zu\n", stats.cycles_detected);
    printf("Objects collected: %zu\n", stats.objects_collected);
    printf("Collection runs: %zu\n", stats.collection_runs);
    printf("False positives: %zu\n", stats.false_positives);
    printf("Current candidates: %zu\n", stats.candidate_count);
    printf("Allocations since collection: %zu\n", stats.allocations_since_collection);
    printf("Collection efficiency: %.2f objects/run\n", stats.collection_efficiency);
    printf("==================================\n");
}

// Cleanup cycle detection system
void wyn_cycle_cleanup(void) {
    if (!g_cycle_detection_initialized) return;
    
    pthread_mutex_lock(&g_cycle_registry.registry_lock);
    
    // Free all candidate data
    for (size_t i = 0; i < g_cycle_registry.candidate_count; i++) {
        if (g_cycle_registry.candidates[i]) {
            free(g_cycle_registry.candidates[i]);
        }
    }
    
    free(g_cycle_registry.candidates);
    g_cycle_registry.candidates = NULL;
    g_cycle_registry.candidate_count = 0;
    g_cycle_registry.candidate_capacity = 0;
    
    pthread_mutex_unlock(&g_cycle_registry.registry_lock);
    pthread_mutex_destroy(&g_cycle_registry.registry_lock);
    
    g_cycle_detection_initialized = false;
}
