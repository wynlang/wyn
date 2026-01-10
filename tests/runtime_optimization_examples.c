#include "../src/runtime_optimization.h"
#include <stdio.h>
#include <stdlib.h>

// Example: Basic runtime optimization setup
void example_basic_runtime_optimization() {
    printf("=== Basic Runtime Optimization Example ===\n");
    
    WynRuntimeOptimizer* optimizer = wyn_runtime_optimizer_new();
    
    // Initialize with automatic detection
    wyn_runtime_optimizer_initialize(optimizer);
    
    printf("Runtime optimizer initialized:\n");
    printf("  SIMD available: %s\n", 
           optimizer->simd_optimizer->simd_available ? "yes" : "no");
    printf("  Detected SIMD type: %d\n", 
           optimizer->simd_optimizer->detected_simd);
    printf("  Max vector width: %zu bytes\n", 
           optimizer->simd_optimizer->max_vector_width);
    
    // Simulate program optimization
    void* mock_program = (void*)0x1000;
    bool success = wyn_runtime_optimizer_optimize(optimizer, mock_program);
    
    printf("Optimization result: %s\n", success ? "success" : "failed");
    printf("Performance gain: %.2fx\n", 
           wyn_runtime_optimizer_get_performance_gain(optimizer));
    
    wyn_runtime_optimizer_free(optimizer);
}

// Example: Memory layout optimization
void example_memory_layout_optimization() {
    printf("\n=== Memory Layout Optimization Example ===\n");
    
    WynMemoryLayout* layout = wyn_memory_layout_new();
    
    // Configure for different optimization strategies
    WynLayoutType strategies[] = {
        WYN_LAYOUT_SEQUENTIAL,
        WYN_LAYOUT_PACKED,
        WYN_LAYOUT_ALIGNED,
        WYN_LAYOUT_CACHE_OPTIMIZED,
        WYN_LAYOUT_SIMD_FRIENDLY
    };
    
    const char* strategy_names[] = {
        "Sequential",
        "Packed",
        "Aligned", 
        "Cache Optimized",
        "SIMD Friendly"
    };
    
    for (size_t i = 0; i < sizeof(strategies) / sizeof(strategies[0]); i++) {
        WynLayoutConfig config = wyn_get_default_layout_config();
        config.type = strategies[i];
        
        wyn_memory_layout_configure(layout, &config);
        
        // Add mock structs
        void* structs[] = {(void*)0x1000, (void*)0x2000, (void*)0x3000};
        size_t sizes[] = {64, 128, 256};
        
        for (size_t j = 0; j < 3; j++) {
            wyn_memory_layout_add_struct(layout, structs[j], sizes[j]);
        }
        
        wyn_memory_layout_optimize(layout);
        
        printf("%s layout:\n", strategy_names[i]);
        printf("  Cache efficiency: %.2f\n", 
               wyn_memory_layout_get_cache_efficiency(layout));
        printf("  Structs optimized: %zu\n", layout->struct_count);
        
        // Reset for next iteration
        layout->struct_count = 0;
        layout->cache_efficiency = 1.0;
        layout->is_optimized = false;
    }
    
    wyn_memory_layout_free(layout);
}

// Example: SIMD optimization capabilities
void example_simd_optimization() {
    printf("\n=== SIMD Optimization Example ===\n");
    
    // Detect available SIMD capabilities
    WynSIMDType detected = wyn_simd_detect_capabilities();
    printf("Detected SIMD: %d\n", detected);
    
    WynSIMDType simd_types[] = {
        WYN_SIMD_SSE2, WYN_SIMD_SSE4, WYN_SIMD_AVX, 
        WYN_SIMD_AVX2, WYN_SIMD_AVX512, WYN_SIMD_NEON
    };
    
    const char* simd_names[] = {
        "SSE2", "SSE4", "AVX", "AVX2", "AVX512", "NEON"
    };
    
    printf("\nSIMD capabilities:\n");
    for (size_t i = 0; i < sizeof(simd_types) / sizeof(simd_types[0]); i++) {
        bool available = wyn_simd_is_available(simd_types[i]);
        size_t width = wyn_simd_get_vector_width(simd_types[i]);
        
        printf("  %s: %s (width: %zu bytes)\n", 
               simd_names[i], available ? "available" : "not available", width);
    }
    
    // Configure SIMD optimizer
    WynSIMDOptimizer* optimizer = wyn_simd_optimizer_new();
    
    WynSIMDConfig config = wyn_get_default_simd_config();
    config.target_simd = detected;
    config.auto_vectorize = true;
    config.enable_fma = true;
    
    wyn_simd_optimizer_configure(optimizer, &config);
    
    printf("\nSIMD optimizer configuration:\n");
    printf("  Target SIMD: %d\n", optimizer->config.target_simd);
    printf("  Auto-vectorize: %s\n", optimizer->config.auto_vectorize ? "enabled" : "disabled");
    printf("  FMA support: %s\n", optimizer->supports_fma ? "yes" : "no");
    printf("  Max vector width: %zu bytes\n", optimizer->max_vector_width);
    
    wyn_simd_optimizer_free(optimizer);
}

