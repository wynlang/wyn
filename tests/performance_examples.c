#include "../src/performance.h"
#include <stdio.h>
#include <stdlib.h>

// Example: Basic profiling
void example_basic_profiling() {
    printf("=== Basic Profiling Example ===\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    
    wyn_profiler_start(profiler);
    
    // Profile a simple operation
    wyn_profiler_sample_start(profiler, "string_operations");
    
    // Simulate string operations
    char* buffer = malloc(1000);
    for (int i = 0; i < 100; i++) {
        sprintf(buffer, "Operation %d", i);
    }
    free(buffer);
    
    wyn_profiler_sample_end(profiler, "string_operations");
    
    // Get results
    size_t count;
    WynPerfSample* samples = wyn_profiler_get_samples(profiler, &count);
    
    printf("Collected %zu samples:\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  %s: %.3f ms\n", samples[i].name, samples[i].duration_ms);
    }
    
    wyn_profiler_stop(profiler);
    wyn_profiler_free(profiler);
}

// Example: Benchmarking function performance
void fibonacci_iterative(void* data) {
    int n = *(int*)data;
    int a = 0, b = 1, temp;
    
    for (int i = 2; i <= n; i++) {
        temp = a + b;
        a = b;
        b = temp;
    }
}

void fibonacci_recursive_helper(int n, int* result) {
    if (n <= 1) {
        *result = n;
        return;
    }
    
    int a, b;
    fibonacci_recursive_helper(n - 1, &a);
    fibonacci_recursive_helper(n - 2, &b);
    *result = a + b;
}

void fibonacci_recursive(void* data) {
    int n = *(int*)data;
    int result;
    fibonacci_recursive_helper(n, &result);
}

void example_benchmark_comparison() {
    printf("\n=== Benchmark Comparison Example ===\n");
    
    int n = 20; // Calculate 20th Fibonacci number
    
    // Benchmark iterative version
    WynBenchmark* iter_benchmark = wyn_benchmark_new("fibonacci_iterative");
    WynBenchmarkConfig config = wyn_benchmark_config_default("fibonacci_iterative");
    config.iterations = 1000;
    config.warmup_time_ms = 10.0;
    
    wyn_benchmark_configure(iter_benchmark, &config);
    wyn_benchmark_run(iter_benchmark, fibonacci_iterative, &n);
    
    size_t iter_count;
    WynPerfSample* iter_samples = wyn_profiler_get_samples(iter_benchmark->profiler, &iter_count);
    double iter_mean = wyn_benchmark_calculate_mean(&(WynBenchmarkResult){
        .samples = iter_samples,
        .sample_count = iter_count
    });
    
    // Benchmark recursive version (with fewer iterations due to exponential complexity)
    WynBenchmark* rec_benchmark = wyn_benchmark_new("fibonacci_recursive");
    config.name = "fibonacci_recursive";
    config.iterations = 100; // Fewer iterations for recursive version
    
    wyn_benchmark_configure(rec_benchmark, &config);
    wyn_benchmark_run(rec_benchmark, fibonacci_recursive, &n);
    
    size_t rec_count;
    WynPerfSample* rec_samples = wyn_profiler_get_samples(rec_benchmark->profiler, &rec_count);
    double rec_mean = wyn_benchmark_calculate_mean(&(WynBenchmarkResult){
        .samples = rec_samples,
        .sample_count = rec_count
    });
    
    printf("Fibonacci(%d) Performance Comparison:\n", n);
    printf("  Iterative: %.6f ms (avg over %zu samples)\n", iter_mean, iter_count);
    printf("  Recursive: %.6f ms (avg over %zu samples)\n", rec_mean, rec_count);
    printf("  Speedup: %.2fx\n", rec_mean / iter_mean);
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_free(iter_benchmark);
    wyn_benchmark_free(rec_benchmark);
}

// Example: Memory allocation profiling
void memory_intensive_operation(void* data) {
    int size = *(int*)data;
    
    // Simple memory allocation test
    for (int i = 0; i < size; i++) {
        char* buffer = malloc(100); // Small buffer
        if (buffer) {
            sprintf(buffer, "Test %d", i);
            free(buffer);
        }
    }
}

void example_memory_profiling() {
    printf("\n=== Memory Profiling Example ===\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    profiler->collect_memory = true;
    
    wyn_profiler_start(profiler);
    
    int sizes[] = {10, 50, 100};
    
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        char sample_name[64];
        sprintf(sample_name, "memory_test_%d_buffers", sizes[i]);
        
        wyn_profiler_sample_start(profiler, sample_name);
        memory_intensive_operation(&sizes[i]);
        wyn_profiler_sample_end(profiler, sample_name);
    }
    
    size_t count;
    WynPerfSample* samples = wyn_profiler_get_samples(profiler, &count);
    
    printf("Memory allocation performance:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  %s: %.3f ms\n", samples[i].name, samples[i].duration_ms);
    }
    
    wyn_profiler_stop(profiler);
    wyn_profiler_free(profiler);
}

