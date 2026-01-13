#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    char test_file[256];
    char expected_output[1024];
    int expected_exit_code;
    double timeout_seconds;
} IntegrationTest;

typedef struct {
    char name[256];
    int passed;
    char error_msg[1024];
    double duration;
    char actual_output[1024];
} IntegrationResult;

int compile_wyn_file(const char* wyn_file, const char* output_file, char* error_msg, size_t error_size) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "../wyn %s -o %s 2>&1", wyn_file, output_file);
    
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        snprintf(error_msg, error_size, "Failed to execute compiler");
        return 0;
    }
    
    char buffer[1024];
    size_t total_read = 0;
    while (fgets(buffer, sizeof(buffer), pipe) && total_read < error_size - 1) {
        size_t len = strlen(buffer);
        if (total_read + len < error_size - 1) {
            strcpy(error_msg + total_read, buffer);
            total_read += len;
        }
    }
    
    int exit_code = pclose(pipe);
    return WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == 0;
}

int run_compiled_program(const char* executable, char* output, size_t output_size, 
                        double timeout_seconds, char* error_msg, size_t error_size) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        snprintf(error_msg, error_size, "Failed to create pipe");
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        snprintf(error_msg, error_size, "Failed to fork process");
        return -1;
    }
    
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        execl(executable, executable, NULL);
        exit(127);
    }
    
    close(pipefd[1]);
    
    // Read output with timeout
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = (int)timeout_seconds;
    timeout.tv_usec = (int)((timeout_seconds - timeout.tv_sec) * 1000000);
    
    FD_ZERO(&readfds);
    FD_SET(pipefd[0], &readfds);
    
    size_t total_read = 0;
    while (total_read < output_size - 1) {
        int ready = select(pipefd[0] + 1, &readfds, NULL, NULL, &timeout);
        if (ready <= 0) break;
        
        ssize_t bytes_read = read(pipefd[0], output + total_read, output_size - total_read - 1);
        if (bytes_read <= 0) break;
        total_read += bytes_read;
    }
    output[total_read] = '\0';
    
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        snprintf(error_msg, error_size, "Process terminated by signal %d", WTERMSIG(status));
        return -1;
    }
    
    return -1;
}

int run_integration_test(IntegrationTest* test, IntegrationResult* result) {
    clock_t start = clock();
    
    strncpy(result->name, test->test_file, sizeof(result->name) - 1);
    result->passed = 0;
    result->error_msg[0] = '\0';
    result->actual_output[0] = '\0';
    
    // Compile the Wyn file
    char executable[256];
    snprintf(executable, sizeof(executable), "%s.out", test->test_file);
    
    char compile_error[1024];
    if (!compile_wyn_file(test->test_file, executable, compile_error, sizeof(compile_error))) {
        snprintf(result->error_msg, sizeof(result->error_msg), 
                "Compilation failed: %s", compile_error);
        result->duration = ((double)(clock() - start)) / CLOCKS_PER_SEC;
        return 0;
    }
    
    // Run the compiled program
    char run_error[1024] = "";
    int exit_code = run_compiled_program(executable, result->actual_output, 
                                       sizeof(result->actual_output), 
                                       test->timeout_seconds, run_error, sizeof(run_error));
    
    // Clean up executable
    unlink(executable);
    
    result->duration = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    
    // Check results
    if (exit_code != test->expected_exit_code) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                "Exit code mismatch. Expected: %d, Actual: %d. %s",
                test->expected_exit_code, exit_code, run_error);
        return 0;
    }
    
    if (strlen(test->expected_output) > 0 && 
        strcmp(test->expected_output, result->actual_output) != 0) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                "Output mismatch. Expected: '%s', Actual: '%s'",
                test->expected_output, result->actual_output);
        return 0;
    }
    
    result->passed = 1;
    return 1;
}

void load_test_config(const char* config_file, IntegrationTest** tests, int* test_count) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        *tests = NULL;
        *test_count = 0;
        return;
    }
    
    int capacity = 10;
    *tests = malloc(sizeof(IntegrationTest) * capacity);
    *test_count = 0;
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (*test_count >= capacity) {
            capacity *= 2;
            *tests = realloc(*tests, sizeof(IntegrationTest) * capacity);
        }
        
        IntegrationTest* test = &(*tests)[*test_count];
        
        // Parse: test_file|expected_output|expected_exit_code|timeout
        char* token = strtok(line, "|");
        if (token) strncpy(test->test_file, token, sizeof(test->test_file) - 1);
        
        token = strtok(NULL, "|");
        if (token) strncpy(test->expected_output, token, sizeof(test->expected_output) - 1);
        
        token = strtok(NULL, "|");
        test->expected_exit_code = token ? atoi(token) : 0;
        
        token = strtok(NULL, "|");
        test->timeout_seconds = token ? atof(token) : 5.0;
        
        (*test_count)++;
    }
    
    fclose(file);
}

int main(int argc, char* argv[]) {
    const char* config_file = argc > 1 ? argv[1] : "integration_tests.conf";
    
    IntegrationTest* tests;
    int test_count;
    
    load_test_config(config_file, &tests, &test_count);
    
    if (test_count == 0) {
        printf("No integration tests found in %s\n", config_file);
        return 0;
    }
    
    printf("Running %d integration tests...\n", test_count);
    
    IntegrationResult* results = malloc(sizeof(IntegrationResult) * test_count);
    int passed = 0;
    double total_duration = 0;
    
    for (int i = 0; i < test_count; i++) {
        printf("Running %s... ", tests[i].test_file);
        fflush(stdout);
        
        if (run_integration_test(&tests[i], &results[i])) {
            printf("PASS (%.3fs)\n", results[i].duration);
            passed++;
        } else {
            printf("FAIL (%.3fs)\n", results[i].duration);
        }
        
        total_duration += results[i].duration;
    }
    
    printf("\n=== Integration Test Results ===\n");
    printf("Total Tests: %d\n", test_count);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", test_count - passed);
    printf("Total Duration: %.3fs\n", total_duration);
    printf("Success Rate: %.1f%%\n", 
           test_count > 0 ? (double)passed / test_count * 100 : 0);
    
    if (passed < test_count) {
        printf("\n=== Failed Tests ===\n");
        for (int i = 0; i < test_count; i++) {
            if (!results[i].passed) {
                printf("FAIL: %s\n", results[i].name);
                printf("  Error: %s\n", results[i].error_msg);
            }
        }
    }
    
    free(tests);
    free(results);
    
    return passed == test_count ? 0 : 1;
}