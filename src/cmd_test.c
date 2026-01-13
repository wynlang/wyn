#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

extern int cmd_compile(const char* target, int argc, char** argv);

typedef struct {
    int total;
    int passed;
    int failed;
} TestResults;

int cmd_test(const char* test_dir, int argc, char** argv) {
    if (!test_dir) test_dir = "tests";
    
    printf("Running tests in: %s\n\n", test_dir);
    
    DIR* dir = opendir(test_dir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s\n", test_dir);
        return 1;
    }
    
    TestResults results = {0, 0, 0};
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Only test files starting with "test_" and ending with ".wyn"
        if (strncmp(entry->d_name, "test_", 5) != 0) continue;
        if (!strstr(entry->d_name, ".wyn")) continue;
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", test_dir, entry->d_name);
        
        results.total++;
        
        // Compile the test
        if (cmd_compile(filepath, 0, NULL) == 0) {
            // Run the test
            char output[512];
            snprintf(output, sizeof(output), "%s.out", filepath);
            
            int exit_code = system(output);
            if (exit_code == 0) {
                results.passed++;
                printf("✅ %s\n", entry->d_name);
            } else {
                results.failed++;
                printf("❌ %s (exit code: %d)\n", entry->d_name, exit_code);
            }
        } else {
            results.failed++;
            printf("❌ %s (compilation failed)\n", entry->d_name);
        }
    }
    
    closedir(dir);
    
    printf("\n=== Test Results ===\n");
    printf("Total:  %d\n", results.total);
    printf("Passed: %d\n", results.passed);
    printf("Failed: %d\n", results.failed);
    
    return results.failed > 0 ? 1 : 0;
}
