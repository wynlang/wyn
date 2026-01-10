#ifndef WYN_PERFORMANCE_H
#define WYN_PERFORMANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Forward declarations
typedef struct WynProfiler WynProfiler;
typedef struct WynBenchmark WynBenchmark;
typedef struct WynOptimizer WynOptimizer;

// Performance measurement types
typedef enum {
    WYN_PERF_TIME,
    WYN_PERF_MEMORY,
    WYN_PERF_CPU,
    WYN_PERF_CACHE,
    WYN_PERF_INSTRUCTIONS,
    WYN_PERF_BRANCHES
} WynPerfMetricType;

// Optimization levels
typedef enum {
    WYN_OPT_NONE,
    WYN_OPT_DEBUG,
    WYN_OPT_SIZE,
    WYN_OPT_SPEED,
    WYN_OPT_AGGRESSIVE
} WynOptLevel;

// Performance metric
typedef struct {
    WynPerfMetricType type;
    char* name;
    double value;
    char* unit;
    uint64_t timestamp;
} WynPerfMetric;

// Performance sample
typedef struct {
    char* name;
    WynPerfMetric* metrics;
    size_t metric_count;
    double duration_ms;
    uint64_t start_time;
    uint64_t end_time;
} WynPerfSample;

// Benchmark configuration
typedef struct {
    char* name;
    size_t iterations;
    double warmup_time_ms;
    double max_time_ms;
    bool measure_memory;
    bool measure_cpu;
    bool measure_cache;
} WynBenchmarkConfig;

// Benchmark result
typedef struct {
    char* name;
    WynPerfSample* samples;
    size_t sample_count;
    double mean_time_ms;
    double median_time_ms;
    double min_time_ms;
    double max_time_ms;
    double std_dev_ms;
    size_t total_iterations;
} WynBenchmarkResult;

// Profiler
typedef struct WynProfiler {
    WynPerfSample* samples;
    size_t sample_count;
    size_t sample_capacity;
    bool enabled;
    bool collect_memory;
    bool collect_cpu;
    uint64_t start_time;
} WynProfiler;

// Benchmark suite
typedef struct WynBenchmark {
    WynBenchmarkConfig config;
    WynBenchmarkResult* results;
    size_t result_count;
    WynProfiler* profiler;
    bool running;
} WynBenchmark;

// Optimizer configuration
typedef struct {
    WynOptLevel level;
    bool enable_inlining;
    bool enable_loop_unrolling;
    bool enable_vectorization;
    bool enable_constant_folding;
    bool enable_dead_code_elimination;
    bool enable_escape_analysis;
    bool profile_guided;
    char* profile_data_path;
} WynOptimizerConfig;

// Optimizer
typedef struct WynOptimizer {
    WynOptimizerConfig config;
    WynProfiler* profiler;
    bool initialized;
} WynOptimizer;

// Profiler functions
WynProfiler* wyn_profiler_new(void);
void wyn_profiler_free(WynProfiler* profiler);
bool wyn_profiler_start(WynProfiler* profiler);
bool wyn_profiler_stop(WynProfiler* profiler);
bool wyn_profiler_sample_start(WynProfiler* profiler, const char* name);
bool wyn_profiler_sample_end(WynProfiler* profiler, const char* name);
WynPerfSample* wyn_profiler_get_samples(WynProfiler* profiler, size_t* count);
bool wyn_profiler_save_report(WynProfiler* profiler, const char* filename);

// Benchmark functions
WynBenchmark* wyn_benchmark_new(const char* name);
void wyn_benchmark_free(WynBenchmark* benchmark);
bool wyn_benchmark_configure(WynBenchmark* benchmark, const WynBenchmarkConfig* config);
bool wyn_benchmark_run(WynBenchmark* benchmark, void (*test_func)(void*), void* data);
WynBenchmarkResult* wyn_benchmark_get_result(WynBenchmark* benchmark, const char* name);
bool wyn_benchmark_save_results(WynBenchmark* benchmark, const char* filename);

// Performance measurement
uint64_t wyn_perf_get_time_ns(void);
uint64_t wyn_perf_get_cpu_cycles(void);
size_t wyn_perf_get_memory_usage(void);
size_t wyn_perf_get_peak_memory(void);
double wyn_perf_get_cpu_usage(void);

// Optimization functions
WynOptimizer* wyn_optimizer_new(void);
void wyn_optimizer_free(WynOptimizer* optimizer);
bool wyn_optimizer_configure(WynOptimizer* optimizer, const WynOptimizerConfig* config);
bool wyn_optimizer_optimize_function(WynOptimizer* optimizer, void* function_ir);
bool wyn_optimizer_optimize_module(WynOptimizer* optimizer, void* module_ir);

// Benchmark utilities
WynBenchmarkConfig wyn_benchmark_config_default(const char* name);
void wyn_benchmark_config_free(WynBenchmarkConfig* config);
double wyn_benchmark_calculate_mean(const WynBenchmarkResult* result);
double wyn_benchmark_calculate_median(const WynBenchmarkResult* result);
double wyn_benchmark_calculate_std_dev(const WynBenchmarkResult* result);

// Performance analysis
typedef struct {
    char* hotspot_function;
    double cpu_percentage;
    size_t call_count;
    double avg_time_ms;
} WynHotspot;

WynHotspot* wyn_analyze_hotspots(WynProfiler* profiler, size_t* count);
bool wyn_generate_flame_graph(WynProfiler* profiler, const char* output_path);
bool wyn_compare_benchmarks(const WynBenchmarkResult* baseline, 
                           const WynBenchmarkResult* current, 
                           double* improvement_ratio);

// Memory profiling
typedef struct {
    size_t total_allocated;
    size_t peak_usage;
    size_t current_usage;
    size_t allocation_count;
    size_t deallocation_count;
    double fragmentation_ratio;
} WynMemoryProfile;

WynMemoryProfile* wyn_profile_memory(WynProfiler* profiler);
bool wyn_detect_memory_leaks(WynProfiler* profiler);
size_t wyn_get_allocation_size(void* ptr);

// Compiler optimization integration
bool wyn_enable_profile_guided_optimization(WynOptimizer* optimizer, const char* profile_data);
bool wyn_optimize_for_size(WynOptimizer* optimizer);
bool wyn_optimize_for_speed(WynOptimizer* optimizer);
bool wyn_enable_link_time_optimization(WynOptimizer* optimizer);

// Performance regression detection
typedef struct {
    char* test_name;
    double baseline_time_ms;
    double current_time_ms;
    double regression_percentage;
    bool is_regression;
} WynRegressionResult;

WynRegressionResult* wyn_detect_regressions(const WynBenchmarkResult* baseline,
                                           const WynBenchmarkResult* current,
                                           double threshold_percentage);

// Continuous performance monitoring
typedef struct {
    WynBenchmark** benchmarks;
    size_t benchmark_count;
    char* baseline_file;
    double regression_threshold;
    bool auto_save_results;
} WynPerfMonitor;

WynPerfMonitor* wyn_perf_monitor_new(void);
void wyn_perf_monitor_free(WynPerfMonitor* monitor);
bool wyn_perf_monitor_add_benchmark(WynPerfMonitor* monitor, WynBenchmark* benchmark);
bool wyn_perf_monitor_run_all(WynPerfMonitor* monitor);
bool wyn_perf_monitor_check_regressions(WynPerfMonitor* monitor);

#endif // WYN_PERFORMANCE_H
