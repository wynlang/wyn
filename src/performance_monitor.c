#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

// T2.3.6: Runtime Performance Monitoring
// Comprehensive ARC operation counters, memory tracking, and regression detection

// Performance monitoring configuration
typedef struct {
    bool enabled;
    bool detailed_logging;
    double regression_threshold;
    size_t sample_interval;
    size_t max_samples;
} PerformanceConfig;

// Performance sample
typedef struct {
    struct timeval timestamp;
    size_t allocations_per_sec;
    size_t deallocations_per_sec;
    size_t retains_per_sec;
    size_t releases_per_sec;
    size_t memory_usage;
    double avg_alloc_time_us;
    double avg_dealloc_time_us;
} PerformanceSample;

// Performance monitoring state
typedef struct {
    PerformanceConfig config;
    PerformanceSample* samples;
    size_t sample_count;
    size_t current_sample;
    
    // Counters
    _Atomic size_t total_arc_allocs;
    _Atomic size_t total_arc_deallocs;
    _Atomic size_t total_retains;
    _Atomic size_t total_releases;
    _Atomic size_t total_weak_creates;
    _Atomic size_t total_weak_destroys;
    _Atomic size_t total_cycle_collections;
    _Atomic size_t total_pool_allocs;
    _Atomic size_t total_pool_frees;
    
    // Timing
    _Atomic uint64_t total_alloc_time_us;
    _Atomic uint64_t total_dealloc_time_us;
    
    // Regression detection
    double baseline_alloc_rate;
    double baseline_dealloc_rate;
    bool regression_detected;
    char regression_message[256];
    
    pthread_mutex_t lock;
    bool initialized;
} PerformanceMonitor;

static PerformanceMonitor g_perf_monitor = {0};

// Get current time in microseconds
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// Initialize performance monitoring
void wyn_perf_init(void) {
    if (g_perf_monitor.initialized) return;
    
    // Default configuration
    g_perf_monitor.config.enabled = true;
    g_perf_monitor.config.detailed_logging = false;
    g_perf_monitor.config.regression_threshold = 0.2; // 20% degradation
    g_perf_monitor.config.sample_interval = 1000; // Every 1000 operations
    g_perf_monitor.config.max_samples = 100;
    
    // Allocate sample buffer
    g_perf_monitor.samples = calloc(g_perf_monitor.config.max_samples, sizeof(PerformanceSample));
    if (!g_perf_monitor.samples) {
        g_perf_monitor.config.enabled = false;
        return;
    }
    
    // Initialize counters
    atomic_store(&g_perf_monitor.total_arc_allocs, 0);
    atomic_store(&g_perf_monitor.total_arc_deallocs, 0);
    atomic_store(&g_perf_monitor.total_retains, 0);
    atomic_store(&g_perf_monitor.total_releases, 0);
    atomic_store(&g_perf_monitor.total_weak_creates, 0);
    atomic_store(&g_perf_monitor.total_weak_destroys, 0);
    atomic_store(&g_perf_monitor.total_cycle_collections, 0);
    atomic_store(&g_perf_monitor.total_pool_allocs, 0);
    atomic_store(&g_perf_monitor.total_pool_frees, 0);
    atomic_store(&g_perf_monitor.total_alloc_time_us, 0);
    atomic_store(&g_perf_monitor.total_dealloc_time_us, 0);
    
    g_perf_monitor.sample_count = 0;
    g_perf_monitor.current_sample = 0;
    g_perf_monitor.baseline_alloc_rate = 0.0;
    g_perf_monitor.baseline_dealloc_rate = 0.0;
    g_perf_monitor.regression_detected = false;
    
    pthread_mutex_init(&g_perf_monitor.lock, NULL);
    g_perf_monitor.initialized = true;
}

// Configure performance monitoring
void wyn_perf_configure(bool enabled, bool detailed_logging, double regression_threshold) {
    wyn_perf_init();
    
    pthread_mutex_lock(&g_perf_monitor.lock);
    g_perf_monitor.config.enabled = enabled;
    g_perf_monitor.config.detailed_logging = detailed_logging;
    g_perf_monitor.config.regression_threshold = regression_threshold;
    pthread_mutex_unlock(&g_perf_monitor.lock);
}

