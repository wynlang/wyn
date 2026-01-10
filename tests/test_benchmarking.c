#include "../src/benchmarking.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_benchmark_suite_creation() {
    printf("Testing benchmark suite creation...\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("test_suite");
    assert(suite != NULL);
    assert(suite->suite_name != NULL);
    assert(strcmp(suite->suite_name, "test_suite") == 0);
    assert(suite->benchmark_count == 0);
    assert(suite->result_count == 0);
    assert(suite->baseline_language == WYN_LANG_C);
    
    wyn_benchmark_suite_free(suite);
    printf("✓ Benchmark suite creation test passed\n");
}

void test_benchmark_configuration() {
    printf("Testing benchmark configuration...\n");
    
    WynBenchmarkConfig config = wyn_create_benchmark_config("computation_test", WYN_BENCH_COMPUTATION);
    assert(config.name != NULL);
    assert(strcmp(config.name, "computation_test") == 0);
    assert(config.type == WYN_BENCH_COMPUTATION);
    assert(config.iterations == 1000);
    assert(config.timeout_seconds == 60.0);
    assert(config.measure_memory == true);
    assert(config.warmup_enabled == true);
    
    wyn_benchmark_config_free(&config);
    printf("✓ Benchmark configuration test passed\n");
}

void test_benchmark_suite_operations() {
    printf("Testing benchmark suite operations...\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("operations_test");
    assert(suite != NULL);
    
    // Add benchmarks
    WynBenchmarkConfig config1 = wyn_create_benchmark_config("computation", WYN_BENCH_COMPUTATION);
    WynBenchmarkConfig config2 = wyn_create_benchmark_config("memory", WYN_BENCH_MEMORY_ALLOCATION);
    WynBenchmarkConfig config3 = wyn_create_benchmark_config("io", WYN_BENCH_IO_OPERATIONS);
    
    assert(wyn_benchmark_suite_add_benchmark(suite, &config1) == true);
    assert(wyn_benchmark_suite_add_benchmark(suite, &config2) == true);
    assert(wyn_benchmark_suite_add_benchmark(suite, &config3) == true);
    
    assert(suite->benchmark_count == 3);
    
    // Check benchmark names
    assert(strcmp(suite->benchmarks[0].name, "computation") == 0);
    assert(strcmp(suite->benchmarks[1].name, "memory") == 0);
    assert(strcmp(suite->benchmarks[2].name, "io") == 0);
    
    wyn_benchmark_config_free(&config1);
    wyn_benchmark_config_free(&config2);
    wyn_benchmark_config_free(&config3);
    wyn_benchmark_suite_free(suite);
    printf("✓ Benchmark suite operations test passed\n");
}

void test_individual_benchmarks() {
    printf("Testing individual benchmarks...\n");
    
    WynPerformanceMetrics metrics;
    
    // Test computation benchmark
    assert(wyn_run_computation_benchmark("fibonacci", WYN_LANG_WYN, &metrics) == true);
    assert(metrics.execution_time_ms > 0);
    assert(metrics.memory_usage_bytes > 0);
    assert(metrics.throughput_ops_per_sec > 0);
    printf("  Computation benchmark: %.3f ms\n", metrics.execution_time_ms);
    
    // Test memory benchmark
    assert(wyn_run_memory_benchmark("allocation", WYN_LANG_C, &metrics) == true);
    assert(metrics.execution_time_ms > 0);
    assert(metrics.memory_usage_bytes > 0);
    printf("  Memory benchmark: %.3f ms\n", metrics.execution_time_ms);
    
    // Test I/O benchmark
    assert(wyn_run_io_benchmark("file_ops", WYN_LANG_RUST, &metrics) == true);
    assert(metrics.execution_time_ms > 0);
    printf("  I/O benchmark: %.3f ms\n", metrics.execution_time_ms);
    
    // Test string benchmark
    assert(wyn_run_string_benchmark("string_ops", WYN_LANG_GO, &metrics) == true);
    assert(metrics.execution_time_ms > 0);
    printf("  String benchmark: %.3f ms\n", metrics.execution_time_ms);
    
    // Test compilation benchmark
    assert(wyn_run_compilation_benchmark("compile_test", WYN_LANG_WYN, &metrics) == true);
    assert(metrics.execution_time_ms > 0);
    printf("  Compilation benchmark: %.3f ms\n", metrics.execution_time_ms);
    
    printf("✓ Individual benchmarks test passed\n");
}

void test_benchmark_suite_execution() {
    printf("Testing benchmark suite execution...\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("execution_test");
    assert(suite != NULL);
    
    // Add a simple benchmark
    WynBenchmarkConfig config = wyn_create_benchmark_config("simple_test", WYN_BENCH_COMPUTATION);
    config.iterations = 100; // Reduce iterations for faster testing
    
    assert(wyn_benchmark_suite_add_benchmark(suite, &config) == true);
    
    // Run benchmark for specific language
    assert(wyn_benchmark_suite_run_language(suite, WYN_LANG_WYN) == true);
    assert(suite->result_count == 1);
    
    // Check result
    WynBenchmarkResult* result = &suite->results[0];
    assert(result->success == true);
    assert(result->language == WYN_LANG_WYN);
    assert(strcmp(result->benchmark_name, "simple_test") == 0);
    assert(result->metrics.execution_time_ms > 0);
    
    printf("  Benchmark result: %.3f ms\n", result->metrics.execution_time_ms);
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_suite_free(suite);
    printf("✓ Benchmark suite execution test passed\n");
}

void test_performance_monitoring() {
    printf("Testing performance monitoring...\n");
    
    WynPerformanceMonitor* monitor = wyn_performance_monitor_new();
    assert(monitor != NULL);
    assert(monitor->regression_threshold == 5.0);
    assert(monitor->suite_count == 0);
    
    // Create and add a benchmark suite
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("monitor_test");
    WynBenchmarkConfig config = wyn_create_benchmark_config("test_bench", WYN_BENCH_COMPUTATION);
    config.iterations = 50; // Small for testing
    
    wyn_benchmark_suite_add_benchmark(suite, &config);
    assert(wyn_performance_monitor_add_suite(monitor, suite) == true);
    assert(monitor->suite_count == 1);
    
    // Run all suites
    assert(wyn_performance_monitor_run_all(monitor) == true);
    assert(monitor->last_run_timestamp > 0);
    
    wyn_benchmark_config_free(&config);
    wyn_performance_monitor_free(monitor);
    printf("✓ Performance monitoring test passed\n");
}

void test_regression_detection() {
    printf("Testing regression detection...\n");
    
    WynRegressionDetector* detector = wyn_regression_detector_new(10.0); // 10% threshold
    assert(detector != NULL);
    assert(detector->regression_threshold == 10.0);
    assert(detector->improvement_threshold == -10.0);
    
    // Create baseline and current results
    WynBenchmarkResult baseline = {0};
    baseline.benchmark_name = strdup("test_benchmark");
    baseline.language = WYN_LANG_WYN;
    baseline.metrics.execution_time_ms = 100.0;
    baseline.success = true;
    
    WynBenchmarkResult current = {0};
    current.benchmark_name = strdup("test_benchmark");
    current.language = WYN_LANG_WYN;
    current.metrics.execution_time_ms = 120.0; // 20% slower (regression)
    current.success = true;
    
    // Analyze regression
    assert(wyn_regression_detector_analyze(detector, &baseline, &current) == true);
    assert(detector->result_count == 1);
    
    // Check results
    size_t count;
    WynRegressionResult* results = wyn_regression_detector_get_results(detector, &count);
    assert(results != NULL);
    assert(count == 1);
    assert(results[0].is_regression == true);
    assert(results[0].regression_percent == 20.0);
    
    printf("  Detected regression: %.1f%%\n", results[0].regression_percent);
    
    assert(wyn_regression_detector_has_regressions(detector) == true);
    
    // Test improvement case
    WynBenchmarkResult improved = current;
    improved.metrics.execution_time_ms = 80.0; // 20% faster (improvement)
    
    assert(wyn_regression_detector_analyze(detector, &baseline, &improved) == true);
    results = wyn_regression_detector_get_results(detector, &count);
    assert(count == 2);
    assert(results[1].is_improvement == true);
    assert(results[1].regression_percent == -20.0);
    
    printf("  Detected improvement: %.1f%%\n", -results[1].regression_percent);
    
    free(baseline.benchmark_name);
    free(current.benchmark_name);
    wyn_regression_detector_free(detector);
    printf("✓ Regression detection test passed\n");
}

void test_language_comparison() {
    printf("Testing language comparison...\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("comparison_test");
    
    // Add benchmark
    WynBenchmarkConfig config = wyn_create_benchmark_config("comp_test", WYN_BENCH_COMPUTATION);
    config.iterations = 100;
    wyn_benchmark_suite_add_benchmark(suite, &config);
    
    // Run for multiple languages
    wyn_benchmark_suite_run_language(suite, WYN_LANG_WYN);
    wyn_benchmark_suite_run_language(suite, WYN_LANG_C);
    wyn_benchmark_suite_run_language(suite, WYN_LANG_RUST);
    
    assert(suite->result_count == 3);
    
    // Compare languages
    double speedup;
    assert(wyn_compare_languages(suite, WYN_LANG_WYN, WYN_LANG_C, &speedup) == true);
    printf("  Wyn vs C speedup: %.2fx\n", speedup);
    
    assert(wyn_compare_languages(suite, WYN_LANG_RUST, WYN_LANG_C, &speedup) == true);
    printf("  Rust vs C speedup: %.2fx\n", speedup);
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_suite_free(suite);
    printf("✓ Language comparison test passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test language names
    assert(strcmp(wyn_language_name(WYN_LANG_WYN), "Wyn") == 0);
    assert(strcmp(wyn_language_name(WYN_LANG_C), "C") == 0);
    assert(strcmp(wyn_language_name(WYN_LANG_RUST), "Rust") == 0);
    assert(strcmp(wyn_language_name(WYN_LANG_GO), "Go") == 0);
    
    // Test benchmark type names
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_COMPUTATION), "Computation") == 0);
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_MEMORY_ALLOCATION), "Memory Allocation") == 0);
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_IO_OPERATIONS), "I/O Operations") == 0);
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_STRING_PROCESSING), "String Processing") == 0);
    
    // Test relative performance calculation
    WynBenchmarkResult baseline = {0};
    baseline.metrics.execution_time_ms = 100.0;
    
    WynBenchmarkResult current = {0};
    current.metrics.execution_time_ms = 80.0; // 25% faster
    
    double relative = wyn_calculate_relative_performance(&baseline, &current);
    assert(relative == 1.25); // 1.25x faster
    
    printf("  Relative performance: %.2fx\n", relative);
    
    printf("✓ Utility functions test passed\n");
}

