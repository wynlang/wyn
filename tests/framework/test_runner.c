#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    char name[256];
    int passed;
    int failed;
    double duration;
    char error_msg[1024];
} TestResult;

typedef struct {
    TestResult* results;
    int count;
    int capacity;
    int total_passed;
    int total_failed;
    double total_duration;
} TestSuite;

TestSuite* create_test_suite() {
    TestSuite* suite = malloc(sizeof(TestSuite));
    suite->capacity = 100;
    suite->results = malloc(sizeof(TestResult) * suite->capacity);
    suite->count = 0;
    suite->total_passed = 0;
    suite->total_failed = 0;
    suite->total_duration = 0.0;
    return suite;
}

void add_test_result(TestSuite* suite, const char* name, int passed, double duration, const char* error) {
    if (suite->count >= suite->capacity) {
        suite->capacity *= 2;
        suite->results = realloc(suite->results, sizeof(TestResult) * suite->capacity);
    }
    
    TestResult* result = &suite->results[suite->count++];
    strncpy(result->name, name, sizeof(result->name) - 1);
    result->passed = passed;
    result->failed = !passed;
    result->duration = duration;
    strncpy(result->error_msg, error ? error : "", sizeof(result->error_msg) - 1);
    
    if (passed) {
        suite->total_passed++;
    } else {
        suite->total_failed++;
    }
    suite->total_duration += duration;
}

int run_test_executable(const char* test_path, char* error_buffer, size_t error_size) {
    clock_t start = clock();
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        snprintf(error_buffer, error_size, "Failed to create pipe");
        return 0;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        snprintf(error_buffer, error_size, "Failed to fork process");
        return 0;
    }
    
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        execl(test_path, test_path, NULL);
        exit(1);
    }
    
    close(pipefd[1]);
    
    char buffer[1024];
    ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        strncpy(error_buffer, buffer, error_size - 1);
        error_buffer[error_size - 1] = '\0';
    }
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void discover_tests(const char* test_dir, TestSuite* suite) {
    DIR* dir = opendir(test_dir);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "test_", 5) == 0) {
            char test_path[512];
            snprintf(test_path, sizeof(test_path), "%s/%s", test_dir, entry->d_name);
            
            struct stat st;
            if (stat(test_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
                clock_t start = clock();
                char error_msg[1024] = "";
                int passed = run_test_executable(test_path, error_msg, sizeof(error_msg));
                double duration = ((double)(clock() - start)) / CLOCKS_PER_SEC;
                
                add_test_result(suite, entry->d_name, passed, duration, error_msg);
            }
        }
    }
    closedir(dir);
}

void print_test_results(TestSuite* suite) {
    printf("\n=== Test Results ===\n");
    printf("Total Tests: %d\n", suite->count);
    printf("Passed: %d\n", suite->total_passed);
    printf("Failed: %d\n", suite->total_failed);
    printf("Total Duration: %.3fs\n", suite->total_duration);
    printf("Success Rate: %.1f%%\n", 
           suite->count > 0 ? (double)suite->total_passed / suite->count * 100 : 0);
    
    if (suite->total_failed > 0) {
        printf("\n=== Failed Tests ===\n");
        for (int i = 0; i < suite->count; i++) {
            if (!suite->results[i].passed) {
                printf("FAIL: %s (%.3fs)\n", suite->results[i].name, suite->results[i].duration);
                if (strlen(suite->results[i].error_msg) > 0) {
                    printf("  Error: %s\n", suite->results[i].error_msg);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    const char* test_dir = argc > 1 ? argv[1] : ".";
    
    TestSuite* suite = create_test_suite();
    
    printf("Discovering and running tests in: %s\n", test_dir);
    discover_tests(test_dir, suite);
    
    print_test_results(suite);
    
    int exit_code = suite->total_failed > 0 ? 1 : 0;
    
    free(suite->results);
    free(suite);
    
    return exit_code;
}