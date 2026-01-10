#include "../src/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void example_environment_variables() {
    printf("=== Environment Variables ===\n");
    
    // Get environment variable
    char* path = wyn_sys_get_env("PATH");
    if (path) {
        printf("PATH: %.100s...\n", path);  // Show first 100 chars
        free(path);
    }
    
    // Set custom environment variable
    wyn_sys_set_env("WYN_APP_NAME", "MyWynApp");
    char* app_name = wyn_sys_get_env("WYN_APP_NAME");
    printf("App name: %s\n", app_name);
    free(app_name);
    
    // List all environment variables
    size_t count;
    char** env_list = wyn_sys_get_all_env(&count);
    printf("Total environment variables: %zu\n", count);
    
    // Show first few
    for (size_t i = 0; i < 3 && i < count; i++) {
        printf("  %s\n", env_list[i]);
    }
    
    wyn_sys_free_env_list(env_list, count);
    printf("\n");
}

void example_directory_operations() {
    printf("=== Directory Operations ===\n");
    
    // Get current directory
    char* cwd = wyn_sys_current_dir();
    printf("Current directory: %s\n", cwd);
    free(cwd);
    
    // Get home directory
    char* home = wyn_sys_home_dir();
    if (home) {
        printf("Home directory: %s\n", home);
        free(home);
    }
    
    // Get temp directory
    char* temp = wyn_sys_temp_dir();
    printf("Temp directory: %s\n", temp);
    free(temp);
    
    // Path manipulation
    char* home_copy = wyn_sys_home_dir();
    char* config_path = wyn_sys_join_path(home_copy ? home_copy : "/tmp", ".config");
    printf("Config path: %s\n", config_path);
    free(config_path);
    free(home_copy);
    
    printf("\n");
}

void example_process_management() {
    printf("=== Process Management ===\n");
    
    // Spawn a simple process
    char* args[] = {"ls", "-la", "/tmp", NULL};
    WynSystemError error;
    WynProcess* process = wyn_sys_spawn_process("ls", args, &error);
    
    if (process) {
        printf("Spawned 'ls' process (PID: %d)\n", process->pid);
        
        // Read output
        char buffer[1024];
        ssize_t bytes_read = wyn_sys_process_read_stdout(process, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("First %zd bytes of output:\n%s\n", bytes_read, buffer);
        }
        
        // Wait for completion
        WynExitStatus status;
        wyn_sys_wait_process(process, &status);
        printf("Process exited with code: %d\n", status.code);
        
        wyn_sys_free_process(process);
    } else {
        printf("Failed to spawn process: %s\n", wyn_sys_error_string(error));
    }
    
    printf("\n");
}

void example_interactive_process() {
    printf("=== Interactive Process ===\n");
    
    // Spawn cat process for interactive I/O
    char* args[] = {"cat", NULL};
    WynSystemError error;
    WynProcess* process = wyn_sys_spawn_process("cat", args, &error);
    
    if (process) {
        printf("Spawned 'cat' process for interactive I/O\n");
        
        // Write to process stdin
        const char* input = "Hello from Wyn!\n";
        ssize_t written = wyn_sys_process_write_stdin(process, input, strlen(input));
        printf("Wrote %zd bytes to process\n", written);
        
        // Close stdin to signal EOF
        wyn_sys_process_close_stdin(process);
        
        // Read echoed output
        char buffer[256];
        ssize_t bytes_read = wyn_sys_process_read_stdout(process, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Process echoed: %s", buffer);
        }
        
        // Wait for completion
        WynExitStatus status;
        wyn_sys_wait_process(process, &status);
        
        wyn_sys_free_process(process);
    }
    
    printf("\n");
}