void test_standard_benchmarks() {
    printf("Testing standard benchmarks...\n");
    
    // Test that standard benchmarks are defined
    assert(wyn_fibonacci_benchmark.name != NULL);
    assert(strcmp(wyn_fibonacci_benchmark.name, "Fibonacci") == 0);
    
    assert(wyn_quicksort_benchmark.name != NULL);
    assert(strcmp(wyn_quicksort_benchmark.name, "Quicksort") == 0);
    
    assert(wyn_matrix_multiply_benchmark.name != NULL);
    assert(strcmp(wyn_matrix_multiply_benchmark.name, "Matrix Multiplication") == 0);
    
    assert(wyn_json_parsing_benchmark.name != NULL);
    assert(strcmp(wyn_json_parsing_benchmark.name, "JSON Parsing") == 0);
    
    assert(wyn_http_server_benchmark.name != NULL);
    assert(strcmp(wyn_http_server_benchmark.name, "HTTP Server") == 0);
    
    assert(wyn_file_io_benchmark.name != NULL);
    assert(strcmp(wyn_file_io_benchmark.name, "File I/O") == 0);
    
    printf("✓ Standard benchmarks test passed\n");
}

void test_full_benchmark_pipeline() {
    printf("Testing full benchmark pipeline...\n");
    
    // Create monitor
    WynPerformanceMonitor* monitor = wyn_performance_monitor_new();
    
    // Create suite with multiple benchmarks
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("full_pipeline");
    
    WynBenchmarkConfig configs[] = {
        wyn_create_benchmark_config("computation", WYN_BENCH_COMPUTATION),
        wyn_create_benchmark_config("memory", WYN_BENCH_MEMORY_ALLOCATION),
        wyn_create_benchmark_config("strings", WYN_BENCH_STRING_PROCESSING)
    };
    
    // Reduce iterations for faster testing
    for (size_t i = 0; i < 3; i++) {
        configs[i].iterations = 50;
        wyn_benchmark_suite_add_benchmark(suite, &configs[i]);
    }
    
    wyn_performance_monitor_add_suite(monitor, suite);
    
    // Run full pipeline
    assert(wyn_performance_monitor_run_all(monitor) == true);
    
    // Check results
    size_t result_count;
    WynBenchmarkResult* results = wyn_benchmark_suite_get_results(suite, &result_count);
    assert(results != NULL);
    assert(result_count > 0);
    
    printf("  Pipeline completed with %zu results\n", result_count);
    
    // Show some results
    for (size_t i = 0; i < (result_count < 5 ? result_count : 5); i++) {
        printf("    %s (%s): %.3f ms\n", 
               results[i].benchmark_name,
               wyn_language_name(results[i].language),
               results[i].metrics.execution_time_ms);
    }
    
    for (size_t i = 0; i < 3; i++) {
        wyn_benchmark_config_free(&configs[i]);
    }
    wyn_performance_monitor_free(monitor);
    printf("✓ Full benchmark pipeline test passed\n");
}

int main() {
    printf("Running Benchmarking and Monitoring Tests\n");
    printf("=========================================\n");
    
    test_benchmark_suite_creation();
    test_benchmark_configuration();
    test_benchmark_suite_operations();
    test_individual_benchmarks();
    test_benchmark_suite_execution();
    test_performance_monitoring();
    test_regression_detection();
    test_language_comparison();
    test_utility_functions();
    test_standard_benchmarks();
    test_full_benchmark_pipeline();
    
    printf("\n✓ All benchmarking and monitoring tests passed!\n");
    return 0;
}
