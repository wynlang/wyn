#include "../src/runtime_optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_runtime_optimizer_creation() {
    printf("Testing runtime optimizer creation...\n");
    
    WynRuntimeOptimizer* optimizer = wyn_runtime_optimizer_new();
    assert(optimizer != NULL);
    assert(optimizer->memory_layout != NULL);
    assert(optimizer->simd_optimizer != NULL);
    assert(optimizer->arc_optimizer != NULL);
    assert(optimizer->is_initialized == false);
    
    wyn_runtime_optimizer_free(optimizer);
    printf("✓ Runtime optimizer creation test passed\n");
}

void test_runtime_optimizer_initialization() {
    printf("Testing runtime optimizer initialization...\n");
    
    WynRuntimeOptimizer* optimizer = wyn_runtime_optimizer_new();
    assert(optimizer != NULL);
    
    assert(wyn_runtime_optimizer_initialize(optimizer) == true);
    assert(optimizer->is_initialized == true);
    
    // Check SIMD detection
    WynSIMDType detected = optimizer->simd_optimizer->detected_simd;
    printf("  Detected SIMD: %d\n", detected);
    assert(detected >= WYN_SIMD_NONE);
    
    wyn_runtime_optimizer_free(optimizer);
    printf("✓ Runtime optimizer initialization test passed\n");
}

void test_memory_layout_optimization() {
    printf("Testing memory layout optimization...\n");
    
    WynMemoryLayout* layout = wyn_memory_layout_new();
    assert(layout != NULL);
    assert(layout->cache_efficiency == 1.0);
    
    // Configure for cache optimization
    WynLayoutConfig config = wyn_get_default_layout_config();
    config.type = WYN_LAYOUT_CACHE_OPTIMIZED;
    assert(wyn_memory_layout_configure(layout, &config) == true);
    
    // Add some mock structs
    void* mock_struct1 = (void*)0x1000;
    void* mock_struct2 = (void*)0x2000;
    
    assert(wyn_memory_layout_add_struct(layout, mock_struct1, 100) == true);
    assert(wyn_memory_layout_add_struct(layout, mock_struct2, 200) == true);
    assert(layout->struct_count == 2);
    
    // Optimize layout
    assert(wyn_memory_layout_optimize(layout) == true);
    assert(layout->is_optimized == true);
    
    double efficiency = wyn_memory_layout_get_cache_efficiency(layout);
    printf("  Cache efficiency: %.2f\n", efficiency);
    assert(efficiency >= 1.0);
    
    wyn_memory_layout_free(layout);
    printf("✓ Memory layout optimization test passed\n");
}

void test_simd_optimization() {
    printf("Testing SIMD optimization...\n");
    
    WynSIMDOptimizer* optimizer = wyn_simd_optimizer_new();
    assert(optimizer != NULL);
    
    // Test SIMD detection
    WynSIMDType detected = wyn_simd_detect_capabilities();
    printf("  Detected SIMD capabilities: %d\n", detected);
    
    // Configure SIMD optimizer
    WynSIMDConfig config = wyn_get_default_simd_config();
    config.target_simd = WYN_SIMD_AVX2;
    assert(wyn_simd_optimizer_configure(optimizer, &config) == true);
    
    assert(optimizer->config.target_simd == WYN_SIMD_AVX2);
    assert(optimizer->max_vector_width == 32);
    assert(optimizer->supports_fma == true);
    
    // Test SIMD utilities
    assert(wyn_simd_is_available(WYN_SIMD_AVX2) == true);
    assert(wyn_simd_get_vector_width(WYN_SIMD_AVX2) == 32);
    assert(wyn_simd_get_vector_width(WYN_SIMD_AVX512) == 64);
    
    wyn_simd_optimizer_free(optimizer);
    printf("✓ SIMD optimization test passed\n");
}

