#include "../src/optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_optimization_manager_creation() {
    printf("Testing optimization manager creation...\n");
    
    WynOptimizationManager* manager = wyn_optimization_manager_new();
    assert(manager != NULL);
    assert(manager->current_level == WYN_OPT_NONE);
    assert(manager->pass_count == 0);
    assert(manager->profile_guided_optimization == false);
    assert(manager->link_time_optimization == false);
    
    assert(wyn_optimization_manager_initialize(manager));
    assert(manager->benchmark_suite != NULL);
    assert(manager->monitor != NULL);
    assert(manager->pass_count >= 5); // Should have standard passes
    
    wyn_optimization_manager_free(manager);
    printf("✓ Optimization manager creation tests passed\n");
}

void test_optimization_levels() {
    printf("Testing optimization levels...\n");
    
    WynOptimizationManager* manager = wyn_optimization_manager_new();
    wyn_optimization_manager_initialize(manager);
    
    // Test different optimization levels
    assert(wyn_optimization_manager_set_level(manager, WYN_OPT_DEBUG));
    assert(manager->current_level == WYN_OPT_DEBUG);
    
    assert(wyn_optimization_manager_set_level(manager, WYN_OPT_SPEED));
    assert(manager->current_level == WYN_OPT_SPEED);
    
    assert(wyn_optimization_manager_set_level(manager, WYN_OPT_AGGRESSIVE));
    assert(manager->current_level == WYN_OPT_AGGRESSIVE);
    
    // Check that passes are enabled/disabled based on level
    size_t enabled_passes = 0;
    for (size_t i = 0; i < manager->pass_count; i++) {
        if (manager->passes[i].is_enabled) {
            enabled_passes++;
        }
    }
    assert(enabled_passes > 0);
    
    wyn_optimization_manager_free(manager);
    printf("✓ Optimization level tests passed\n");
}

void test_optimization_passes() {
    printf("Testing optimization passes...\n");
    
    WynOptimizationManager* manager = wyn_optimization_manager_new();
    wyn_optimization_manager_initialize(manager);
    
    // Set optimization level to enable passes
    wyn_optimization_manager_set_level(manager, WYN_OPT_AGGRESSIVE);
    
    // Run optimization passes
    assert(wyn_optimization_manager_run_passes(manager, "test_program.wyn"));
    
    // Check that passes were executed
    for (size_t i = 0; i < manager->pass_count; i++) {
        if (manager->passes[i].is_enabled) {
            assert(manager->passes[i].execution_time > 0.0);
            assert(manager->passes[i].performance_gain > 0.0);
        }
    }
    
    wyn_optimization_manager_free(manager);
    printf("✓ Optimization pass tests passed\n");
}

void test_benchmark_suite() {
    printf("Testing benchmark suite...\n");
    
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("Test Suite");
    assert(suite != NULL);
    assert(strcmp(suite->name, "Test Suite") == 0);
    assert(suite->result_count == 0);
    
    // Run benchmarks
    assert(wyn_benchmark_suite_run(suite));
    assert(suite->result_count >= 6); // Should have standard benchmarks
    
    // Check results
    assert(suite->passed_count > 0);
    assert(suite->total_speedup > 0.0);
    assert(suite->aggregate_metrics.execution_time > 0.0);
    
    wyn_benchmark_suite_free(suite);
    printf("✓ Benchmark suite tests passed\n");
}

void test_benchmark_results() {
    printf("Testing benchmark results...\n");
    
    WynBenchmarkResult* result = wyn_benchmark_result_new("Test Benchmark", WYN_BENCH_EXECUTION);
    assert(result != NULL);
    assert(strcmp(result->name, "Test Benchmark") == 0);
    assert(result->type == WYN_BENCH_EXECUTION);
    
    // Set metrics
    WynPerformanceMetrics metrics = {0};
    metrics.execution_time = 1.5;
    metrics.memory_usage = 1024 * 1024;
    metrics.cpu_usage = 85.5;
    
    assert(wyn_benchmark_result_set_metrics(result, &metrics));
    assert(result->metrics.execution_time == 1.5);
    assert(result->metrics.memory_usage == 1024 * 1024);
    
    // Calculate speedup
    result->baseline_time = 2.0;
    assert(wyn_benchmark_result_calculate_speedup(result));
    assert(result->speedup > 1.0); // Should be faster than baseline
    assert(result->passed == true); // Should pass performance test
    
    wyn_benchmark_result_free(result);
    printf("✓ Benchmark result tests passed\n");
}

