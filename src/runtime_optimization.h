#ifndef WYN_RUNTIME_OPTIMIZATION_H
#define WYN_RUNTIME_OPTIMIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynRuntimeOptimizer WynRuntimeOptimizer;
typedef struct WynMemoryLayout WynMemoryLayout;
typedef struct WynSIMDOptimizer WynSIMDOptimizer;
typedef struct WynARCOptimizer WynARCOptimizer;

// Memory layout optimization types
typedef enum {
    WYN_LAYOUT_SEQUENTIAL,
    WYN_LAYOUT_PACKED,
    WYN_LAYOUT_ALIGNED,
    WYN_LAYOUT_CACHE_OPTIMIZED,
    WYN_LAYOUT_SIMD_FRIENDLY
} WynLayoutType;

// SIMD instruction types
typedef enum {
    WYN_SIMD_NONE,
    WYN_SIMD_SSE2,
    WYN_SIMD_SSE4,
    WYN_SIMD_AVX,
    WYN_SIMD_AVX2,
    WYN_SIMD_AVX512,
    WYN_SIMD_NEON,
    WYN_SIMD_AUTO_DETECT
} WynSIMDType;

// ARC optimization strategies
typedef enum {
    WYN_ARC_STANDARD,
    WYN_ARC_ESCAPE_ANALYSIS,
    WYN_ARC_WEAK_REFERENCES,
    WYN_ARC_CYCLE_DETECTION,
    WYN_ARC_BATCH_OPERATIONS
} WynARCStrategy;

// Cache optimization hints
typedef enum {
    WYN_CACHE_TEMPORAL_LOCALITY,
    WYN_CACHE_SPATIAL_LOCALITY,
    WYN_CACHE_PREFETCH_HINT,
    WYN_CACHE_NON_TEMPORAL,
    WYN_CACHE_WRITE_COMBINING
} WynCacheHint;

// Memory layout configuration
typedef struct {
    WynLayoutType type;
    size_t alignment;
    size_t cache_line_size;
    bool enable_padding;
    bool enable_reordering;
    WynCacheHint cache_hint;
} WynLayoutConfig;

// Memory layout optimizer
typedef struct WynMemoryLayout {
    WynLayoutConfig config;
    size_t struct_count;
    size_t struct_capacity;
    void** structs;
    size_t* original_sizes;
    size_t* optimized_sizes;
    double cache_efficiency;
    bool is_optimized;
} WynMemoryLayout;

// SIMD optimization configuration
typedef struct {
    WynSIMDType target_simd;
    bool auto_vectorize;
    bool enable_unrolling;
    size_t vector_width;
    bool prefer_accuracy;
    bool enable_fma;
} WynSIMDConfig;

// SIMD optimizer
typedef struct WynSIMDOptimizer {
    WynSIMDConfig config;
    WynSIMDType detected_simd;
    bool simd_available;
    size_t max_vector_width;
    bool supports_fma;
    bool supports_gather_scatter;
} WynSIMDOptimizer;

// ARC optimization configuration
typedef struct {
    WynARCStrategy strategy;
    bool enable_escape_analysis;
    bool enable_weak_references;
    bool enable_cycle_detection;
    bool enable_batch_operations;
    size_t retain_release_batch_size;
    double escape_analysis_threshold;
} WynARCConfig;

// ARC optimizer
typedef struct WynARCOptimizer {
    WynARCConfig config;
    size_t objects_tracked;
    size_t stack_allocated_objects;
    size_t heap_allocated_objects;
    size_t cycles_detected;
    size_t retain_operations;
    size_t release_operations;
    double optimization_ratio;
} WynARCOptimizer;

// Runtime optimizer (main interface)
typedef struct WynRuntimeOptimizer {
    WynMemoryLayout* memory_layout;
    WynSIMDOptimizer* simd_optimizer;
    WynARCOptimizer* arc_optimizer;
    bool is_initialized;
    double overall_performance_gain;
} WynRuntimeOptimizer;

// Runtime optimizer functions
WynRuntimeOptimizer* wyn_runtime_optimizer_new(void);
void wyn_runtime_optimizer_free(WynRuntimeOptimizer* optimizer);
bool wyn_runtime_optimizer_initialize(WynRuntimeOptimizer* optimizer);
bool wyn_runtime_optimizer_optimize(WynRuntimeOptimizer* optimizer, void* program);
double wyn_runtime_optimizer_get_performance_gain(WynRuntimeOptimizer* optimizer);

// Memory layout optimization
WynMemoryLayout* wyn_memory_layout_new(void);
void wyn_memory_layout_free(WynMemoryLayout* layout);
bool wyn_memory_layout_configure(WynMemoryLayout* layout, const WynLayoutConfig* config);
bool wyn_memory_layout_add_struct(WynMemoryLayout* layout, void* struct_def, size_t size);
bool wyn_memory_layout_optimize(WynMemoryLayout* layout);
double wyn_memory_layout_get_cache_efficiency(WynMemoryLayout* layout);