void test_arc_optimization() {
    printf("Testing ARC optimization...\n");
    
    WynARCOptimizer* optimizer = wyn_arc_optimizer_new();
    assert(optimizer != NULL);
    assert(optimizer->optimization_ratio == 1.0);
    
    // Configure ARC optimizer
    WynARCConfig config = wyn_get_default_arc_config();
    config.enable_escape_analysis = true;
    assert(wyn_arc_optimizer_configure(optimizer, &config) == true);
    
    // Test object optimization
    void* mock_objects[] = {(void*)0x1000, (void*)0x2000, (void*)0x3000};
    
    for (size_t i = 0; i < 3; i++) {
        assert(wyn_arc_optimize_object(optimizer, mock_objects[i]) == true);
    }
    
    assert(optimizer->objects_tracked == 3);
    printf("  Objects tracked: %zu\n", optimizer->objects_tracked);
    printf("  Stack allocated: %zu\n", optimizer->stack_allocated_objects);
    printf("  Heap allocated: %zu\n", optimizer->heap_allocated_objects);
    printf("  Optimization ratio: %.2f\n", optimizer->optimization_ratio);
    
    assert(optimizer->optimization_ratio >= 1.0);
    
    wyn_arc_optimizer_free(optimizer);
    printf("✓ ARC optimization test passed\n");
}

void test_escape_analysis() {
    printf("Testing escape analysis...\n");
    
    void* mock_object = (void*)0x1000;
    void* mock_function = (void*)0x2000;
    
    WynEscapeInfo* info = wyn_analyze_object_escape(mock_object, mock_function);
    assert(info != NULL);
    assert(info->object == mock_object);
    
    printf("  Object escapes function: %s\n", info->escapes_function ? "yes" : "no");
    printf("  Object escapes thread: %s\n", info->escapes_thread ? "yes" : "no");
    printf("  Can stack allocate: %s\n", info->can_stack_allocate ? "yes" : "no");
    printf("  Lifetime estimate: %zu\n", info->lifetime_estimate);
    
    bool can_stack_alloc = wyn_can_stack_allocate(info);
    assert(can_stack_alloc == info->can_stack_allocate);
    
    wyn_escape_info_free(info);
    printf("✓ Escape analysis test passed\n");
}

void test_arc_batch_operations() {
    printf("Testing ARC batch operations...\n");
    
    WynARCBatch* batch = wyn_arc_batch_new(true); // retain batch
    assert(batch != NULL);
    assert(batch->is_retain_batch == true);
    assert(batch->object_count == 0);
    
    // Add objects to batch
    void* objects[] = {(void*)0x1000, (void*)0x2000, (void*)0x3000, (void*)0x4000};
    
    for (size_t i = 0; i < 4; i++) {
        assert(wyn_arc_batch_add_object(batch, objects[i]) == true);
    }
    
    assert(batch->object_count == 4);
    
    // Execute batch
    assert(wyn_arc_batch_execute(batch) == true);
    
    // Check savings
    size_t savings = wyn_arc_batch_get_savings(batch);
    printf("  Batch savings: %zu operations\n", savings);
    assert(savings == 3); // 4 objects - 1 = 3 saved operations
    
    wyn_arc_batch_free(batch);
    printf("✓ ARC batch operations test passed\n");
}

void test_cache_optimization() {
    printf("Testing cache optimization...\n");
    
    void* mock_data = malloc(1024);
    assert(mock_data != NULL);
    
    // Test cache optimization
    assert(wyn_optimize_cache_layout(mock_data, 1024, WYN_CACHE_TEMPORAL_LOCALITY) == true);
    assert(wyn_prefetch_data(mock_data, 1024) == true);
    
    // Test cache line size
    size_t cache_line_size = wyn_get_cache_line_size();
    printf("  Cache line size: %zu bytes\n", cache_line_size);
    assert(cache_line_size > 0);
    
    // Test alignment
    bool is_aligned = wyn_is_cache_aligned(mock_data, 8);
    printf("  Data is 8-byte aligned: %s\n", is_aligned ? "yes" : "no");
    
    free(mock_data);
    printf("✓ Cache optimization test passed\n");
}

