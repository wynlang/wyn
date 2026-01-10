#include "../src/performance.h"
#include <stdio.h>
#include <stdlib.h>

// Simple working examples
void simple_profiling_example() {
    printf("=== Simple Profiling Example ===\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    wyn_profiler_start(profiler);
    
    wyn_profiler_sample_start(profiler, "simple_operation");
    
    // Simple computation
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    
    wyn_profiler_sample_end(profiler, "simple_operation");
    
    size_t count;
    WynPerfSample* samples = wyn_profiler_get_samples(profiler, &count);
    
    printf("Operation completed in %.6f ms\n", samples[0].duration_ms);
    
    wyn_profiler_free(profiler);
}

void simple_benchmark_example() {
    printf("\n=== Simple Benchmark Example ===\n");
    
    WynBenchmark* benchmark = wyn_benchmark_new("simple_test");
    
    WynBenchmarkConfig config = wyn_benchmark_config_default("simple_test");
    config.iterations = 100;
    config.warmup_time_ms = 1.0;
    
    wyn_benchmark_configure(benchmark, &config);
    
    printf("Benchmark configured: %zu iterations\n", config.iterations);
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_free(benchmark);
}

void simple_optimizer_example() {
    printf("\n=== Simple Optimizer Example ===\n");
    
    WynOptimizer* optimizer = wyn_optimizer_new();
    
    printf("Optimizer created with level: %d\n", optimizer->config.level);
    printf("Inlining enabled: %s\n", optimizer->config.enable_inlining ? "yes" : "no");
    
    wyn_optimizer_free(optimizer);
}

int main() {
    printf("Wyn Performance Optimization - Simple Examples\n");
    printf("==============================================\n\n");
    
    simple_profiling_example();
    simple_benchmark_example();
    simple_optimizer_example();
    
    printf("\n✓ Simple examples completed successfully!\n");
    return 0;
}