// Record ARC allocation
void wyn_perf_record_alloc(uint64_t time_us) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    
    atomic_fetch_add(&g_perf_monitor.total_arc_allocs, 1);
    atomic_fetch_add(&g_perf_monitor.total_alloc_time_us, time_us);
}

// Record ARC deallocation
void wyn_perf_record_dealloc(uint64_t time_us) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    
    atomic_fetch_add(&g_perf_monitor.total_arc_deallocs, 1);
    atomic_fetch_add(&g_perf_monitor.total_dealloc_time_us, time_us);
}

// Record retain operation
void wyn_perf_record_retain(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_retains, 1);
}

// Record release operation
void wyn_perf_record_release(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_releases, 1);
}

// Record weak reference creation
void wyn_perf_record_weak_create(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_weak_creates, 1);
}

// Record weak reference destruction
void wyn_perf_record_weak_destroy(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_weak_destroys, 1);
}

// Record cycle collection
void wyn_perf_record_cycle_collection(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_cycle_collections, 1);
}

// Record pool allocation
void wyn_perf_record_pool_alloc(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_pool_allocs, 1);
}

// Record pool free
void wyn_perf_record_pool_free(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    atomic_fetch_add(&g_perf_monitor.total_pool_frees, 1);
}

// Take performance sample
static void take_performance_sample(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    
    pthread_mutex_lock(&g_perf_monitor.lock);
    
    size_t index = g_perf_monitor.current_sample;
    PerformanceSample* sample = &g_perf_monitor.samples[index];
    
    // Record timestamp
    gettimeofday(&sample->timestamp, NULL);
    
    // Get current counters
    size_t allocs = atomic_load(&g_perf_monitor.total_arc_allocs);
    size_t deallocs = atomic_load(&g_perf_monitor.total_arc_deallocs);
    size_t retains = atomic_load(&g_perf_monitor.total_retains);
    size_t releases = atomic_load(&g_perf_monitor.total_releases);
    uint64_t alloc_time = atomic_load(&g_perf_monitor.total_alloc_time_us);
    uint64_t dealloc_time = atomic_load(&g_perf_monitor.total_dealloc_time_us);
    
    // Calculate rates (operations per second)
    static struct timeval last_sample_time = {0};
    if (last_sample_time.tv_sec > 0) {
        double time_diff = (sample->timestamp.tv_sec - last_sample_time.tv_sec) +
                          (sample->timestamp.tv_usec - last_sample_time.tv_usec) / 1000000.0;
        
        if (time_diff > 0) {
            static size_t last_allocs = 0, last_deallocs = 0;
            static size_t last_retains = 0, last_releases = 0;
            
            sample->allocations_per_sec = (size_t)((allocs - last_allocs) / time_diff);
            sample->deallocations_per_sec = (size_t)((deallocs - last_deallocs) / time_diff);
            sample->retains_per_sec = (size_t)((retains - last_retains) / time_diff);
            sample->releases_per_sec = (size_t)((releases - last_releases) / time_diff);
            
            last_allocs = allocs;
            last_deallocs = deallocs;
            last_retains = retains;
            last_releases = releases;
        }
    }
    last_sample_time = sample->timestamp;
    
    // Calculate average times
    sample->avg_alloc_time_us = allocs > 0 ? (double)alloc_time / allocs : 0.0;
    sample->avg_dealloc_time_us = deallocs > 0 ? (double)dealloc_time / deallocs : 0.0;
    
    // Get memory usage from ARC stats
    WynARCStats arc_stats = wyn_arc_get_stats();
    sample->memory_usage = arc_stats.total_memory;
    
    // Update sample tracking
    g_perf_monitor.current_sample = (index + 1) % g_perf_monitor.config.max_samples;
    if (g_perf_monitor.sample_count < g_perf_monitor.config.max_samples) {
        g_perf_monitor.sample_count++;
    }
    
    pthread_mutex_unlock(&g_perf_monitor.lock);
}

