#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

typedef struct {
    int total;
    int passed;
    int failed;
} TestResults;

static TestResults results = {0, 0, 0};

int run_test_file(const char* filename) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./wyn %s 2>&1 > /dev/null && %s.out", filename, filename);
    
    int exit_code = system(cmd);
    results.total++;
    
    if (exit_code == 0) {
        results.passed++;
        printf("✅ %s\n", filename);
        return 0;
    } else {
        results.failed++;
        printf("❌ %s\n", filename);
        return 1;
    }
}

int main(int argc, char** argv) {
    const char* test_dir = argc > 1 ? argv[1] : "tests";
    
    printf("Running tests in: %s\n\n", test_dir);
    
    DIR* dir = opendir(test_dir);
    if (!dir) {
        printf("Error: Cannot open directory %s\n", test_dir);
        return 1;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wyn") && strstr(entry->d_name, "test_")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", test_dir, entry->d_name);
            run_test_file(filepath);
        }
    }
    
    closedir(dir);
    
    printf("\n=== Test Results ===\n");
    printf("Total:  %d\n", results.total);
    printf("Passed: %d\n", results.passed);
    printf("Failed: %d\n", results.failed);
    
    return results.failed > 0 ? 1 : 0;
}
