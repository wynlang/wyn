#ifndef WYN_SYSTEM_H
#define WYN_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// Forward declarations
typedef struct WynProcess WynProcess;
typedef struct WynSystemInfo WynSystemInfo;

// System error codes
typedef enum {
    WYN_SYS_OK,
    WYN_SYS_NOT_FOUND,
    WYN_SYS_PERMISSION_DENIED,
    WYN_SYS_INVALID_ARGUMENT,
    WYN_SYS_PROCESS_ERROR,
    WYN_SYS_MEMORY_ERROR,
    WYN_SYS_IO_ERROR,
    WYN_SYS_UNKNOWN_ERROR
} WynSystemError;

// Process exit status
typedef struct {
    bool success;
    int code;
    bool terminated_by_signal;
    int signal;
} WynExitStatus;

// Process structure
typedef struct WynProcess {
    pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    bool running;
} WynProcess;

// System information
typedef struct WynSystemInfo {
    char* os_name;
    char* os_version;
    char* arch;
    size_t total_memory;
    size_t available_memory;
    size_t cpu_count;
    char* hostname;
} WynSystemInfo;

// Environment variable operations
char* wyn_sys_get_env(const char* name);
WynSystemError wyn_sys_set_env(const char* name, const char* value);
WynSystemError wyn_sys_unset_env(const char* name);
char** wyn_sys_get_all_env(size_t* count);
void wyn_sys_free_env_list(char** env_list, size_t count);

// Directory operations
char* wyn_sys_current_dir(void);
WynSystemError wyn_sys_set_current_dir(const char* path);
char* wyn_sys_home_dir(void);
char* wyn_sys_temp_dir(void);

// Process operations
WynProcess* wyn_sys_spawn_process(const char* command, char* const* args, WynSystemError* error);
WynSystemError wyn_sys_wait_process(WynProcess* process, WynExitStatus* status);
WynSystemError wyn_sys_kill_process(WynProcess* process);
WynSystemError wyn_sys_terminate_process(WynProcess* process);
void wyn_sys_free_process(WynProcess* process);

// Process I/O operations
ssize_t wyn_sys_process_write_stdin(WynProcess* process, const void* data, size_t size);
ssize_t wyn_sys_process_read_stdout(WynProcess* process, void* buffer, size_t size);
ssize_t wyn_sys_process_read_stderr(WynProcess* process, void* buffer, size_t size);
WynSystemError wyn_sys_process_close_stdin(WynProcess* process);

// Signal handling (Unix) / Event handling (Windows)
typedef void (*WynSignalHandler)(int signal);
WynSystemError wyn_sys_set_signal_handler(int signal, WynSignalHandler handler);
WynSystemError wyn_sys_ignore_signal(int signal);
WynSystemError wyn_sys_default_signal(int signal);
WynSystemError wyn_sys_send_signal(pid_t pid, int signal);

// System information
WynSystemInfo* wyn_sys_get_system_info(void);
void wyn_sys_free_system_info(WynSystemInfo* info);
size_t wyn_sys_get_page_size(void);
uint64_t wyn_sys_get_uptime_ms(void);

// File system utilities
bool wyn_sys_file_exists(const char* path);
bool wyn_sys_is_directory(const char* path);
bool wyn_sys_is_file(const char* path);
WynSystemError wyn_sys_create_directory(const char* path);
WynSystemError wyn_sys_remove_file(const char* path);
WynSystemError wyn_sys_remove_directory(const char* path);

// Path utilities
char* wyn_sys_join_path(const char* base, const char* component);
char* wyn_sys_absolute_path(const char* path);
char* wyn_sys_basename(const char* path);
char* wyn_sys_dirname(const char* path);

// Error handling
const char* wyn_sys_error_string(WynSystemError error);

// Platform-specific constants
#ifdef _WIN32
#define WYN_SYS_PATH_SEPARATOR '\\'
#define WYN_SYS_PATH_SEPARATOR_STR "\\"
#define WYN_SYS_PATH_LIST_SEPARATOR ';'
#else
#define WYN_SYS_PATH_SEPARATOR '/'
#define WYN_SYS_PATH_SEPARATOR_STR "/"
#define WYN_SYS_PATH_LIST_SEPARATOR ':'
#endif

// Common signals (Unix-style, mapped on Windows)
#define WYN_SYS_SIGINT  2
#define WYN_SYS_SIGTERM 15
#define WYN_SYS_SIGKILL 9
#define WYN_SYS_SIGUSR1 10
#define WYN_SYS_SIGUSR2 12

#endif // WYN_SYSTEM_H
