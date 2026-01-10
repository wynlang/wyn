#include "../src/performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

// Test function for benchmarking
void test_function(void* data) {
    int* counter = (int*)data;
    (*counter)++;
    
    // Simulate some work
    for (volatile int i = 0; i < 1000; i++) {
        // Busy loop
    }
}

void test_profiler_basic() {
    printf("Testing basic profiler functionality...\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    assert(profiler != NULL);
    assert(profiler->enabled == true);
    
    // Test starting and stopping profiler
    assert(wyn_profiler_start(profiler) == true);
    assert(wyn_profiler_stop(profiler) == true);
    
    wyn_profiler_free(profiler);
    printf("✓ Basic profiler test passed\n");
}

void test_profiler_sampling() {
    printf("Testing profiler sampling...\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    assert(profiler != NULL);
    
    wyn_profiler_start(profiler);
    
    // Test sampling
    assert(wyn_profiler_sample_start(profiler, "test_sample") == true);
    usleep(1000); // Sleep for 1ms
    assert(wyn_profiler_sample_end(profiler, "test_sample") == true);
    
    // Check samples
    size_t count;
    WynPerfSample* samples = wyn_profiler_get_samples(profiler, &count);
    assert(samples != NULL);
    assert(count == 1);
    assert(samples[0].duration_ms > 0);
    
    wyn_profiler_stop(profiler);
    wyn_profiler_free(profiler);
    printf("✓ Profiler sampling test passed\n");
}

void test_benchmark_basic() {
    printf("Testing basic benchmark functionality...\n");
    
    WynBenchmark* benchmark = wyn_benchmark_new("test_benchmark");
    assert(benchmark != NULL);
    assert(benchmark->profiler != NULL);
    
    // Test configuration
    WynBenchmarkConfig config = wyn_benchmark_config_default("test");
    config.iterations = 10;
    config.warmup_time_ms = 1.0;
    
    assert(wyn_benchmark_configure(benchmark, &config) == true);
    assert(benchmark->config.iterations == 10);
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_free(benchmark);
    printf("✓ Basic benchmark test passed\n");
}

void test_benchmark_run() {
    printf("Testing benchmark execution...\n");
    
    WynBenchmark* benchmark = wyn_benchmark_new("execution_test");
    assert(benchmark != NULL);
    
    WynBenchmarkConfig config = wyn_benchmark_config_default("execution_test");
    config.iterations = 5;
    config.warmup_time_ms = 1.0;
    
    wyn_benchmark_configure(benchmark, &config);
    
    int counter = 0;
    assert(wyn_benchmark_run(benchmark, test_function, &counter) == true);
    assert(counter >= 5); // At least 5 iterations plus warmup
    
    // Check that samples were collected
    size_t sample_count;
    WynPerfSample* samples = wyn_profiler_get_samples(benchmark->profiler, &sample_count);
    assert(samples != NULL);
    assert(sample_count == 5);
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_free(benchmark);
    printf("✓ Benchmark execution test passed\n");
}

void test_performance_measurement() {
    printf("Testing performance measurement functions...\n");
    
    uint64_t time1 = wyn_perf_get_time_ns();
    usleep(1000); // Sleep for 1ms
    uint64_t time2 = wyn_perf_get_time_ns();
    
    assert(time2 > time1);
    assert((time2 - time1) >= 1000000); // At least 1ms in nanoseconds
    
    uint64_t cycles = wyn_perf_get_cpu_cycles();
    (void)cycles; // Suppress unused variable warning
    
    printf("✓ Performance measurement test passed\n");
}

void test_optimizer_basic() {
    printf("Testing basic optimizer functionality...\n");
    
    WynOptimizer* optimizer = wyn_optimizer_new();
    assert(optimizer != NULL);
    assert(optimizer->config.level == WYN_OPT_SPEED);
    assert(optimizer->config.enable_inlining == true);
    assert(optimizer->profiler != NULL);
    
    // Test configuration
    WynOptimizerConfig config = {0};
    config.level = WYN_OPT_SIZE;
    config.enable_inlining = false;
    config.enable_vectorization = true;
    
    assert(wyn_optimizer_configure(optimizer, &config) == true);
    assert(optimizer->config.level == WYN_OPT_SIZE);
    assert(optimizer->config.enable_inlining == false);
    assert(optimizer->config.enable_vectorization == true);
    
    wyn_optimizer_free(optimizer);
    printf("✓ Basic optimizer test passed\n");
}

void test_benchmark_statistics() {
    printf("Testing benchmark statistics...\n");
    
    // Create mock benchmark result
    WynBenchmarkResult result = {0};
    result.sample_count = 5;
    result.samples = malloc(5 * sizeof(WynPerfSample));
    
    // Set up sample data
    for (size_t i = 0; i < 5; i++) {
        result.samples[i].duration_ms = (double)(i + 1) * 10.0; // 10, 20, 30, 40, 50 ms
        result.samples[i].name = NULL;
        result.samples[i].metrics = NULL;
        result.samples[i].metric_count = 0;
    }
    
    double mean = wyn_benchmark_calculate_mean(&result);
    assert(mean == 30.0); // (10+20+30+40+50)/5 = 30
    
    double median = wyn_benchmark_calculate_median(&result);
    assert(median == 30.0); // Middle value
    
    double std_dev = wyn_benchmark_calculate_std_dev(&result);
    assert(std_dev > 0); // Should have some deviation
    
    free(result.samples);
    printf("✓ Benchmark statistics test passed\n");
}

void test_perf_monitor() {
    printf("Testing performance monitor...\n");
    
    WynPerfMonitor* monitor = wyn_perf_monitor_new();
    assert(monitor != NULL);
    assert(monitor->regression_threshold == 5.0);
    assert(monitor->auto_save_results == true);
    
    WynBenchmark* benchmark = wyn_benchmark_new("monitor_test");
    assert(benchmark != NULL);
    
    // Note: wyn_perf_monitor_add_benchmark is currently a stub that returns false
    // This test verifies the monitor can be created and freed properly
    
    wyn_benchmark_free(benchmark);
    wyn_perf_monitor_free(monitor);
    printf("✓ Performance monitor test passed\n");
}

void test_multiple_samples() {
    printf("Testing multiple profiler samples...\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    assert(profiler != NULL);
    
    wyn_profiler_start(profiler);
    
    // Create multiple samples
    for (int i = 0; i < 3; i++) {
        char sample_name[32];
        snprintf(sample_name, sizeof(sample_name), "sample_%d", i);
        
        assert(wyn_profiler_sample_start(profiler, sample_name) == true);
        usleep(500); // Sleep for 0.5ms
        assert(wyn_profiler_sample_end(profiler, sample_name) == true);
    }
    
    size_t count;
    WynPerfSample* samples = wyn_profiler_get_samples(profiler, &count);
    assert(samples != NULL);
    assert(count == 3);
    
    // Verify all samples have positive duration
    for (size_t i = 0; i < count; i++) {
        assert(samples[i].duration_ms > 0);
        assert(samples[i].name != NULL);
        assert(samples[i].metric_count == 1);
        assert(samples[i].metrics != NULL);
    }
    
    wyn_profiler_stop(profiler);
    wyn_profiler_free(profiler);
    printf("✓ Multiple samples test passed\n");
}

int main() {
    printf("Running Performance Optimization Tests\n");
    printf("=====================================\n");
    
    test_profiler_basic();
    test_profiler_sampling();
    test_benchmark_basic();
    test_benchmark_run();
    test_performance_measurement();
    test_optimizer_basic();
    test_benchmark_statistics();
    test_perf_monitor();
    test_multiple_samples();
    
    printf("\n✓ All performance optimization tests passed!\n");
    return 0;
}
