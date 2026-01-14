#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>

extern int cmd_compile(const char* target, int argc, char** argv);

typedef struct {
    int total;
    int passed;
    int failed;
    double total_time;
} TestResults;

int cmd_test(const char* test_dir, int argc, char** argv) {
    if (!test_dir) test_dir = "tests";
    
    printf("🧪 Wyn Test Runner\n");
    printf("Running tests in: %s\n\n", test_dir);
    
    DIR* dir = opendir(test_dir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s\n", test_dir);
        return 1;
    }
    
    TestResults results = {0, 0, 0, 0.0};
    struct dirent* entry;
    clock_t start_time = clock();
    
    while ((entry = readdir(dir)) != NULL) {
        // Only test files starting with "test_" and ending with ".wyn"
        if (strncmp(entry->d_name, "test_", 5) != 0) continue;
        if (!strstr(entry->d_name, ".wyn")) continue;
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", test_dir, entry->d_name);
        
        results.total++;
        
        clock_t test_start = clock();
        
        // Compile the test
        if (cmd_compile(filepath, 0, NULL) == 0) {
            // Run the test
            char output[512];
            snprintf(output, sizeof(output), "%s.out", filepath);
            
            int exit_code = system(output);
            double test_time = (double)(clock() - test_start) / CLOCKS_PER_SEC;
            
            if (exit_code == 0) {
                results.passed++;
                printf("✅ %s (%.3fs)\n", entry->d_name, test_time);
            } else {
                results.failed++;
                printf("❌ %s (exit code: %d, %.3fs)\n", entry->d_name, exit_code, test_time);
            }
        } else {
            results.failed++;
            double test_time = (double)(clock() - test_start) / CLOCKS_PER_SEC;
            printf("❌ %s (compilation failed, %.3fs)\n", entry->d_name, test_time);
        }
    }
    
    closedir(dir);
    
    results.total_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    
    printf("\n=== Test Results ===\n");
    printf("Total:   %d\n", results.total);
    printf("Passed:  %d (%.1f%%)\n", results.passed, 
           results.total > 0 ? (100.0 * results.passed / results.total) : 0.0);
    printf("Failed:  %d\n", results.failed);
    printf("Time:    %.3fs\n", results.total_time);
    
    if (results.failed == 0) {
        printf("\n🎉 All tests passed!\n");
    }
    
    return results.failed > 0 ? 1 : 0;
}
