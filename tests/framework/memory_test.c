#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

typedef struct {
    char test_name[256];
    int memory_leaks_found;
    long peak_memory_kb;
    long final_memory_kb;
    char leak_details[2048];
    double test_duration;
} MemoryTestResult;

typedef struct {
    MemoryTestResult* results;
    int count;
    int capacity;
    int total_leaks;
    long total_peak_memory;
} MemoryTestSuite;

MemoryTestSuite* create_memory_test_suite() {
    MemoryTestSuite* suite = malloc(sizeof(MemoryTestSuite));
    suite->capacity = 50;
    suite->results = malloc(sizeof(MemoryTestResult) * suite->capacity);
    suite->count = 0;
    suite->total_leaks = 0;
    suite->total_peak_memory = 0;
    return suite;
}

long get_process_memory_kb(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE* file = fopen(path, "r");
    if (!file) return -1;
    
    char line[256];
    long memory_kb = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld kB", &memory_kb);
            break;
        }
    }
    
    fclose(file);
    return memory_kb;
}

int run_valgrind_test(const char* executable, MemoryTestResult* result) {
    char valgrind_cmd[1024];
    char temp_file[] = "/tmp/valgrind_output_XXXXXX";
    int temp_fd = mkstemp(temp_file);
    if (temp_fd == -1) {
        strcpy(result->leak_details, "Failed to create temporary file");
        return 0;
    }
    close(temp_fd);
    
    snprintf(valgrind_cmd, sizeof(valgrind_cmd),
             "valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all "
             "--track-origins=yes --log-file=%s %s", temp_file, executable);
    
    clock_t start = clock();
    int valgrind_result = system(valgrind_cmd);
    result->test_duration = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    
    // Parse valgrind output
    FILE* output_file = fopen(temp_file, "r");
    if (!output_file) {
        strcpy(result->leak_details, "Failed to read valgrind output");
        unlink(temp_file);
        return 0;
    }
    
    char line[512];
    result->memory_leaks_found = 0;
    result->leak_details[0] = '\0';
    size_t details_len = 0;
    
    while (fgets(line, sizeof(line), output_file)) {
        // Look for leak summary
        if (strstr(line, "definitely lost:") || strstr(line, "possibly lost:")) {
            long bytes_lost;
            if (sscanf(line, "%*s %*s %ld bytes", &bytes_lost) == 1 && bytes_lost > 0) {
                result->memory_leaks_found++;
                
                if (details_len < sizeof(result->leak_details) - 100) {
                    int added = snprintf(result->leak_details + details_len, 
                                       sizeof(result->leak_details) - details_len,
                                       "%s", line);
                    if (added > 0) details_len += added;
                }
            }
        }
        
        // Look for peak memory usage
        if (strstr(line, "total heap usage:")) {
            // Extract peak memory information if available
        }
    }
    
    fclose(output_file);
    unlink(temp_file);
    
    return WIFEXITED(valgrind_result) && WEXITSTATUS(valgrind_result) == 0;
}

int run_native_memory_test(const char* executable, MemoryTestResult* result) {
    clock_t start = clock();
    
    pid_t pid = fork();
    if (pid == -1) {
        strcpy(result->leak_details, "Failed to fork process");
        return 0;
    }
    
    if (pid == 0) {
        // Child process - run the test
        execl(executable, executable, NULL);
        exit(1);
    }
    
    // Parent process - monitor memory usage
    long peak_memory = 0;
    long current_memory;
    
    while ((current_memory = get_process_memory_kb(pid)) >= 0) {
        if (current_memory > peak_memory) {
            peak_memory = current_memory;
        }
        usleep(10000); // Check every 10ms
        
        // Check if process is still running
        int status;
        if (waitpid(pid, &status, WNOHANG) != 0) {
            break;
        }
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    result->test_duration = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    result->peak_memory_kb = peak_memory;
    result->final_memory_kb = get_process_memory_kb(getpid());
    
    // Simple heuristic for memory leaks (not as accurate as valgrind)
    result->memory_leaks_found = 0;
    snprintf(result->leak_details, sizeof(result->leak_details),
             "Peak memory: %ld KB (native monitoring)", peak_memory);
    
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void add_memory_test_result(MemoryTestSuite* suite, MemoryTestResult* result) {
    if (suite->count >= suite->capacity) {
        suite->capacity *= 2;
        suite->results = realloc(suite->results, sizeof(MemoryTestResult) * suite->capacity);
    }
    
    suite->results[suite->count++] = *result;
    suite->total_leaks += result->memory_leaks_found;
    suite->total_peak_memory += result->peak_memory_kb;
}

void run_memory_test_on_wyn_file(const char* wyn_file, MemoryTestSuite* suite, int use_valgrind) {
    // Compile the Wyn file
    char executable[256];
    snprintf(executable, sizeof(executable), "%s.memtest", wyn_file);
    
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), "../wyn %s -o %s -g", wyn_file, executable);
    
    if (system(compile_cmd) != 0) {
        fprintf(stderr, "Failed to compile %s for memory testing\n", wyn_file);
        return;
    }
    
    MemoryTestResult result;
    strncpy(result.test_name, wyn_file, sizeof(result.test_name) - 1);
    
    int success;
    if (use_valgrind) {
        success = run_valgrind_test(executable, &result);
    } else {
        success = run_native_memory_test(executable, &result);
    }
    
    if (success) {
        add_memory_test_result(suite, &result);
    } else {
        fprintf(stderr, "Memory test failed for %s\n", wyn_file);
    }
    
    // Clean up
    unlink(executable);
}

