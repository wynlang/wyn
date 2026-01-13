#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    char name[256];
    double execution_time;
    double cpu_time;
    long memory_peak;
    long memory_avg;
    int iterations;
    double throughput;
} BenchmarkResult;

typedef struct {
    BenchmarkResult* results;
    int count;
    int capacity;
} BenchmarkSuite;

BenchmarkSuite* create_benchmark_suite() {
    BenchmarkSuite* suite = malloc(sizeof(BenchmarkSuite));
    suite->capacity = 50;
    suite->results = malloc(sizeof(BenchmarkResult) * suite->capacity);
    suite->count = 0;
    return suite;
}

double get_time_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

long get_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss; // Peak memory in KB (Linux) or bytes (macOS)
}

int run_benchmark_executable(const char* executable, int iterations, BenchmarkResult* result) {
    strncpy(result->name, executable, sizeof(result->name) - 1);
    result->iterations = iterations;
    
    double total_time = 0;
    double total_cpu_time = 0;
    long total_memory = 0;
    long peak_memory = 0;
    
    for (int i = 0; i < iterations; i++) {
        double start_time = get_time_seconds();
        long start_memory = get_memory_usage();
        
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Failed to fork for benchmark\n");
            return 0;
        }
        
        if (pid == 0) {
            // Child process - run the benchmark
            execl(executable, executable, NULL);
            exit(1);
        }
        
        // Parent process - wait and measure
        int status;
        struct rusage child_usage;
        wait4(pid, &status, 0, &child_usage);
        
        double end_time = get_time_seconds();
        long end_memory = get_memory_usage();
        
        double iteration_time = end_time - start_time;
        double cpu_time = child_usage.ru_utime.tv_sec + child_usage.ru_utime.tv_usec / 1000000.0 +
                         child_usage.ru_stime.tv_sec + child_usage.ru_stime.tv_usec / 1000000.0;
        long memory_used = child_usage.ru_maxrss;
        
        total_time += iteration_time;
        total_cpu_time += cpu_time;
        total_memory += memory_used;
        
        if (memory_used > peak_memory) {
            peak_memory = memory_used;
        }
        
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Benchmark %s failed on iteration %d\n", executable, i + 1);
            return 0;
        }
    }
    
    result->execution_time = total_time / iterations;
    result->cpu_time = total_cpu_time / iterations;
    result->memory_peak = peak_memory;
    result->memory_avg = total_memory / iterations;
    result->throughput = iterations / total_time;
    
    return 1;
}

void add_benchmark_result(BenchmarkSuite* suite, BenchmarkResult* result) {
    if (suite->count >= suite->capacity) {
        suite->capacity *= 2;
        suite->results = realloc(suite->results, sizeof(BenchmarkResult) * suite->capacity);
    }
    
    suite->results[suite->count++] = *result;
}

void run_benchmark_from_wyn(const char* wyn_file, int iterations, BenchmarkSuite* suite) {
    // Compile the Wyn file
    char executable[256];
    snprintf(executable, sizeof(executable), "%s.bench", wyn_file);
    
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), "../wyn %s -o %s -O2", wyn_file, executable);
    
    int compile_result = system(compile_cmd);
    if (compile_result != 0) {
        fprintf(stderr, "Failed to compile %s for benchmarking\n", wyn_file);
        return;
    }
    
    // Run benchmark
    BenchmarkResult result;
    if (run_benchmark_executable(executable, iterations, &result)) {
        add_benchmark_result(suite, &result);
    }
    
    // Clean up
    unlink(executable);
}

void print_benchmark_results(BenchmarkSuite* suite) {
    printf("\n=== Performance Benchmark Results ===\n");
    printf("%-30s %10s %10s %12s %12s %10s\n", 
           "Benchmark", "Exec Time", "CPU Time", "Peak Mem", "Avg Mem", "Throughput");
    printf("%-30s %10s %10s %12s %12s %10s\n", 
           "----------", "---------", "--------", "--------", "-------", "----------");
    
    for (int i = 0; i < suite->count; i++) {
        BenchmarkResult* r = &suite->results[i];
        printf("%-30s %9.3fs %9.3fs %10ldKB %10ldKB %8.1f/s\n",
               r->name, r->execution_time, r->cpu_time, 
               r->memory_peak, r->memory_avg, r->throughput);
    }
    
    if (suite->count > 1) {
        printf("\n=== Performance Summary ===\n");
        
        // Find fastest and slowest
        int fastest = 0, slowest = 0;
        for (int i = 1; i < suite->count; i++) {
            if (suite->results[i].execution_time < suite->results[fastest].execution_time) {
                fastest = i;
            }
            if (suite->results[i].execution_time > suite->results[slowest].execution_time) {
                slowest = i;
            }
        }
        
        printf("Fastest: %s (%.3fs)\n", suite->results[fastest].name, suite->results[fastest].execution_time);
        printf("Slowest: %s (%.3fs)\n", suite->results[slowest].name, suite->results[slowest].execution_time);
        
        if (suite->results[slowest].execution_time > 0) {
            double speedup = suite->results[slowest].execution_time / suite->results[fastest].execution_time;
            printf("Speedup: %.2fx\n", speedup);
        }
    }
}

void save_benchmark_json(BenchmarkSuite* suite, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "{\n");
    fprintf(file, "  \"timestamp\": %ld,\n", time(NULL));
    fprintf(file, "  \"benchmarks\": [\n");
    
    for (int i = 0; i < suite->count; i++) {
        BenchmarkResult* r = &suite->results[i];
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"%s\",\n", r->name);
        fprintf(file, "      \"execution_time\": %.6f,\n", r->execution_time);
        fprintf(file, "      \"cpu_time\": %.6f,\n", r->cpu_time);
        fprintf(file, "      \"memory_peak\": %ld,\n", r->memory_peak);
        fprintf(file, "      \"memory_avg\": %ld,\n", r->memory_avg);
        fprintf(file, "      \"iterations\": %d,\n", r->iterations);
        fprintf(file, "      \"throughput\": %.2f\n", r->throughput);
        fprintf(file, "    }%s\n", i < suite->count - 1 ? "," : "");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    fclose(file);
}

int main(int argc, char* argv[]) {
    int iterations = 10;
    const char* output_file = "benchmark_results.json";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
    }
    
    BenchmarkSuite* suite = create_benchmark_suite();
    
    printf("Running performance benchmarks (%d iterations each)...\n", iterations);
    
    // Discover and run benchmark files
    const char* benchmark_files[] = {
        "fibonacci.wyn",
        "quicksort.wyn", 
        "matrix_multiply.wyn",
        "string_processing.wyn",
        "recursive_factorial.wyn",
        NULL
    };
    
    for (int i = 0; benchmark_files[i]; i++) {
        if (access(benchmark_files[i], F_OK) == 0) {
            printf("Running benchmark: %s\n", benchmark_files[i]);
            run_benchmark_from_wyn(benchmark_files[i], iterations, suite);
        }
    }
    
    if (suite->count == 0) {
        printf("No benchmark files found.\n");
        printf("Expected files: fibonacci.wyn, quicksort.wyn, matrix_multiply.wyn, etc.\n");
        return 1;
    }
    
    print_benchmark_results(suite);
    save_benchmark_json(suite, output_file);
    
    printf("\nBenchmark results saved to: %s\n", output_file);
    
    free(suite->results);
    free(suite);
    
    return 0;
}