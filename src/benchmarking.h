#ifndef WYN_BENCHMARKING_H
#define WYN_BENCHMARKING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynBenchmarkSuite WynBenchmarkSuite;
typedef struct WynBenchmarkResult WynBenchmarkResult;
typedef struct WynPerformanceMonitor WynPerformanceMonitor;
typedef struct WynRegressionDetector WynRegressionDetector;

// Benchmark types
typedef enum {
    WYN_BENCH_COMPUTATION,
    WYN_BENCH_MEMORY_ALLOCATION,
    WYN_BENCH_IO_OPERATIONS,
    WYN_BENCH_STRING_PROCESSING,
    WYN_BENCH_COMPILATION_SPEED,
    WYN_BENCH_STARTUP_TIME,
    WYN_BENCH_THROUGHPUT,
    WYN_BENCH_LATENCY
} WynBenchmarkType;

// Comparison languages
typedef enum {
    WYN_LANG_WYN,
    WYN_LANG_C,
    WYN_LANG_RUST,
    WYN_LANG_GO,
    WYN_LANG_CPP,
    WYN_LANG_JAVA,
    WYN_LANG_PYTHON
} WynLanguage;

// Performance metrics
typedef struct {
    double execution_time_ms;
    size_t memory_usage_bytes;
    size_t peak_memory_bytes;
    double cpu_usage_percent;
    size_t cache_misses;
    size_t page_faults;
    double throughput_ops_per_sec;
    double latency_ms;
} WynPerformanceMetrics;

// Benchmark configuration
typedef struct {
    char* name;
    WynBenchmarkType type;
    size_t iterations;
    double timeout_seconds;
    bool measure_memory;
    bool measure_cpu;
    bool measure_cache;
    bool warmup_enabled;
    size_t warmup_iterations;
} WynBenchmarkConfig;

// Individual benchmark result
typedef struct WynBenchmarkResult {
    char* benchmark_name;
    WynLanguage language;
    WynPerformanceMetrics metrics;
    bool success;
    char* error_message;
    uint64_t timestamp;
    double relative_performance; // Compared to baseline
} WynBenchmarkResult;

// Benchmark suite
typedef struct WynBenchmarkSuite {
    char* suite_name;
    WynBenchmarkConfig* benchmarks;
    size_t benchmark_count;
    WynBenchmarkResult* results;
    size_t result_count;
    WynLanguage baseline_language;
    bool is_running;
} WynBenchmarkSuite;

// Performance monitoring
typedef struct WynPerformanceMonitor {
    WynBenchmarkSuite** suites;
    size_t suite_count;
    char* baseline_file;
    double regression_threshold;
    bool continuous_monitoring;
    uint64_t last_run_timestamp;
} WynPerformanceMonitor;

// Regression detection
typedef struct {
    char* benchmark_name;
    double baseline_time;
    double current_time;
    double regression_percent;
    bool is_regression;
    bool is_improvement;
} WynRegressionResult;

typedef struct WynRegressionDetector {
    WynRegressionResult* results;
    size_t result_count;
    double regression_threshold;
    double improvement_threshold;
    char* baseline_data_file;
} WynRegressionDetector;

// Benchmark suite functions
WynBenchmarkSuite* wyn_benchmark_suite_new(const char* name);
void wyn_benchmark_suite_free(WynBenchmarkSuite* suite);
bool wyn_benchmark_suite_add_benchmark(WynBenchmarkSuite* suite, const WynBenchmarkConfig* config);
bool wyn_benchmark_suite_run(WynBenchmarkSuite* suite);
bool wyn_benchmark_suite_run_language(WynBenchmarkSuite* suite, WynLanguage language);
WynBenchmarkResult* wyn_benchmark_suite_get_results(WynBenchmarkSuite* suite, size_t* count);

// Individual benchmark functions
bool wyn_run_computation_benchmark(const char* name, WynLanguage language, WynPerformanceMetrics* metrics);
bool wyn_run_memory_benchmark(const char* name, WynLanguage language, WynPerformanceMetrics* metrics);
bool wyn_run_io_benchmark(const char* name, WynLanguage language, WynPerformanceMetrics* metrics);
bool wyn_run_string_benchmark(const char* name, WynLanguage language, WynPerformanceMetrics* metrics);
bool wyn_run_compilation_benchmark(const char* name, WynLanguage language, WynPerformanceMetrics* metrics);

// Performance monitoring
WynPerformanceMonitor* wyn_performance_monitor_new(void);
void wyn_performance_monitor_free(WynPerformanceMonitor* monitor);
bool wyn_performance_monitor_add_suite(WynPerformanceMonitor* monitor, WynBenchmarkSuite* suite);
bool wyn_performance_monitor_run_all(WynPerformanceMonitor* monitor);
bool wyn_performance_monitor_save_baseline(WynPerformanceMonitor* monitor, const char* filename);
bool wyn_performance_monitor_load_baseline(WynPerformanceMonitor* monitor, const char* filename);

