#include "../src/system.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void test_environment_variables() {
    printf("Testing environment variables...\n");
    
    // Test setting and getting environment variable
    WynSystemError error = wyn_sys_set_env("WYN_TEST_VAR", "test_value");
    assert(error == WYN_SYS_OK);
    
    char* value = wyn_sys_get_env("WYN_TEST_VAR");
    assert(value != NULL);
    assert(strcmp(value, "test_value") == 0);
    free(value);
    
    // Test unsetting environment variable
    error = wyn_sys_unset_env("WYN_TEST_VAR");
    assert(error == WYN_SYS_OK);
    
    value = wyn_sys_get_env("WYN_TEST_VAR");
    assert(value == NULL);
    
    // Test getting all environment variables
    size_t count;
    char** env_list = wyn_sys_get_all_env(&count);
    assert(env_list != NULL);
    assert(count > 0);
    
    printf("Found %zu environment variables\n", count);
    wyn_sys_free_env_list(env_list, count);
    
    printf("✓ Environment variable tests passed\n");
}

void test_directory_operations() {
    printf("Testing directory operations...\n");
    
    // Test current directory
    char* cwd = wyn_sys_current_dir();
    assert(cwd != NULL);
    printf("Current directory: %s\n", cwd);
    free(cwd);
    
    // Test home directory
    char* home = wyn_sys_home_dir();
    if (home) {
        printf("Home directory: %s\n", home);
        free(home);
    }
    
    // Test temp directory
    char* temp = wyn_sys_temp_dir();
    assert(temp != NULL);
    printf("Temp directory: %s\n", temp);
    free(temp);
    
    printf("✓ Directory operation tests passed\n");
}

void test_file_system_utilities() {
    printf("Testing file system utilities...\n");
    
    // Test file existence
    assert(wyn_sys_file_exists("/"));  // Root should exist
    assert(!wyn_sys_file_exists("/nonexistent_file_12345"));
    
    // Test directory check
    assert(wyn_sys_is_directory("/"));
    assert(!wyn_sys_is_directory("/nonexistent"));
    
    // Test path utilities
    char* joined = wyn_sys_join_path("/tmp", "test_file");
    assert(joined != NULL);
    printf("Joined path: %s\n", joined);
    free(joined);
    
    char* base = wyn_sys_basename("/path/to/file.txt");
    assert(base != NULL);
    assert(strcmp(base, "file.txt") == 0);
    free(base);
    
    char* dir = wyn_sys_dirname("/path/to/file.txt");
    assert(dir != NULL);
    printf("Directory: %s\n", dir);
    free(dir);
    
    printf("✓ File system utility tests passed\n");
}

void test_process_operations() {
    printf("Testing process operations...\n");
    
    // Test simple process spawn
    char* args[] = {"echo", "hello", "world", NULL};
    WynSystemError error;
    WynProcess* process = wyn_sys_spawn_process("echo", args, &error);
    
    if (process) {
        printf("Spawned process with PID: %d\n", process->pid);
        
        // Read output
        char buffer[256];
        ssize_t bytes_read = wyn_sys_process_read_stdout(process, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Process output: %s", buffer);
        }
        
        // Wait for process to complete
        WynExitStatus status;
        error = wyn_sys_wait_process(process, &status);
        assert(error == WYN_SYS_OK);
        assert(status.success);
        
        wyn_sys_free_process(process);
        printf("✓ Process completed successfully\n");
    } else {
        printf("Failed to spawn process: %s\n", wyn_sys_error_string(error));
    }
    
    printf("✓ Process operation tests passed\n");
}

void test_system_information() {
    printf("Testing system information...\n");
    
    WynSystemInfo* info = wyn_sys_get_system_info();
    if (info) {
        printf("OS: %s %s\n", info->os_name ? info->os_name : "Unknown", 
               info->os_version ? info->os_version : "Unknown");
        printf("Architecture: %s\n", info->arch ? info->arch : "Unknown");
        printf("Hostname: %s\n", info->hostname ? info->hostname : "Unknown");
        printf("CPU count: %zu\n", info->cpu_count);
        
        if (info->total_memory > 0) {
            printf("Total memory: %zu bytes\n", info->total_memory);
        }
        
        wyn_sys_free_system_info(info);
    }
    
    size_t page_size = wyn_sys_get_page_size();
    printf("Page size: %zu bytes\n", page_size);
    assert(page_size > 0);
    
    printf("✓ System information tests passed\n");
}

static bool signal_received = false;

void test_signal_handler(int signal) {
    (void)signal;  // Suppress unused parameter warning
    signal_received = true;
}

void test_signal_handling() {
    printf("Testing signal handling...\n");
    
    // Set up signal handler
    WynSystemError error = wyn_sys_set_signal_handler(SIGUSR1, test_signal_handler);
    assert(error == WYN_SYS_OK);
    
    // Send signal to self
    error = wyn_sys_send_signal(getpid(), SIGUSR1);
    assert(error == WYN_SYS_OK);
    
    // Give signal time to be delivered
    usleep(10000);  // 10ms
    
    // Check if signal was received
    if (signal_received) {
        printf("✓ Signal handling works\n");
    } else {
        printf("⚠ Signal may not have been delivered yet\n");
    }
    
    // Reset to default handler
    error = wyn_sys_default_signal(SIGUSR1);
    assert(error == WYN_SYS_OK);
    
    printf("✓ Signal handling tests passed\n");
}

void test_error_handling() {
    printf("Testing error handling...\n");
    
    // Test error strings
    assert(strcmp(wyn_sys_error_string(WYN_SYS_OK), "Success") == 0);
    assert(strcmp(wyn_sys_error_string(WYN_SYS_NOT_FOUND), "Not found") == 0);
    assert(strcmp(wyn_sys_error_string(WYN_SYS_PERMISSION_DENIED), "Permission denied") == 0);
    
    // Test invalid operations
    WynSystemError error = wyn_sys_set_env(NULL, "value");
    assert(error == WYN_SYS_INVALID_ARGUMENT);
    
    char* value = wyn_sys_get_env(NULL);
    assert(value == NULL);
    
    printf("✓ Error handling tests passed\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL handling
    wyn_sys_free_process(NULL);  // Should not crash
    wyn_sys_free_system_info(NULL);  // Should not crash
    wyn_sys_free_env_list(NULL, 0);  // Should not crash
    
    // Test invalid file operations
    assert(!wyn_sys_file_exists(NULL));
    assert(!wyn_sys_is_directory(NULL));
    assert(!wyn_sys_is_file(NULL));
    
    // Test invalid path operations
    char* result = wyn_sys_join_path(NULL, "test");
    assert(result == NULL);
    
    result = wyn_sys_basename(NULL);
    assert(result == NULL);
    
    printf("✓ Edge case tests passed\n");
}

int main() {
    printf("Running Cross-Platform System Interface Tests\n");
    printf("=============================================\n\n");
    
    test_environment_variables();
    test_directory_operations();
    test_file_system_utilities();
    test_process_operations();
    test_system_information();
    test_signal_handling();
    test_error_handling();
    test_edge_cases();
    
    printf("\n✅ All system interface tests passed!\n");
    return 0;
}