// Cache optimization functions
bool wyn_optimize_cache_layout(void* data, size_t size, WynCacheHint hint);
bool wyn_prefetch_data(void* data, size_t size);
bool wyn_align_for_cache(void** data, size_t size, size_t alignment);
size_t wyn_get_cache_line_size(void);
bool wyn_is_cache_aligned(void* data, size_t alignment);

// SIMD optimization
WynSIMDOptimizer* wyn_simd_optimizer_new(void);
void wyn_simd_optimizer_free(WynSIMDOptimizer* optimizer);
bool wyn_simd_optimizer_configure(WynSIMDOptimizer* optimizer, const WynSIMDConfig* config);
WynSIMDType wyn_simd_detect_capabilities(void);
bool wyn_simd_optimize_loop(WynSIMDOptimizer* optimizer, void* loop_body);
bool wyn_simd_vectorize_operation(WynSIMDOptimizer* optimizer, void* operation);

// SIMD utility functions
bool wyn_simd_is_available(WynSIMDType type);
size_t wyn_simd_get_vector_width(WynSIMDType type);
bool wyn_simd_supports_operation(WynSIMDType type, const char* operation);
bool wyn_simd_can_vectorize(void* code_block);

// ARC optimization
WynARCOptimizer* wyn_arc_optimizer_new(void);
void wyn_arc_optimizer_free(WynARCOptimizer* optimizer);
bool wyn_arc_optimizer_configure(WynARCOptimizer* optimizer, const WynARCConfig* config);
bool wyn_arc_optimize_object(WynARCOptimizer* optimizer, void* object);
bool wyn_arc_perform_escape_analysis(WynARCOptimizer* optimizer, void* function);

// Escape analysis functions
typedef struct {
    void* object;
    bool escapes_function;
    bool escapes_thread;
    bool can_stack_allocate;
    size_t lifetime_estimate;
} WynEscapeInfo;

WynEscapeInfo* wyn_analyze_object_escape(void* object, void* function_context);
bool wyn_can_stack_allocate(const WynEscapeInfo* escape_info);
bool wyn_optimize_allocation_site(void* allocation_site, const WynEscapeInfo* escape_info);
void wyn_escape_info_free(WynEscapeInfo* info);

// ARC batch operations
typedef struct {
    void** objects;
    size_t object_count;
    size_t capacity;
    bool is_retain_batch;
} WynARCBatch;

WynARCBatch* wyn_arc_batch_new(bool is_retain_batch);
void wyn_arc_batch_free(WynARCBatch* batch);
bool wyn_arc_batch_add_object(WynARCBatch* batch, void* object);
bool wyn_arc_batch_execute(WynARCBatch* batch);
size_t wyn_arc_batch_get_savings(WynARCBatch* batch);

// Performance monitoring and profiling
typedef struct {
    double cpu_usage_before;
    double cpu_usage_after;
    size_t memory_usage_before;
    size_t memory_usage_after;
    double cache_miss_rate_before;
    double cache_miss_rate_after;
    double execution_time_before;
    double execution_time_after;
    double performance_improvement;
} WynPerformanceMetrics;

WynPerformanceMetrics* wyn_measure_performance_impact(WynRuntimeOptimizer* optimizer, 
                                                     void* code_before, void* code_after);
bool wyn_profile_cache_performance(void* code, WynPerformanceMetrics* metrics);
bool wyn_profile_simd_performance(void* code, WynPerformanceMetrics* metrics);
bool wyn_profile_arc_performance(void* code, WynPerformanceMetrics* metrics);

// Advanced optimization strategies
typedef struct {
    bool enable_loop_tiling;
    bool enable_loop_fusion;
    bool enable_loop_interchange;
    bool enable_software_pipelining;
    bool enable_branch_prediction_hints;
    size_t tile_size;
    size_t unroll_factor;
} WynAdvancedOptConfig;

bool wyn_apply_advanced_optimizations(WynRuntimeOptimizer* optimizer, 
                                     const WynAdvancedOptConfig* config, void* code);
bool wyn_optimize_loop_nest(void* loop_nest, const WynAdvancedOptConfig* config);
bool wyn_add_branch_prediction_hints(void* code);

// Platform-specific optimizations
typedef enum {
    WYN_PLATFORM_X86_64,
    WYN_PLATFORM_ARM64,
    WYN_PLATFORM_WASM,
    WYN_PLATFORM_AUTO_DETECT
} WynPlatformType;

typedef struct {
    WynPlatformType platform;
    bool enable_platform_intrinsics;
    bool enable_cpu_specific_optimizations;
    bool enable_memory_model_optimizations;
} WynPlatformOptConfig;

bool wyn_apply_platform_optimizations(WynRuntimeOptimizer* optimizer,
                                     const WynPlatformOptConfig* config);
WynPlatformType wyn_detect_platform(void);
bool wyn_optimize_for_platform(void* code, WynPlatformType platform);

// Configuration helpers
WynLayoutConfig wyn_get_default_layout_config(void);
WynSIMDConfig wyn_get_default_simd_config(void);
WynARCConfig wyn_get_default_arc_config(void);
WynAdvancedOptConfig wyn_get_default_advanced_config(void);
WynPlatformOptConfig wyn_get_default_platform_config(void);

#endif // WYN_RUNTIME_OPTIMIZATION_H