// Check for performance regression
static void check_regression(void) {
    if (!g_perf_monitor.initialized || g_perf_monitor.sample_count < 10) return;
    
    pthread_mutex_lock(&g_perf_monitor.lock);
    
    // Calculate recent average rates
    double recent_alloc_rate = 0.0;
    double recent_dealloc_rate = 0.0;
    size_t recent_samples = 5; // Last 5 samples
    
    for (size_t i = 0; i < recent_samples && i < g_perf_monitor.sample_count; i++) {
        size_t idx = (g_perf_monitor.current_sample - 1 - i + g_perf_monitor.config.max_samples) % g_perf_monitor.config.max_samples;
        recent_alloc_rate += g_perf_monitor.samples[idx].allocations_per_sec;
        recent_dealloc_rate += g_perf_monitor.samples[idx].deallocations_per_sec;
    }
    recent_alloc_rate /= recent_samples;
    recent_dealloc_rate /= recent_samples;
    
    // Set baseline if not set
    if (g_perf_monitor.baseline_alloc_rate == 0.0) {
        g_perf_monitor.baseline_alloc_rate = recent_alloc_rate;
        g_perf_monitor.baseline_dealloc_rate = recent_dealloc_rate;
        pthread_mutex_unlock(&g_perf_monitor.lock);
        return;
    }
    
    // Check for regression
    double alloc_degradation = (g_perf_monitor.baseline_alloc_rate - recent_alloc_rate) / g_perf_monitor.baseline_alloc_rate;
    double dealloc_degradation = (g_perf_monitor.baseline_dealloc_rate - recent_dealloc_rate) / g_perf_monitor.baseline_dealloc_rate;
    
    if (alloc_degradation > g_perf_monitor.config.regression_threshold ||
        dealloc_degradation > g_perf_monitor.config.regression_threshold) {
        
        g_perf_monitor.regression_detected = true;
        snprintf(g_perf_monitor.regression_message, sizeof(g_perf_monitor.regression_message),
                "Performance regression detected: alloc %.1f%%, dealloc %.1f%%",
                alloc_degradation * 100, dealloc_degradation * 100);
    }
    
    pthread_mutex_unlock(&g_perf_monitor.lock);
}

// Trigger performance sampling
void wyn_perf_sample(void) {
    if (!g_perf_monitor.initialized || !g_perf_monitor.config.enabled) return;
    
    static _Atomic size_t operation_count = 0;
    size_t count = atomic_fetch_add(&operation_count, 1);
    
    if (count % g_perf_monitor.config.sample_interval == 0) {
        take_performance_sample();
        check_regression();
    }
}

// Performance statistics
// (Defined in arc_runtime.h)
WynPerfStats wyn_perf_get_stats(void) {
    wyn_perf_init();
    
    WynPerfStats stats = {0};
    
    stats.total_arc_allocs = atomic_load(&g_perf_monitor.total_arc_allocs);
    stats.total_arc_deallocs = atomic_load(&g_perf_monitor.total_arc_deallocs);
    stats.total_retains = atomic_load(&g_perf_monitor.total_retains);
    stats.total_releases = atomic_load(&g_perf_monitor.total_releases);
    stats.total_weak_creates = atomic_load(&g_perf_monitor.total_weak_creates);
    stats.total_weak_destroys = atomic_load(&g_perf_monitor.total_weak_destroys);
    stats.total_cycle_collections = atomic_load(&g_perf_monitor.total_cycle_collections);
    stats.total_pool_allocs = atomic_load(&g_perf_monitor.total_pool_allocs);
    stats.total_pool_frees = atomic_load(&g_perf_monitor.total_pool_frees);
    
    uint64_t total_alloc_time = atomic_load(&g_perf_monitor.total_alloc_time_us);
    uint64_t total_dealloc_time = atomic_load(&g_perf_monitor.total_dealloc_time_us);
    
    stats.avg_alloc_time_us = stats.total_arc_allocs > 0 ? (double)total_alloc_time / stats.total_arc_allocs : 0.0;
    stats.avg_dealloc_time_us = stats.total_arc_deallocs > 0 ? (double)total_dealloc_time / stats.total_arc_deallocs : 0.0;
    
    WynARCStats arc_stats = wyn_arc_get_stats();
    stats.current_memory_usage = arc_stats.total_memory;
    
    pthread_mutex_lock(&g_perf_monitor.lock);
    stats.sample_count = g_perf_monitor.sample_count;
    stats.regression_detected = g_perf_monitor.regression_detected;
    strncpy(stats.regression_message, g_perf_monitor.regression_message, sizeof(stats.regression_message) - 1);
    pthread_mutex_unlock(&g_perf_monitor.lock);
    
    return stats;
}

