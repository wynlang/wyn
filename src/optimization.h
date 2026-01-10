#ifndef WYN_OPTIMIZATION_H
#define WYN_OPTIMIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynOptimizationManager WynOptimizationManager;
typedef struct WynBenchmarkSuite WynBenchmarkSuite;
typedef struct WynPerformanceMonitor WynPerformanceMonitor;

// Optimization levels
typedef enum {
    WYN_OPT_NONE,
    WYN_OPT_DEBUG,
    WYN_OPT_SIZE,
    WYN_OPT_SPEED,
    WYN_OPT_AGGRESSIVE
} WynOptimizationLevel;

// Benchmark types
typedef enum {
    WYN_BENCH_COMPILATION,
    WYN_BENCH_EXECUTION,
    WYN_BENCH_MEMORY,
    WYN_BENCH_THROUGHPUT,
    WYN_BENCH_LATENCY,
    WYN_BENCH_STARTUP
} WynBenchmarkType;

// Performance metrics
typedef struct {
    double execution_time;
    size_t memory_usage;
    size_t peak_memory;
    double cpu_usage;
    size_t cache_misses;
    size_t instructions;
    double throughput;
    double latency;
} WynPerformanceMetrics;

// Benchmark result
typedef struct {
    char* name;
    WynBenchmarkType type;
    WynPerformanceMetrics metrics;
    double baseline_time;
    double current_time;
    double speedup;
    bool passed;
    char* notes;
} WynBenchmarkResult;

// Optimization pass
typedef struct {
    char* name;
    char* description;
    WynOptimizationLevel min_level;
    bool is_enabled;
    double execution_time;
    size_t code_size_before;
    size_t code_size_after;
    double performance_gain;
} WynOptimizationPass;

// Benchmark suite
typedef struct WynBenchmarkSuite {
    char* name;
    WynBenchmarkResult* results;
    size_t result_count;
    WynPerformanceMetrics aggregate_metrics;
    double total_speedup;
    size_t passed_count;
    size_t failed_count;
} WynBenchmarkSuite;

// Performance monitor
typedef struct WynPerformanceMonitor {
    WynBenchmarkSuite* suites;
    size_t suite_count;
    WynPerformanceMetrics* historical_data;
    size_t data_point_count;
    double performance_trend;
    bool regression_detected;
    char* regression_details;
} WynPerformanceMonitor;

// Optimization manager
typedef struct WynOptimizationManager {
    WynOptimizationLevel current_level;
    WynOptimizationPass* passes;
    size_t pass_count;
    WynBenchmarkSuite* benchmark_suite;
    WynPerformanceMonitor* monitor;
    char* target_architecture;
    bool profile_guided_optimization;
    bool link_time_optimization;
} WynOptimizationManager;

// Optimization manager functions
WynOptimizationManager* wyn_optimization_manager_new(void);
void wyn_optimization_manager_free(WynOptimizationManager* manager);
bool wyn_optimization_manager_initialize(WynOptimizationManager* manager);
bool wyn_optimization_manager_set_level(WynOptimizationManager* manager, WynOptimizationLevel level);
bool wyn_optimization_manager_run_passes(WynOptimizationManager* manager, const char* input_file);

// Optimization pass functions
WynOptimizationPass* wyn_optimization_pass_new(const char* name, const char* description, WynOptimizationLevel min_level);
void wyn_optimization_pass_free(WynOptimizationPass* pass);
bool wyn_optimization_pass_run(WynOptimizationPass* pass, const char* input_file);
bool wyn_optimization_manager_add_pass(WynOptimizationManager* manager, WynOptimizationPass* pass);

// Standard optimization passes
WynOptimizationPass* wyn_create_dead_code_elimination_pass(void);
WynOptimizationPass* wyn_create_constant_folding_pass(void);
WynOptimizationPass* wyn_create_function_inlining_pass(void);
WynOptimizationPass* wyn_create_loop_optimization_pass(void);
WynOptimizationPass* wyn_create_vectorization_pass(void);
WynOptimizationPass* wyn_create_memory_optimization_pass(void);
WynOptimizationPass* wyn_create_register_allocation_pass(void);
WynOptimizationPass* wyn_create_instruction_scheduling_pass(void);

// Benchmark suite functions
WynBenchmarkSuite* wyn_benchmark_suite_new(const char* name);
void wyn_benchmark_suite_free(WynBenchmarkSuite* suite);
bool wyn_benchmark_suite_add_result(WynBenchmarkSuite* suite, WynBenchmarkResult* result);
bool wyn_benchmark_suite_run(WynBenchmarkSuite* suite);
bool wyn_benchmark_suite_analyze(WynBenchmarkSuite* suite);

// Benchmark result functions
WynBenchmarkResult* wyn_benchmark_result_new(const char* name, WynBenchmarkType type);
void wyn_benchmark_result_free(WynBenchmarkResult* result);
bool wyn_benchmark_result_set_metrics(WynBenchmarkResult* result, const WynPerformanceMetrics* metrics);
bool wyn_benchmark_result_calculate_speedup(WynBenchmarkResult* result);

