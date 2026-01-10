#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

// T2.4.1: Escape Analysis Implementation
// Identify stack-allocatable objects and eliminate unnecessary retain/release pairs

// Escape analysis result for an object (defined in arc_runtime.h)

// Object allocation site information
typedef struct AllocationSite {
    void* allocation_point;     // Code location where object is allocated
    size_t object_size;         // Size of the allocated object
    uint32_t type_id;          // Type identifier
    EscapeStatus escape_status; // Escape analysis result
    bool can_stack_allocate;   // Whether this can be stack allocated
    bool needs_retain_release; // Whether retain/release calls are needed
    int reference_count;       // Number of references to this object
    struct AllocationSite* next;
} AllocationSite;

// Escape analysis context
typedef struct {
    AllocationSite* allocation_sites;
    size_t total_sites;
    size_t stack_allocatable_sites;
    size_t eliminated_retain_release_pairs;
    bool analysis_enabled;
    pthread_mutex_t lock;
} EscapeAnalysisContext;

static EscapeAnalysisContext g_escape_context = {0};

// Initialize escape analysis
void wyn_escape_analysis_init(void) {
    if (g_escape_context.analysis_enabled) return;
    
    g_escape_context.allocation_sites = NULL;
    g_escape_context.total_sites = 0;
    g_escape_context.stack_allocatable_sites = 0;
    g_escape_context.eliminated_retain_release_pairs = 0;
    g_escape_context.analysis_enabled = true;
    
    pthread_mutex_init(&g_escape_context.lock, NULL);
}

// Register allocation site for analysis
AllocationSite* wyn_escape_register_allocation(void* allocation_point, size_t size, uint32_t type_id) {
    if (!g_escape_context.analysis_enabled) return NULL;
    
    AllocationSite* site = malloc(sizeof(AllocationSite));
    if (!site) return NULL;
    
    site->allocation_point = allocation_point;
    site->object_size = size;
    site->type_id = type_id;
    site->escape_status = ESCAPE_UNKNOWN;
    site->can_stack_allocate = false;
    site->needs_retain_release = true;
    site->reference_count = 0;
    site->next = NULL;
    
    pthread_mutex_lock(&g_escape_context.lock);
    
    // Add to linked list
    site->next = g_escape_context.allocation_sites;
    g_escape_context.allocation_sites = site;
    g_escape_context.total_sites++;
    
    pthread_mutex_unlock(&g_escape_context.lock);
    
    return site;
}

// Perform escape analysis on an allocation site
static EscapeStatus analyze_escape_status(AllocationSite* site) {
    if (!site) return ESCAPE_UNKNOWN;
    
    // Simple heuristics for escape analysis
    // In a real implementation, this would analyze the LLVM IR
    
    // Small objects with single reference are likely stack allocatable
    if (site->object_size <= 256 && site->reference_count <= 1) {
        return ESCAPE_NO_ESCAPE;
    }
    
    // Objects with multiple references may escape locally
    if (site->reference_count <= 3) {
        return ESCAPE_LOCAL_ESCAPE;
    }
    
    // Objects with many references likely escape globally
    return ESCAPE_GLOBAL_ESCAPE;
}

// Update reference count for allocation site
void wyn_escape_add_reference(AllocationSite* site) {
    if (!site) return;
    
    pthread_mutex_lock(&g_escape_context.lock);
    site->reference_count++;
    pthread_mutex_unlock(&g_escape_context.lock);
}

// Run escape analysis on all registered sites
void wyn_escape_analyze_all(void) {
    if (!g_escape_context.analysis_enabled) return;
    
    pthread_mutex_lock(&g_escape_context.lock);
    
    AllocationSite* current = g_escape_context.allocation_sites;
    while (current) {
        // Perform escape analysis
        current->escape_status = analyze_escape_status(current);
        
        // Determine optimization opportunities
        switch (current->escape_status) {
            case ESCAPE_NO_ESCAPE:
                current->can_stack_allocate = true;
                current->needs_retain_release = false;
                g_escape_context.stack_allocatable_sites++;
                g_escape_context.eliminated_retain_release_pairs++;
                break;
                
            case ESCAPE_LOCAL_ESCAPE:
                current->can_stack_allocate = false;
                current->needs_retain_release = (current->reference_count > 1);
                if (!current->needs_retain_release) {
                    g_escape_context.eliminated_retain_release_pairs++;
                }
                break;
                
            case ESCAPE_GLOBAL_ESCAPE:
            case ESCAPE_ARGUMENT_ESCAPE:
                current->can_stack_allocate = false;
                current->needs_retain_release = true;
                break;
                
            case ESCAPE_UNKNOWN:
            default:
                // Conservative: assume heap allocation and retain/release needed
                current->can_stack_allocate = false;
                current->needs_retain_release = true;
                break;
        }
        
        current = current->next;
    }
    
    pthread_mutex_unlock(&g_escape_context.lock);
}

// Check if an allocation site can be stack allocated
bool wyn_escape_can_stack_allocate(AllocationSite* site) {
    if (!site) return false;
    return site->can_stack_allocate;
}