void wyn_perf_reset_stats(void) {
    if (!g_perf_monitor.initialized) return;
    
    atomic_store(&g_perf_monitor.total_arc_allocs, 0);
    atomic_store(&g_perf_monitor.total_arc_deallocs, 0);
    atomic_store(&g_perf_monitor.total_retains, 0);
    atomic_store(&g_perf_monitor.total_releases, 0);
    atomic_store(&g_perf_monitor.total_weak_creates, 0);
    atomic_store(&g_perf_monitor.total_weak_destroys, 0);
    atomic_store(&g_perf_monitor.total_cycle_collections, 0);
    atomic_store(&g_perf_monitor.total_pool_allocs, 0);
    atomic_store(&g_perf_monitor.total_pool_frees, 0);
    atomic_store(&g_perf_monitor.total_alloc_time_us, 0);
    atomic_store(&g_perf_monitor.total_dealloc_time_us, 0);
    
    pthread_mutex_lock(&g_perf_monitor.lock);
    g_perf_monitor.sample_count = 0;
    g_perf_monitor.current_sample = 0;
    g_perf_monitor.baseline_alloc_rate = 0.0;
    g_perf_monitor.baseline_dealloc_rate = 0.0;
    g_perf_monitor.regression_detected = false;
    g_perf_monitor.regression_message[0] = '\0';
    pthread_mutex_unlock(&g_perf_monitor.lock);
}

void wyn_perf_print_stats(void) {
    WynPerfStats stats = wyn_perf_get_stats();
    
    printf("=== ARC Performance Statistics ===\n");
    printf("ARC allocations: %zu\n", stats.total_arc_allocs);
    printf("ARC deallocations: %zu\n", stats.total_arc_deallocs);
    printf("Retain operations: %zu\n", stats.total_retains);
    printf("Release operations: %zu\n", stats.total_releases);
    printf("Weak creates: %zu\n", stats.total_weak_creates);
    printf("Weak destroys: %zu\n", stats.total_weak_destroys);
    printf("Cycle collections: %zu\n", stats.total_cycle_collections);
    printf("Pool allocations: %zu\n", stats.total_pool_allocs);
    printf("Pool frees: %zu\n", stats.total_pool_frees);
    printf("Avg alloc time: %.2f μs\n", stats.avg_alloc_time_us);
    printf("Avg dealloc time: %.2f μs\n", stats.avg_dealloc_time_us);
    printf("Current memory: %zu bytes\n", stats.current_memory_usage);
    printf("Performance samples: %zu\n", stats.sample_count);
    
    if (stats.regression_detected) {
        printf("⚠️  REGRESSION: %s\n", stats.regression_message);
    } else {
        printf("✅ No performance regression detected\n");
    }
    printf("==================================\n");
}

// Cleanup performance monitoring
void wyn_perf_cleanup(void) {
    if (!g_perf_monitor.initialized) return;
    
    pthread_mutex_lock(&g_perf_monitor.lock);
    
    if (g_perf_monitor.samples) {
        free(g_perf_monitor.samples);
        g_perf_monitor.samples = NULL;
    }
    
    pthread_mutex_unlock(&g_perf_monitor.lock);
    pthread_mutex_destroy(&g_perf_monitor.lock);
    
    g_perf_monitor.initialized = false;
}