// Example: ARC optimization with escape analysis
void example_arc_optimization() {
    printf("\n=== ARC Optimization Example ===\n");
    
    WynARCOptimizer* optimizer = wyn_arc_optimizer_new();
    
    // Configure for aggressive optimization
    WynARCConfig config = wyn_get_default_arc_config();
    config.enable_escape_analysis = true;
    config.enable_batch_operations = true;
    config.retain_release_batch_size = 8;
    
    wyn_arc_optimizer_configure(optimizer, &config);
    
    printf("ARC optimizer configuration:\n");
    printf("  Strategy: %d\n", config.strategy);
    printf("  Escape analysis: %s\n", config.enable_escape_analysis ? "enabled" : "disabled");
    printf("  Batch operations: %s\n", config.enable_batch_operations ? "enabled" : "disabled");
    printf("  Batch size: %zu\n", config.retain_release_batch_size);
    
    // Simulate object optimization
    void* objects[] = {
        (void*)0x1000, (void*)0x2000, (void*)0x3000, (void*)0x4000,
        (void*)0x5000, (void*)0x6000, (void*)0x7000, (void*)0x8000
    };
    
    printf("\nOptimizing objects:\n");
    for (size_t i = 0; i < 8; i++) {
        wyn_arc_optimize_object(optimizer, objects[i]);
    }
    
    printf("  Objects tracked: %zu\n", optimizer->objects_tracked);
    printf("  Stack allocated: %zu\n", optimizer->stack_allocated_objects);
    printf("  Heap allocated: %zu\n", optimizer->heap_allocated_objects);
    printf("  Optimization ratio: %.2f\n", optimizer->optimization_ratio);
    
    double stack_percentage = (double)optimizer->stack_allocated_objects / 
                             optimizer->objects_tracked * 100.0;
    printf("  Stack allocation rate: %.1f%%\n", stack_percentage);
    
    wyn_arc_optimizer_free(optimizer);
}

// Example: Escape analysis workflow
void example_escape_analysis() {
    printf("\n=== Escape Analysis Example ===\n");
    
    void* objects[] = {(void*)0x1000, (void*)0x2000, (void*)0x3000, (void*)0x4000};
    void* function_context = (void*)0x5000;
    
    printf("Analyzing object escape behavior:\n");
    
    for (size_t i = 0; i < 4; i++) {
        WynEscapeInfo* info = wyn_analyze_object_escape(objects[i], function_context);
        
        printf("  Object %zu:\n", i + 1);
        printf("    Escapes function: %s\n", info->escapes_function ? "yes" : "no");
        printf("    Escapes thread: %s\n", info->escapes_thread ? "yes" : "no");
        printf("    Can stack allocate: %s\n", info->can_stack_allocate ? "yes" : "no");
        printf("    Lifetime estimate: %zu cycles\n", info->lifetime_estimate);
        
        if (wyn_can_stack_allocate(info)) {
            printf("    → Optimization: Use stack allocation\n");
        } else {
            printf("    → Optimization: Use heap allocation with ARC\n");
        }
        
        wyn_escape_info_free(info);
    }
}

// Example: ARC batch operations
void example_arc_batch_operations() {
    printf("\n=== ARC Batch Operations Example ===\n");
    
    // Create retain batch
    WynARCBatch* retain_batch = wyn_arc_batch_new(true);
    WynARCBatch* release_batch = wyn_arc_batch_new(false);
    
    void* objects[] = {
        (void*)0x1000, (void*)0x2000, (void*)0x3000, 
        (void*)0x4000, (void*)0x5000, (void*)0x6000
    };
    
    // Add objects to retain batch
    printf("Building retain batch:\n");
    for (size_t i = 0; i < 6; i++) {
        wyn_arc_batch_add_object(retain_batch, objects[i]);
        printf("  Added object %zu to retain batch\n", i + 1);
    }
    
    // Add objects to release batch
    printf("\nBuilding release batch:\n");
    for (size_t i = 0; i < 4; i++) {
        wyn_arc_batch_add_object(release_batch, objects[i]);
        printf("  Added object %zu to release batch\n", i + 1);
    }
    
    // Execute batches
    printf("\nExecuting batches:\n");
    wyn_arc_batch_execute(retain_batch);
    wyn_arc_batch_execute(release_batch);
    
    size_t retain_savings = wyn_arc_batch_get_savings(retain_batch);
    size_t release_savings = wyn_arc_batch_get_savings(release_batch);
    
    printf("  Retain batch savings: %zu operations\n", retain_savings);
    printf("  Release batch savings: %zu operations\n", release_savings);
    printf("  Total savings: %zu operations\n", retain_savings + release_savings);
    
    wyn_arc_batch_free(retain_batch);
    wyn_arc_batch_free(release_batch);
}