void test_platform_detection() {
    printf("Testing platform detection...\n");
    
    WynPlatformType platform = wyn_detect_platform();
    printf("  Detected platform: %d\n", platform);
    
    const char* platform_names[] = {"x86_64", "ARM64", "WASM", "Auto-detect"};
    if (platform < 4) {
        printf("  Platform name: %s\n", platform_names[platform]);
    }
    
    assert(platform >= WYN_PLATFORM_X86_64 && platform <= WYN_PLATFORM_AUTO_DETECT);
    
    printf("✓ Platform detection test passed\n");
}

void test_configuration_helpers() {
    printf("Testing configuration helpers...\n");
    
    // Test layout config
    WynLayoutConfig layout_config = wyn_get_default_layout_config();
    assert(layout_config.type == WYN_LAYOUT_CACHE_OPTIMIZED);
    assert(layout_config.alignment == 16);
    assert(layout_config.cache_line_size == 64);
    
    // Test SIMD config
    WynSIMDConfig simd_config = wyn_get_default_simd_config();
    assert(simd_config.target_simd == WYN_SIMD_AUTO_DETECT);
    assert(simd_config.auto_vectorize == true);
    assert(simd_config.enable_fma == true);
    
    // Test ARC config
    WynARCConfig arc_config = wyn_get_default_arc_config();
    assert(arc_config.strategy == WYN_ARC_ESCAPE_ANALYSIS);
    assert(arc_config.enable_escape_analysis == true);
    assert(arc_config.retain_release_batch_size == 16);
    
    // Test advanced config
    WynAdvancedOptConfig advanced_config = wyn_get_default_advanced_config();
    assert(advanced_config.enable_loop_tiling == true);
    assert(advanced_config.tile_size == 64);
    assert(advanced_config.unroll_factor == 4);
    
    // Test platform config
    WynPlatformOptConfig platform_config = wyn_get_default_platform_config();
    assert(platform_config.platform == WYN_PLATFORM_AUTO_DETECT);
    assert(platform_config.enable_platform_intrinsics == true);
    
    printf("✓ Configuration helpers test passed\n");
}

void test_full_optimization_pipeline() {
    printf("Testing full optimization pipeline...\n");
    
    WynRuntimeOptimizer* optimizer = wyn_runtime_optimizer_new();
    assert(optimizer != NULL);
    
    // Initialize optimizer
    assert(wyn_runtime_optimizer_initialize(optimizer) == true);
    
    // Add some mock data to memory layout
    void* mock_structs[] = {(void*)0x1000, (void*)0x2000, (void*)0x3000};
    for (size_t i = 0; i < 3; i++) {
        wyn_memory_layout_add_struct(optimizer->memory_layout, mock_structs[i], 100 + i * 50);
    }
    
    // Run full optimization
    void* mock_program = (void*)0x5000;
    assert(wyn_runtime_optimizer_optimize(optimizer, mock_program) == true);
    
    // Check performance gain
    double gain = wyn_runtime_optimizer_get_performance_gain(optimizer);
    printf("  Overall performance gain: %.2f\n", gain);
    assert(gain >= 1.0);
    
    wyn_runtime_optimizer_free(optimizer);
    printf("✓ Full optimization pipeline test passed\n");
}

int main() {
    printf("Running Runtime Performance Optimization Tests\n");
    printf("==============================================\n");
    
    test_runtime_optimizer_creation();
    test_runtime_optimizer_initialization();
    test_memory_layout_optimization();
    test_simd_optimization();
    test_arc_optimization();
    test_escape_analysis();
    test_arc_batch_operations();
    test_cache_optimization();
    test_platform_detection();
    test_configuration_helpers();
    test_full_optimization_pipeline();
    
    printf("\n✓ All runtime performance optimization tests passed!\n");
    return 0;
}