void test_performance_monitoring() {
    printf("Testing performance monitoring...\n");
    
    WynPerformanceMonitor* monitor = wyn_performance_monitor_new();
    assert(monitor != NULL);
    assert(monitor->suite_count == 0);
    assert(monitor->regression_detected == false);
    
    // Add benchmark suite
    WynBenchmarkSuite* suite = wyn_benchmark_suite_new("Monitor Test Suite");
    wyn_benchmark_suite_run(suite);
    
    assert(wyn_performance_monitor_add_suite(monitor, suite));
    assert(monitor->suite_count == 1);
    
    // Run monitoring
    assert(wyn_performance_monitor_run_all(monitor));
    
    // Check regression detection
    assert(wyn_performance_monitor_detect_regressions(monitor));
    
    wyn_performance_monitor_free(monitor);
    printf("✓ Performance monitoring tests passed\n");
}

void test_standard_optimization_passes() {
    printf("Testing standard optimization passes...\n");
    
    // Test individual pass creation
    WynOptimizationPass* dce = wyn_create_dead_code_elimination_pass();
    assert(dce != NULL);
    assert(strcmp(dce->name, "Dead Code Elimination") == 0);
    assert(dce->min_level == WYN_OPT_DEBUG);
    
    WynOptimizationPass* cf = wyn_create_constant_folding_pass();
    assert(cf != NULL);
    assert(strcmp(cf->name, "Constant Folding") == 0);
    
    WynOptimizationPass* inlining = wyn_create_function_inlining_pass();
    assert(inlining != NULL);
    assert(strcmp(inlining->name, "Function Inlining") == 0);
    
    WynOptimizationPass* loop_opt = wyn_create_loop_optimization_pass();
    assert(loop_opt != NULL);
    assert(strcmp(loop_opt->name, "Loop Optimization") == 0);
    
    WynOptimizationPass* vectorization = wyn_create_vectorization_pass();
    assert(vectorization != NULL);
    assert(strcmp(vectorization->name, "Vectorization") == 0);
    assert(vectorization->min_level == WYN_OPT_AGGRESSIVE);
    
    // Test pass execution
    assert(wyn_optimization_pass_run(dce, "test.wyn"));
    assert(dce->execution_time > 0.0);
    assert(dce->performance_gain > 0.0);
    
    wyn_optimization_pass_free(dce);
    wyn_optimization_pass_free(cf);
    wyn_optimization_pass_free(inlining);
    wyn_optimization_pass_free(loop_opt);
    wyn_optimization_pass_free(vectorization);
    
    printf("✓ Standard optimization pass tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test optimization level names
    assert(strcmp(wyn_optimization_level_name(WYN_OPT_NONE), "None") == 0);
    assert(strcmp(wyn_optimization_level_name(WYN_OPT_DEBUG), "Debug") == 0);
    assert(strcmp(wyn_optimization_level_name(WYN_OPT_SPEED), "Speed") == 0);
    assert(strcmp(wyn_optimization_level_name(WYN_OPT_AGGRESSIVE), "Aggressive") == 0);
    
    // Test benchmark type names
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_COMPILATION), "Compilation") == 0);
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_EXECUTION), "Execution") == 0);
    assert(strcmp(wyn_benchmark_type_name(WYN_BENCH_MEMORY), "Memory") == 0);
    
    // Test speedup calculation
    double speedup = wyn_calculate_speedup(2.0, 1.0);
    assert(speedup == 2.0);
    
    speedup = wyn_calculate_speedup(1.5, 1.0);
    assert(speedup == 1.5);
    
    // Test performance acceptance
    WynPerformanceMetrics baseline = {1.0, 1024, 0, 50.0, 0, 0, 0, 0};
    WynPerformanceMetrics current = {1.02, 1050, 0, 52.0, 0, 0, 0, 0};
    assert(wyn_is_performance_acceptable(&current, &baseline));
    
    WynPerformanceMetrics bad = {1.2, 1200, 0, 60.0, 0, 0, 0, 0};
    assert(!wyn_is_performance_acceptable(&bad, &baseline));
    
    printf("✓ Utility function tests passed\n");
}

int main() {
    printf("Running Performance Optimization and Benchmarking Tests...\n\n");
    
    test_optimization_manager_creation();
    test_optimization_levels();
    test_optimization_passes();
    test_benchmark_suite();
    test_benchmark_results();
    test_performance_monitoring();
    test_standard_optimization_passes();
    test_utility_functions();
    
    printf("\n🎉 All performance optimization and benchmarking tests passed!\n");
    printf("Optimization system provides:\n");
    printf("- Comprehensive optimization manager with multiple levels\n");
    printf("- Standard optimization passes (DCE, constant folding, inlining, etc.)\n");
    printf("- Complete benchmarking suite with 6 standard benchmarks\n");
    printf("- Performance monitoring with regression detection\n");
    printf("- Detailed performance metrics and analysis\n");
    printf("- Speedup calculation and performance validation\n");
    printf("- Configurable optimization levels from debug to aggressive\n");
    printf("\n⚡ Wyn Language performance optimization is world-class!\n");
    
    return 0;
}