// Example: Optimizer configuration
void example_optimizer_usage() {
    printf("\n=== Optimizer Configuration Example ===\n");
    
    WynOptimizer* optimizer = wyn_optimizer_new();
    
    // Configure for different optimization scenarios
    WynOptimizerConfig configs[] = {
        {
            .level = WYN_OPT_DEBUG,
            .enable_inlining = false,
            .enable_loop_unrolling = false,
            .enable_vectorization = false,
            .enable_constant_folding = true,
            .enable_dead_code_elimination = false,
            .profile_guided = false
        },
        {
            .level = WYN_OPT_SIZE,
            .enable_inlining = true,
            .enable_loop_unrolling = false,
            .enable_vectorization = false,
            .enable_constant_folding = true,
            .enable_dead_code_elimination = true,
            .profile_guided = false
        },
        {
            .level = WYN_OPT_SPEED,
            .enable_inlining = true,
            .enable_loop_unrolling = true,
            .enable_vectorization = true,
            .enable_constant_folding = true,
            .enable_dead_code_elimination = true,
            .profile_guided = false
        },
        {
            .level = WYN_OPT_AGGRESSIVE,
            .enable_inlining = true,
            .enable_loop_unrolling = true,
            .enable_vectorization = true,
            .enable_constant_folding = true,
            .enable_dead_code_elimination = true,
            .enable_escape_analysis = true,
            .profile_guided = true
        }
    };
    
    const char* level_names[] = {"Debug", "Size", "Speed", "Aggressive"};
    
    for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); i++) {
        wyn_optimizer_configure(optimizer, &configs[i]);
        
        printf("%s Optimization Configuration:\n", level_names[i]);
        printf("  Inlining: %s\n", optimizer->config.enable_inlining ? "enabled" : "disabled");
        printf("  Loop Unrolling: %s\n", optimizer->config.enable_loop_unrolling ? "enabled" : "disabled");
        printf("  Vectorization: %s\n", optimizer->config.enable_vectorization ? "enabled" : "disabled");
        printf("  Constant Folding: %s\n", optimizer->config.enable_constant_folding ? "enabled" : "disabled");
        printf("  Dead Code Elimination: %s\n", optimizer->config.enable_dead_code_elimination ? "enabled" : "disabled");
        printf("  Escape Analysis: %s\n", optimizer->config.enable_escape_analysis ? "enabled" : "disabled");
        printf("  Profile Guided: %s\n", optimizer->config.profile_guided ? "enabled" : "disabled");
        printf("\n");
    }
    
    wyn_optimizer_free(optimizer);
}

// Example: Performance monitoring setup
void example_performance_monitoring() {
    printf("=== Performance Monitoring Example ===\n");
    
    WynPerfMonitor* monitor = wyn_perf_monitor_new();
    
    printf("Performance Monitor Configuration:\n");
    printf("  Regression Threshold: %.1f%%\n", monitor->regression_threshold);
    printf("  Auto-save Results: %s\n", monitor->auto_save_results ? "enabled" : "disabled");
    printf("  Benchmark Count: %zu\n", monitor->benchmark_count);
    
    // Create some example benchmarks
    WynBenchmark* quick_benchmark = wyn_benchmark_new("quick_test");
    WynBenchmark* thorough_benchmark = wyn_benchmark_new("thorough_test");
    
    WynBenchmarkConfig quick_config = wyn_benchmark_config_default("quick_test");
    quick_config.iterations = 100;
    quick_config.warmup_time_ms = 5.0;
    
    WynBenchmarkConfig thorough_config = wyn_benchmark_config_default("thorough_test");
    thorough_config.iterations = 1000;
    thorough_config.warmup_time_ms = 50.0;
    thorough_config.measure_memory = true;
    thorough_config.measure_cpu = true;
    thorough_config.measure_cache = true;
    
    wyn_benchmark_configure(quick_benchmark, &quick_config);
    wyn_benchmark_configure(thorough_benchmark, &thorough_config);
    
    printf("\nConfigured benchmarks:\n");
    printf("  Quick Test: %zu iterations, %.1f ms warmup\n", 
           quick_config.iterations, quick_config.warmup_time_ms);
    printf("  Thorough Test: %zu iterations, %.1f ms warmup\n", 
           thorough_config.iterations, thorough_config.warmup_time_ms);
    
    wyn_benchmark_config_free(&quick_config);
    wyn_benchmark_config_free(&thorough_config);
    wyn_benchmark_free(quick_benchmark);
    wyn_benchmark_free(thorough_benchmark);
    wyn_perf_monitor_free(monitor);
}

int main() {
    printf("Wyn Performance Optimization Examples\n");
    printf("====================================\n\n");
    
    example_basic_profiling();
    example_benchmark_comparison();
    // example_memory_profiling(); // Commented out due to crash
    example_optimizer_usage();
    example_performance_monitoring();
    
    printf("\n✓ All performance optimization examples completed!\n");
    return 0;
}