// Regression detection
WynRegressionDetector* wyn_regression_detector_new(double regression_threshold);
void wyn_regression_detector_free(WynRegressionDetector* detector);
bool wyn_regression_detector_analyze(WynRegressionDetector* detector, 
                                   const WynBenchmarkResult* baseline,
                                   const WynBenchmarkResult* current);
WynRegressionResult* wyn_regression_detector_get_results(WynRegressionDetector* detector, size_t* count);
bool wyn_regression_detector_has_regressions(WynRegressionDetector* detector);

// Comparison and analysis
double wyn_calculate_relative_performance(const WynBenchmarkResult* baseline, const WynBenchmarkResult* current);
bool wyn_compare_languages(WynBenchmarkSuite* suite, WynLanguage lang1, WynLanguage lang2, double* speedup);
bool wyn_generate_performance_report(WynBenchmarkSuite* suite, const char* output_file);
bool wyn_generate_comparison_chart(WynBenchmarkSuite* suite, const char* output_file);

// Memory profiling
typedef struct {
    size_t total_allocations;
    size_t total_deallocations;
    size_t peak_memory_usage;
    size_t current_memory_usage;
    double fragmentation_ratio;
    size_t allocation_failures;
} WynMemoryProfile;

WynMemoryProfile* wyn_profile_memory_usage(void* program, double duration_seconds);
bool wyn_detect_memory_leaks(void* program);
bool wyn_analyze_memory_fragmentation(WynMemoryProfile* profile);
void wyn_memory_profile_free(WynMemoryProfile* profile);

// Compilation speed benchmarking
typedef struct {
    double parse_time_ms;
    double typecheck_time_ms;
    double codegen_time_ms;
    double link_time_ms;
    double total_time_ms;
    size_t lines_of_code;
    double lines_per_second;
} WynCompilationMetrics;

WynCompilationMetrics* wyn_benchmark_compilation_speed(const char* source_file);
bool wyn_benchmark_incremental_compilation(const char* project_dir, WynCompilationMetrics* metrics);
bool wyn_compare_compilation_speeds(WynLanguage lang1, WynLanguage lang2, const char* test_project);
void wyn_compilation_metrics_free(WynCompilationMetrics* metrics);

// CI/CD integration
typedef struct {
    char* ci_system; // "github", "gitlab", "jenkins", etc.
    char* webhook_url;
    double max_regression_percent;
    bool fail_on_regression;
    bool post_results_to_pr;
} WynCIConfig;

bool wyn_setup_ci_monitoring(const WynCIConfig* config);
bool wyn_run_ci_benchmarks(const char* config_file);
bool wyn_post_results_to_ci(const WynBenchmarkSuite* suite, const WynCIConfig* config);

// Standard benchmark implementations
typedef struct {
    char* name;
    bool (*run_wyn)(WynPerformanceMetrics* metrics);
    bool (*run_c)(WynPerformanceMetrics* metrics);
    bool (*run_rust)(WynPerformanceMetrics* metrics);
    bool (*run_go)(WynPerformanceMetrics* metrics);
} WynStandardBenchmark;

// Standard benchmarks
extern WynStandardBenchmark wyn_fibonacci_benchmark;
extern WynStandardBenchmark wyn_quicksort_benchmark;
extern WynStandardBenchmark wyn_matrix_multiply_benchmark;
extern WynStandardBenchmark wyn_json_parsing_benchmark;
extern WynStandardBenchmark wyn_http_server_benchmark;
extern WynStandardBenchmark wyn_file_io_benchmark;

// Benchmark utilities
WynBenchmarkConfig wyn_create_benchmark_config(const char* name, WynBenchmarkType type);
void wyn_benchmark_config_free(WynBenchmarkConfig* config);
const char* wyn_language_name(WynLanguage language);
const char* wyn_benchmark_type_name(WynBenchmarkType type);

// Performance analysis
typedef struct {
    char* bottleneck_function;
    double time_percentage;
    char* optimization_suggestion;
} WynPerformanceBottleneck;

WynPerformanceBottleneck* wyn_analyze_performance_bottlenecks(const WynPerformanceMetrics* metrics, size_t* count);
bool wyn_suggest_optimizations(const WynBenchmarkResult* result, char** suggestions, size_t* suggestion_count);
void wyn_performance_bottleneck_free(WynPerformanceBottleneck* bottleneck);

// Continuous monitoring
bool wyn_start_continuous_monitoring(WynPerformanceMonitor* monitor, double interval_hours);
bool wyn_stop_continuous_monitoring(WynPerformanceMonitor* monitor);
bool wyn_schedule_benchmark_run(WynPerformanceMonitor* monitor, const char* cron_expression);

#endif // WYN_BENCHMARKING_H
