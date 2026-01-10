#include "../src/benchmarking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example: Basic benchmark suite setup
void example_basic_benchmark_suite() {
    printf("=== Basic Benchmark Suite Example ===\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("performance_comparison");
    
    // Create different types of benchmarks
    WynBenchmarkConfig benchmarks[] = {
        wyn_create_benchmark_config("fibonacci", WYN_BENCH_COMPUTATION),
        wyn_create_benchmark_config("memory_alloc", WYN_BENCH_MEMORY_ALLOCATION),
        wyn_create_benchmark_config("file_io", WYN_BENCH_IO_OPERATIONS),
        wyn_create_benchmark_config("string_ops", WYN_BENCH_STRING_PROCESSING)
    };
    
    // Configure benchmarks for faster execution
    for (size_t i = 0; i < 4; i++) {
        benchmarks[i].iterations = 100;
        benchmarks[i].timeout_seconds = 30.0;
        wyn_benchmark_suite_add_benchmark(suite, &benchmarks[i]);
    }
    
    printf("Created benchmark suite '%s' with %zu benchmarks:\n", 
           suite->suite_name, suite->benchmark_count);
    
    for (size_t i = 0; i < suite->benchmark_count; i++) {
        printf("  %zu. %s (%s)\n", 
               i + 1, 
               suite->benchmarks[i].name,
               wyn_benchmark_type_name(suite->benchmarks[i].type));
    }
    
    // Cleanup
    for (size_t i = 0; i < 4; i++) {
        wyn_benchmark_config_free(&benchmarks[i]);
    }
    wyn_benchmark_suite_free(suite);
}

// Example: Language performance comparison
void example_language_comparison() {
    printf("\n=== Language Performance Comparison Example ===\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("language_comparison");
    
    // Add computation benchmark
    WynBenchmarkConfig config = wyn_create_benchmark_config("computation_test", WYN_BENCH_COMPUTATION);
    config.iterations = 200;
    wyn_benchmark_suite_add_benchmark(suite, &config);
    
    // Run benchmarks for different languages
    WynLanguage languages[] = {WYN_LANG_WYN, WYN_LANG_C, WYN_LANG_RUST, WYN_LANG_GO};
    const char* lang_names[] = {"Wyn", "C", "Rust", "Go"};
    
    printf("Running computation benchmark across languages:\n");
    
    for (size_t i = 0; i < 4; i++) {
        printf("  Running %s benchmark...\n", lang_names[i]);
        wyn_benchmark_suite_run_language(suite, languages[i]);
    }
    
    // Get and display results
    size_t result_count;
    WynBenchmarkResult* results = wyn_benchmark_suite_get_results(suite, &result_count);
    
    printf("\nResults:\n");
    for (size_t i = 0; i < result_count; i++) {
        printf("  %s: %.3f ms (%.2f MB memory)\n",
               wyn_language_name(results[i].language),
               results[i].metrics.execution_time_ms,
               results[i].metrics.memory_usage_bytes / (1024.0 * 1024.0));
    }
    
    // Compare languages
    double speedup;
    if (wyn_compare_languages(suite, WYN_LANG_WYN, WYN_LANG_C, &speedup)) {
        printf("\nWyn vs C performance: %.2fx\n", speedup);
    }
    
    if (wyn_compare_languages(suite, WYN_LANG_RUST, WYN_LANG_C, &speedup)) {
        printf("Rust vs C performance: %.2fx\n", speedup);
    }
    
    if (wyn_compare_languages(suite, WYN_LANG_GO, WYN_LANG_C, &speedup)) {
        printf("Go vs C performance: %.2fx\n", speedup);
    }
    
    wyn_benchmark_config_free(&config);
    wyn_benchmark_suite_free(suite);
}

// Example: Performance regression detection
void example_regression_detection() {
    printf("\n=== Performance Regression Detection Example ===\n");
    
    WynRegressionDetector* detector = wyn_regression_detector_new(5.0); // 5% threshold
    
    // Simulate baseline results
    WynBenchmarkResult baseline_results[] = {
        {
            .benchmark_name = "fibonacci",
            .language = WYN_LANG_WYN,
            .metrics = {.execution_time_ms = 100.0},
            .success = true
        },
        {
            .benchmark_name = "quicksort", 
            .language = WYN_LANG_WYN,
            .metrics = {.execution_time_ms = 50.0},
            .success = true
        },
        {
            .benchmark_name = "json_parse",
            .language = WYN_LANG_WYN, 
            .metrics = {.execution_time_ms = 25.0},
            .success = true
        }
    };
    
    // Simulate current results with some regressions and improvements
    WynBenchmarkResult current_results[] = {
        {
            .benchmark_name = "fibonacci",
            .language = WYN_LANG_WYN,
            .metrics = {.execution_time_ms = 110.0}, // 10% slower (regression)
            .success = true
        },
        {
            .benchmark_name = "quicksort",
            .language = WYN_LANG_WYN,
            .metrics = {.execution_time_ms = 45.0}, // 10% faster (improvement)
            .success = true
        },
        {
            .benchmark_name = "json_parse",
            .language = WYN_LANG_WYN,
            .metrics = {.execution_time_ms = 26.0}, // 4% slower (within threshold)
            .success = true
        }
    };
    
    printf("Analyzing performance changes (threshold: %.1f%%):\n", detector->regression_threshold);
    
    // Analyze each benchmark
    for (size_t i = 0; i < 3; i++) {
        // Create copies with allocated strings for the detector
        WynBenchmarkResult baseline_copy = baseline_results[i];
        WynBenchmarkResult current_copy = current_results[i];
        
        baseline_copy.benchmark_name = strdup(baseline_results[i].benchmark_name);
        current_copy.benchmark_name = strdup(current_results[i].benchmark_name);
        
        wyn_regression_detector_analyze(detector, &baseline_copy, &current_copy);
        
        // Free the allocated strings
        free(baseline_copy.benchmark_name);
        free(current_copy.benchmark_name);
    }
    
    // Get and display results
    size_t result_count;
    WynRegressionResult* results = wyn_regression_detector_get_results(detector, &result_count);
    
    for (size_t i = 0; i < result_count; i++) {
        WynRegressionResult* result = &results[i];
        printf("  %s: %.1f ms → %.1f ms (%.1f%% change) - ", 
               result->benchmark_name,
               result->baseline_time,
               result->current_time,
               result->regression_percent);
        
        if (result->is_regression) {
            printf("REGRESSION ⚠️\n");
        } else if (result->is_improvement) {
            printf("IMPROVEMENT ✓\n");
        } else {
            printf("OK\n");
        }
    }
    
    printf("\nOverall status: %s\n", 
           wyn_regression_detector_has_regressions(detector) ? "REGRESSIONS DETECTED" : "NO REGRESSIONS");
    
    wyn_regression_detector_free(detector);
}

// Example: Performance monitoring setup
void example_performance_monitoring() {
    printf("\n=== Performance Monitoring Setup Example ===\n");
    
    WynPerformanceMonitor* monitor = wyn_performance_monitor_new();
    
    printf("Performance Monitor Configuration:\n");
    printf("  Regression threshold: %.1f%%\n", monitor->regression_threshold);
    printf("  Continuous monitoring: %s\n", monitor->continuous_monitoring ? "enabled" : "disabled");
    
    // Create multiple benchmark suites
    WynBenchmarkSuite* compute_suite = wyn_benchmark_suite_new("computation_benchmarks");
    WynBenchmarkSuite* memory_suite = wyn_benchmark_suite_new("memory_benchmarks");
    WynBenchmarkSuite* io_suite = wyn_benchmark_suite_new("io_benchmarks");
    
    // Add benchmarks to each suite
    WynBenchmarkConfig compute_config = wyn_create_benchmark_config("fibonacci", WYN_BENCH_COMPUTATION);
    WynBenchmarkConfig memory_config = wyn_create_benchmark_config("allocation", WYN_BENCH_MEMORY_ALLOCATION);
    WynBenchmarkConfig io_config = wyn_create_benchmark_config("file_ops", WYN_BENCH_IO_OPERATIONS);
    
    // Reduce iterations for demo
    compute_config.iterations = 50;
    memory_config.iterations = 50;
    io_config.iterations = 50;
    
    wyn_benchmark_suite_add_benchmark(compute_suite, &compute_config);
    wyn_benchmark_suite_add_benchmark(memory_suite, &memory_config);
    wyn_benchmark_suite_add_benchmark(io_suite, &io_config);
    
    // Add suites to monitor
    wyn_performance_monitor_add_suite(monitor, compute_suite);
    wyn_performance_monitor_add_suite(monitor, memory_suite);
    wyn_performance_monitor_add_suite(monitor, io_suite);
    
    printf("\nAdded %zu benchmark suites to monitor:\n", monitor->suite_count);
    for (size_t i = 0; i < monitor->suite_count; i++) {
        printf("  %zu. %s\n", i + 1, monitor->suites[i]->suite_name);
    }
    
    // Run all benchmarks
    printf("\nRunning all benchmark suites...\n");
    wyn_performance_monitor_run_all(monitor);
    
    printf("Monitoring completed at timestamp: %llu\n", 
           (unsigned long long)monitor->last_run_timestamp);
    
    // Show summary
    size_t total_results = 0;
    for (size_t i = 0; i < monitor->suite_count; i++) {
        total_results += monitor->suites[i]->result_count;
    }
    printf("Total benchmark results collected: %zu\n", total_results);
    
    wyn_benchmark_config_free(&compute_config);
    wyn_benchmark_config_free(&memory_config);
    wyn_benchmark_config_free(&io_config);
    wyn_performance_monitor_free(monitor);
}

// Example: Individual benchmark analysis
void example_individual_benchmark_analysis() {
    printf("\n=== Individual Benchmark Analysis Example ===\n");
    
    WynPerformanceMetrics metrics;
    
    // Test different benchmark types
    struct {
        const char* name;
        WynBenchmarkType type;
        bool (*run_func)(const char*, WynLanguage, WynPerformanceMetrics*);
    } benchmark_types[] = {
        {"Computation", WYN_BENCH_COMPUTATION, wyn_run_computation_benchmark},
        {"Memory", WYN_BENCH_MEMORY_ALLOCATION, wyn_run_memory_benchmark},
        {"I/O", WYN_BENCH_IO_OPERATIONS, wyn_run_io_benchmark},
        {"String", WYN_BENCH_STRING_PROCESSING, wyn_run_string_benchmark},
        {"Compilation", WYN_BENCH_COMPILATION_SPEED, wyn_run_compilation_benchmark}
    };
    
    printf("Running individual benchmarks for Wyn:\n");
    
    for (size_t i = 0; i < 5; i++) {
        if (benchmark_types[i].run_func("test", WYN_LANG_WYN, &metrics)) {
            printf("  %s Benchmark:\n", benchmark_types[i].name);
            printf("    Execution time: %.3f ms\n", metrics.execution_time_ms);
            printf("    Memory usage: %.2f MB\n", metrics.memory_usage_bytes / (1024.0 * 1024.0));
            printf("    CPU usage: %.1f%%\n", metrics.cpu_usage_percent);
            
            if (metrics.throughput_ops_per_sec > 0) {
                printf("    Throughput: %.0f ops/sec\n", metrics.throughput_ops_per_sec);
            }
            printf("\n");
        }
    }
}

// Example: Standard benchmark showcase
void example_standard_benchmarks() {
    printf("=== Standard Benchmark Showcase Example ===\n");
    
    WynStandardBenchmark* benchmarks[] = {
        &wyn_fibonacci_benchmark,
        &wyn_quicksort_benchmark,
        &wyn_matrix_multiply_benchmark,
        &wyn_json_parsing_benchmark,
        &wyn_http_server_benchmark,
        &wyn_file_io_benchmark
    };
    
    printf("Available standard benchmarks:\n");
    
    for (size_t i = 0; i < 6; i++) {
        printf("  %zu. %s\n", i + 1, benchmarks[i]->name);
        printf("     Implementation status:\n");
        printf("       Wyn: %s\n", benchmarks[i]->run_wyn ? "✓" : "✗");
        printf("       C: %s\n", benchmarks[i]->run_c ? "✓" : "✗");
        printf("       Rust: %s\n", benchmarks[i]->run_rust ? "✓" : "✗");
        printf("       Go: %s\n", benchmarks[i]->run_go ? "✓" : "✗");
        printf("\n");
    }
    
    printf("Note: Standard benchmarks provide consistent cross-language comparisons\n");
    printf("for common algorithms and use cases.\n");
}

// Example: Relative performance analysis
void example_relative_performance_analysis() {
    printf("\n=== Relative Performance Analysis Example ===\n");
    
    // Simulate benchmark results for analysis
    WynBenchmarkResult results[] = {
        {
            .benchmark_name = "fibonacci",
            .language = WYN_LANG_C,
            .metrics = {.execution_time_ms = 100.0}, // Baseline
            .success = true
        },
        {
            .benchmark_name = "fibonacci", 
            .language = WYN_LANG_WYN,
            .metrics = {.execution_time_ms = 110.0}, // 10% slower
            .success = true
        },
        {
            .benchmark_name = "fibonacci",
            .language = WYN_LANG_RUST,
            .metrics = {.execution_time_ms = 95.0}, // 5% faster
            .success = true
        },
        {
            .benchmark_name = "fibonacci",
            .language = WYN_LANG_GO,
            .metrics = {.execution_time_ms = 130.0}, // 30% slower
            .success = true
        }
    };
    
    printf("Relative performance analysis (C as baseline):\n");
    
    WynBenchmarkResult* baseline = &results[0]; // C baseline
    
    for (size_t i = 1; i < 4; i++) {
        double relative = wyn_calculate_relative_performance(baseline, &results[i]);
        double percent_diff = (1.0 - relative) * 100.0;
        
        printf("  %s: %.2fx relative performance", 
               wyn_language_name(results[i].language), relative);
        
        if (percent_diff > 0) {
            printf(" (%.1f%% slower)\n", percent_diff);
        } else {
            printf(" (%.1f%% faster)\n", -percent_diff);
        }
    }
    
    printf("\nPerformance ranking (fastest to slowest):\n");
    printf("  1. Rust (0.95x - fastest)\n");
    printf("  2. C (1.00x - baseline)\n");
    printf("  3. Wyn (1.10x)\n");
    printf("  4. Go (1.30x)\n");
}

int main() {
    printf("Wyn Benchmarking and Monitoring Examples\n");
    printf("========================================\n\n");
    
    example_basic_benchmark_suite();
    example_language_comparison();
    example_regression_detection();
    example_performance_monitoring();
    example_individual_benchmark_analysis();
    example_standard_benchmarks();
    example_relative_performance_analysis();
    
    printf("\n✓ All benchmarking and monitoring examples completed!\n");
    return 0;
}