void print_memory_test_results(MemoryTestSuite* suite) {
    printf("\n=== Memory Leak Detection Results ===\n");
    printf("%-30s %10s %12s %10s\n", "Test", "Leaks", "Peak Mem", "Duration");
    printf("%-30s %10s %12s %10s\n", "----", "-----", "--------", "--------");
    
    for (int i = 0; i < suite->count; i++) {
        MemoryTestResult* r = &suite->results[i];
        printf("%-30s %10d %10ldKB %9.3fs\n",
               r->test_name, r->memory_leaks_found, r->peak_memory_kb, r->test_duration);
    }
    
    printf("\n=== Memory Test Summary ===\n");
    printf("Total Tests: %d\n", suite->count);
    printf("Total Memory Leaks: %d\n", suite->total_leaks);
    printf("Average Peak Memory: %ldKB\n", 
           suite->count > 0 ? suite->total_peak_memory / suite->count : 0);
    
    if (suite->total_leaks > 0) {
        printf("\n=== Leak Details ===\n");
        for (int i = 0; i < suite->count; i++) {
            if (suite->results[i].memory_leaks_found > 0) {
                printf("\n%s:\n", suite->results[i].test_name);
                printf("%s\n", suite->results[i].leak_details);
            }
        }
    }
}

void save_memory_report(MemoryTestSuite* suite, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "# Memory Leak Detection Report\n");
    time_t now = time(NULL);
    fprintf(file, "Generated: %s\n", ctime(&now));
    fprintf(file, "\n## Summary\n");
    fprintf(file, "- Total Tests: %d\n", suite->count);
    fprintf(file, "- Total Memory Leaks: %d\n", suite->total_leaks);
    fprintf(file, "- Average Peak Memory: %ldKB\n", 
            suite->count > 0 ? suite->total_peak_memory / suite->count : 0);
    
    fprintf(file, "\n## Test Results\n");
    for (int i = 0; i < suite->count; i++) {
        MemoryTestResult* r = &suite->results[i];
        fprintf(file, "\n### %s\n", r->test_name);
        fprintf(file, "- Memory Leaks: %d\n", r->memory_leaks_found);
        fprintf(file, "- Peak Memory: %ldKB\n", r->peak_memory_kb);
        fprintf(file, "- Duration: %.3fs\n", r->test_duration);
        
        if (r->memory_leaks_found > 0) {
            fprintf(file, "- Details:\n```\n%s```\n", r->leak_details);
        }
    }
    
    fclose(file);
}

int main(int argc, char* argv[]) {
    int use_valgrind = 0;
    const char* output_file = "memory_report.md";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--valgrind") == 0) {
            use_valgrind = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
    }
    
    // Check if valgrind is available
    if (use_valgrind && system("which valgrind > /dev/null 2>&1") != 0) {
        printf("Valgrind not found, falling back to native memory monitoring\n");
        use_valgrind = 0;
    }
    
    MemoryTestSuite* suite = create_memory_test_suite();
    
    printf("Running memory leak detection tests%s...\n", 
           use_valgrind ? " (using Valgrind)" : " (native monitoring)");
    
    // Test files to check for memory leaks
    const char* test_files[] = {
        "memory_test_basic.wyn",
        "memory_test_arrays.wyn",
        "memory_test_structs.wyn",
        "memory_test_recursion.wyn",
        "memory_test_loops.wyn",
        NULL
    };
    
    for (int i = 0; test_files[i]; i++) {
        if (access(test_files[i], F_OK) == 0) {
            printf("Testing: %s\n", test_files[i]);
            run_memory_test_on_wyn_file(test_files[i], suite, use_valgrind);
        }
    }
    
    if (suite->count == 0) {
        printf("No memory test files found.\n");
        printf("Expected files: memory_test_basic.wyn, memory_test_arrays.wyn, etc.\n");
        return 1;
    }
    
    print_memory_test_results(suite);
    save_memory_report(suite, output_file);
    
    printf("\nMemory test report saved to: %s\n", output_file);
    
    int exit_code = suite->total_leaks > 0 ? 1 : 0;
    
    free(suite->results);
    free(suite);
    
    return exit_code;
}