void example_file_system_operations() {
    printf("=== File System Operations ===\n");
    
    // Check file existence
    printf("Root directory exists: %s\n", wyn_sys_file_exists("/") ? "yes" : "no");
    printf("Is root a directory: %s\n", wyn_sys_is_directory("/") ? "yes" : "no");
    
    // Create a test directory
    char* temp_dir = wyn_sys_temp_dir();
    char* test_dir = wyn_sys_join_path(temp_dir, "wyn_test_dir");
    free(temp_dir);
    WynSystemError error = wyn_sys_create_directory(test_dir);
    if (error == WYN_SYS_OK) {
        printf("Created test directory: %s\n", test_dir);
        
        // Remove it
        wyn_sys_remove_directory(test_dir);
        printf("Removed test directory\n");
    }
    free(test_dir);
    
    // Path utilities
    char* abs_path = wyn_sys_absolute_path(".");
    if (abs_path) {
        printf("Absolute path of '.': %s\n", abs_path);
        free(abs_path);
    }
    
    char* basename = wyn_sys_basename("/path/to/file.txt");
    printf("Basename of '/path/to/file.txt': %s\n", basename);
    free(basename);
    
    printf("\n");
}

static bool signal_handled = false;

void example_signal_handler(int signal) {
    printf("Received signal %d\n", signal);
    signal_handled = true;
}

void example_signal_handling() {
    printf("=== Signal Handling ===\n");
    
    // Set up signal handler
    wyn_sys_set_signal_handler(SIGUSR1, example_signal_handler);
    printf("Set up handler for SIGUSR1\n");
    
    // Send signal to self
    printf("Sending SIGUSR1 to self...\n");
    wyn_sys_send_signal(getpid(), SIGUSR1);
    
    // Give signal time to be delivered
    usleep(10000);  // 10ms
    
    if (signal_handled) {
        printf("Signal was handled successfully\n");
    }
    
    // Reset to default
    wyn_sys_default_signal(SIGUSR1);
    printf("Reset signal handler to default\n");
    
    printf("\n");
}

void example_system_information() {
    printf("=== System Information ===\n");
    
    WynSystemInfo* info = wyn_sys_get_system_info();
    if (info) {
        printf("Operating System: %s %s\n", 
               info->os_name ? info->os_name : "Unknown",
               info->os_version ? info->os_version : "");
        printf("Architecture: %s\n", info->arch ? info->arch : "Unknown");
        printf("Hostname: %s\n", info->hostname ? info->hostname : "Unknown");
        printf("CPU cores: %zu\n", info->cpu_count);
        
        if (info->total_memory > 0) {
            printf("Total memory: %.2f GB\n", info->total_memory / (1024.0 * 1024.0 * 1024.0));
        }
        
        wyn_sys_free_system_info(info);
    }
    
    printf("Page size: %zu bytes\n", wyn_sys_get_page_size());
    
    printf("\n");
}

void example_error_handling() {
    printf("=== Error Handling ===\n");
    
    // Demonstrate error handling
    WynSystemError error = wyn_sys_set_current_dir("/nonexistent_directory");
    if (error != WYN_SYS_OK) {
        printf("Expected error changing to nonexistent directory: %s\n", 
               wyn_sys_error_string(error));
    }
    
    // Try to spawn nonexistent command
    char* args[] = {"nonexistent_command_12345", NULL};
    WynProcess* process = wyn_sys_spawn_process("nonexistent_command_12345", args, &error);
    if (!process) {
        printf("Expected error spawning nonexistent command: %s\n", 
               wyn_sys_error_string(error));
    }
    
    printf("Error handling works correctly\n");
    printf("\n");
}

void example_cross_platform_paths() {
    printf("=== Cross-Platform Path Handling ===\n");
    
    printf("Path separator: '%c'\n", WYN_SYS_PATH_SEPARATOR);
    printf("Path list separator: '%c'\n", WYN_SYS_PATH_LIST_SEPARATOR);
    
    // Build paths correctly for the platform
    char* home_dir = wyn_sys_home_dir();
    char* config_dir = wyn_sys_join_path(home_dir, ".config");
    char* app_config = wyn_sys_join_path(config_dir, "myapp");
    char* config_file = wyn_sys_join_path(app_config, "config.toml");
    
    printf("Config file path: %s\n", config_file);
    
    free(home_dir);
    free(config_dir);
    free(app_config);
    free(config_file);
    
    printf("\n");
}

int main() {
    printf("Wyn Cross-Platform System Interface Examples\n");
    printf("============================================\n\n");
    
    example_environment_variables();
    example_directory_operations();
    example_process_management();
    example_interactive_process();
    example_file_system_operations();
    example_signal_handling();
    example_system_information();
    example_error_handling();
    example_cross_platform_paths();
    
    printf("All system interface examples completed successfully!\n");
    return 0;
}