// Standard benchmarks
bool wyn_run_compilation_benchmark(WynBenchmarkSuite* suite);
bool wyn_run_execution_benchmark(WynBenchmarkSuite* suite);
bool wyn_run_memory_benchmark(WynBenchmarkSuite* suite);
bool wyn_run_throughput_benchmark(WynBenchmarkSuite* suite);
bool wyn_run_latency_benchmark(WynBenchmarkSuite* suite);
bool wyn_run_startup_benchmark(WynBenchmarkSuite* suite);

// Performance monitoring functions
WynPerformanceMonitor* wyn_performance_monitor_new(void);
void wyn_performance_monitor_free(WynPerformanceMonitor* monitor);
bool wyn_performance_monitor_add_suite(WynPerformanceMonitor* monitor, WynBenchmarkSuite* suite);
bool wyn_performance_monitor_run_all(WynPerformanceMonitor* monitor);
bool wyn_performance_monitor_detect_regressions(WynPerformanceMonitor* monitor);
bool wyn_performance_monitor_generate_report(WynPerformanceMonitor* monitor, const char* output_file);

// Performance analysis functions
typedef struct {
    char* function_name;
    double cpu_time;
    double wall_time;
    size_t call_count;
    size_t memory_allocated;
    double cache_hit_rate;
} WynProfileEntry;

typedef struct {
    WynProfileEntry* entries;
    size_t entry_count;
    double total_time;
    size_t total_memory;
    double overall_efficiency;
} WynProfileReport;

WynProfileReport* wyn_profile_application(const char* executable, const char* args);
void wyn_profile_report_free(WynProfileReport* report);
bool wyn_profile_report_export(WynProfileReport* report, const char* output_file);

// Memory optimization functions
typedef struct {
    size_t heap_size;
    size_t stack_size;
    size_t code_size;
    size_t data_size;
    size_t total_size;
    double fragmentation_ratio;
} WynMemoryProfile;

WynMemoryProfile* wyn_analyze_memory_usage(const char* executable);
void wyn_memory_profile_free(WynMemoryProfile* profile);
bool wyn_optimize_memory_layout(WynOptimizationManager* manager, const char* input_file);

// CPU optimization functions
typedef struct {
    char* instruction_set;
    bool supports_simd;
    bool supports_vectorization;
    size_t cache_line_size;
    size_t l1_cache_size;
    size_t l2_cache_size;
    size_t l3_cache_size;
} WynCPUProfile;

WynCPUProfile* wyn_detect_cpu_features(void);
void wyn_cpu_profile_free(WynCPUProfile* profile);
bool wyn_optimize_for_cpu(WynOptimizationManager* manager, WynCPUProfile* profile);

// Compiler optimization functions
bool wyn_enable_profile_guided_optimization(WynOptimizationManager* manager, const char* profile_data);
bool wyn_enable_link_time_optimization(WynOptimizationManager* manager);
bool wyn_optimize_for_size(WynOptimizationManager* manager);
bool wyn_optimize_for_speed(WynOptimizationManager* manager);

// Performance regression detection
typedef struct {
    char* test_name;
    double baseline_value;
    double current_value;
    double regression_threshold;
    bool is_regression;
    char* description;
} WynRegressionTest;

bool wyn_detect_performance_regression(WynPerformanceMonitor* monitor, double threshold);
WynRegressionTest* wyn_get_regression_tests(WynPerformanceMonitor* monitor, size_t* count);
bool wyn_fix_performance_regression(WynOptimizationManager* manager, WynRegressionTest* test);

// Continuous performance monitoring
typedef struct {
    uint64_t timestamp;
    WynPerformanceMetrics metrics;
    char* commit_hash;
    char* build_config;
} WynPerformanceSnapshot;

bool wyn_take_performance_snapshot(WynPerformanceMonitor* monitor, WynPerformanceSnapshot* snapshot);
bool wyn_compare_performance_snapshots(WynPerformanceSnapshot* baseline, WynPerformanceSnapshot* current, double* change_percent);
bool wyn_store_performance_history(WynPerformanceMonitor* monitor, WynPerformanceSnapshot* snapshot);

// Benchmark comparison functions
typedef struct {
    char* language_name;
    WynPerformanceMetrics metrics;
    double relative_performance;
} WynLanguageComparison;

bool wyn_compare_with_languages(WynBenchmarkSuite* suite, WynLanguageComparison** comparisons, size_t* count);
bool wyn_generate_comparison_report(WynLanguageComparison* comparisons, size_t count, const char* output_file);

// Utility functions
const char* wyn_optimization_level_name(WynOptimizationLevel level);
const char* wyn_benchmark_type_name(WynBenchmarkType type);
double wyn_calculate_speedup(double baseline, double current);
bool wyn_is_performance_acceptable(const WynPerformanceMetrics* metrics, const WynPerformanceMetrics* baseline);

// Advanced optimization techniques
bool wyn_apply_auto_vectorization(WynOptimizationManager* manager);
bool wyn_apply_loop_unrolling(WynOptimizationManager* manager);
bool wyn_apply_function_specialization(WynOptimizationManager* manager);
bool wyn_apply_constant_propagation(WynOptimizationManager* manager);
bool wyn_apply_dead_store_elimination(WynOptimizationManager* manager);

#endif // WYN_OPTIMIZATION_H