// Example: Cache optimization strategies
void example_cache_optimization() {
    printf("\n=== Cache Optimization Example ===\n");
    
    size_t cache_line_size = wyn_get_cache_line_size();
    printf("System cache line size: %zu bytes\n", cache_line_size);
    
    // Allocate test data
    void* data = malloc(4096);
    printf("Allocated data at: %p\n", data);
    
    // Test different cache hints
    WynCacheHint hints[] = {
        WYN_CACHE_TEMPORAL_LOCALITY,
        WYN_CACHE_SPATIAL_LOCALITY,
        WYN_CACHE_PREFETCH_HINT,
        WYN_CACHE_NON_TEMPORAL,
        WYN_CACHE_WRITE_COMBINING
    };
    
    const char* hint_names[] = {
        "Temporal Locality",
        "Spatial Locality", 
        "Prefetch Hint",
        "Non-Temporal",
        "Write Combining"
    };
    
    printf("\nApplying cache optimizations:\n");
    for (size_t i = 0; i < sizeof(hints) / sizeof(hints[0]); i++) {
        bool success = wyn_optimize_cache_layout(data, 4096, hints[i]);
        printf("  %s: %s\n", hint_names[i], success ? "applied" : "failed");
    }
    
    // Test alignment
    printf("\nAlignment tests:\n");
    size_t alignments[] = {8, 16, 32, 64};
    for (size_t i = 0; i < sizeof(alignments) / sizeof(alignments[0]); i++) {
        bool aligned = wyn_is_cache_aligned(data, alignments[i]);
        printf("  %zu-byte aligned: %s\n", alignments[i], aligned ? "yes" : "no");
    }
    
    // Prefetch data
    bool prefetch_success = wyn_prefetch_data(data, 4096);
    printf("  Prefetch hint: %s\n", prefetch_success ? "applied" : "failed");
    
    free(data);
}

// Example: Platform-specific optimizations
void example_platform_optimization() {
    printf("\n=== Platform-Specific Optimization Example ===\n");
    
    WynPlatformType platform = wyn_detect_platform();
    
    const char* platform_names[] = {"x86_64", "ARM64", "WASM", "Auto-detect"};
    printf("Detected platform: %s\n", platform_names[platform]);
    
    // Get platform-specific configuration
    WynPlatformOptConfig config = wyn_get_default_platform_config();
    config.platform = platform;
    
    printf("Platform optimization configuration:\n");
    printf("  Platform intrinsics: %s\n", 
           config.enable_platform_intrinsics ? "enabled" : "disabled");
    printf("  CPU-specific optimizations: %s\n", 
           config.enable_cpu_specific_optimizations ? "enabled" : "disabled");
    printf("  Memory model optimizations: %s\n", 
           config.enable_memory_model_optimizations ? "enabled" : "disabled");
    
    // Platform-specific recommendations
    switch (platform) {
        case WYN_PLATFORM_X86_64:
            printf("\nRecommendations for x86_64:\n");
            printf("  - Use AVX/AVX2 for vectorization\n");
            printf("  - Enable branch prediction hints\n");
            printf("  - Optimize for 64-byte cache lines\n");
            break;
            
        case WYN_PLATFORM_ARM64:
            printf("\nRecommendations for ARM64:\n");
            printf("  - Use NEON for vectorization\n");
            printf("  - Leverage AArch64 addressing modes\n");
            printf("  - Optimize for weaker memory ordering\n");
            break;
            
        case WYN_PLATFORM_WASM:
            printf("\nRecommendations for WebAssembly:\n");
            printf("  - Minimize memory allocations\n");
            printf("  - Use SIMD.js when available\n");
            printf("  - Optimize for linear memory model\n");
            break;
            
        default:
            printf("\nUsing generic optimizations\n");
            break;
    }
}

int main() {
    printf("Wyn Runtime Performance Optimization Examples\n");
    printf("============================================\n\n");
    
    example_basic_runtime_optimization();
    example_memory_layout_optimization();
    example_simd_optimization();
    example_arc_optimization();
    example_escape_analysis();
    example_arc_batch_operations();
    example_cache_optimization();
    example_platform_optimization();
    
    printf("\n✓ All runtime performance optimization examples completed!\n");
    return 0;
}