// Check if retain/release is needed for an allocation site
bool wyn_escape_needs_retain_release(AllocationSite* site) {
    if (!site) return true; // Conservative default
    return site->needs_retain_release;
}

// Stack allocation for objects that don't escape
void* wyn_stack_alloc(size_t size) {
    // In a real implementation, this would use alloca() or similar
    // For now, we simulate with regular malloc but mark it as stack-like
    void* ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

// Stack deallocation (no-op for real stack allocation)
void wyn_stack_free(void* ptr) {
    // In a real implementation with alloca(), this would be a no-op
    // For our simulation, we still need to free
    if (ptr) {
        free(ptr);
    }
}

// Optimized ARC allocation with escape analysis
WynObject* wyn_arc_alloc_optimized(size_t size, uint32_t type_id, void (*destructor)(void*)) {
    // Register allocation site for analysis
    AllocationSite* site = wyn_escape_register_allocation(__builtin_return_address(0), size, type_id);
    
    // For now, use regular allocation
    // In a real implementation, this would check escape analysis results
    WynObject* obj = wyn_arc_alloc(size, type_id, destructor);
    
    if (site && obj) {
        wyn_escape_add_reference(site);
    }
    
    return obj;
}

// Optimized retain that may be eliminated
WynObject* wyn_arc_retain_escape_optimized(WynObject* obj, AllocationSite* site) {
    if (!obj) return NULL;
    
    // Check if retain/release can be eliminated
    if (site && !wyn_escape_needs_retain_release(site)) {
        // Retain/release eliminated by escape analysis
        return obj;
    }
    
    // Use regular retain
    return wyn_arc_retain(obj);
}

// Optimized release that may be eliminated
void wyn_arc_release_escape_optimized(WynObject* obj, AllocationSite* site) {
    if (!obj) return;
    
    // Check if retain/release can be eliminated
    if (site && !wyn_escape_needs_retain_release(site)) {
        // Retain/release eliminated by escape analysis
        return;
    }
    
    // Use regular release
    wyn_arc_release(obj);
}

// Escape analysis statistics (defined in arc_runtime.h)
WynEscapeStats wyn_escape_get_stats(void) {
    WynEscapeStats stats = {0};
    
    if (!g_escape_context.analysis_enabled) return stats;
    
    pthread_mutex_lock(&g_escape_context.lock);
    
    stats.total_allocation_sites = g_escape_context.total_sites;
    stats.stack_allocatable_sites = g_escape_context.stack_allocatable_sites;
    stats.eliminated_retain_release_pairs = g_escape_context.eliminated_retain_release_pairs;
    
    if (stats.total_allocation_sites > 0) {
        stats.stack_allocation_ratio = (double)stats.stack_allocatable_sites / stats.total_allocation_sites;
        stats.optimization_ratio = (double)stats.eliminated_retain_release_pairs / stats.total_allocation_sites;
    }
    
    pthread_mutex_unlock(&g_escape_context.lock);
    
    return stats;
}

void wyn_escape_print_stats(void) {
    WynEscapeStats stats = wyn_escape_get_stats();
    
    printf("=== Escape Analysis Statistics ===\n");
    printf("Total allocation sites: %zu\n", stats.total_allocation_sites);
    printf("Stack allocatable sites: %zu\n", stats.stack_allocatable_sites);
    printf("Eliminated retain/release pairs: %zu\n", stats.eliminated_retain_release_pairs);
    printf("Stack allocation ratio: %.2f%%\n", stats.stack_allocation_ratio * 100);
    printf("Optimization ratio: %.2f%%\n", stats.optimization_ratio * 100);
    printf("==================================\n");
}

// Reset escape analysis
void wyn_escape_reset(void) {
    if (!g_escape_context.analysis_enabled) return;
    
    pthread_mutex_lock(&g_escape_context.lock);
    
    // Free all allocation sites
    AllocationSite* current = g_escape_context.allocation_sites;
    while (current) {
        AllocationSite* next = current->next;
        free(current);
        current = next;
    }
    
    g_escape_context.allocation_sites = NULL;
    g_escape_context.total_sites = 0;
    g_escape_context.stack_allocatable_sites = 0;
    g_escape_context.eliminated_retain_release_pairs = 0;
    
    pthread_mutex_unlock(&g_escape_context.lock);
}

// Cleanup escape analysis
void wyn_escape_cleanup(void) {
    if (!g_escape_context.analysis_enabled) return;
    
    wyn_escape_reset();
    
    pthread_mutex_destroy(&g_escape_context.lock);
    g_escape_context.analysis_enabled = false;
}

// LLVM optimization pass integration (placeholder)
void wyn_escape_llvm_optimization_pass(void) {
    // In a real implementation, this would integrate with LLVM's optimization passes
    // For now, we just run our analysis
    wyn_escape_analyze_all();
}

// Enable/disable escape analysis
void wyn_escape_set_enabled(bool enabled) {
    if (enabled && !g_escape_context.analysis_enabled) {
        wyn_escape_analysis_init();
    } else if (!enabled && g_escape_context.analysis_enabled) {
        wyn_escape_cleanup();
    }
}
