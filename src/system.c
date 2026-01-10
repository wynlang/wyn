#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <direct.h>
    #include <sys/stat.h>
    #include <io.h>
    #include "windows_compat.h"
    #define setenv(name, value, overwrite) _putenv_s(name, value)
    #define unsetenv(name) _putenv_s(name, "")
    #define access _access
    #define F_OK 0
    #define stat _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #define mkdir(path, mode) _mkdir(path)
    #define getpagesize() 4096
    #define realpath(path, resolved) _fullpath(resolved, path, _MAX_PATH)
    #define basename(path) _basename(path)
    #define dirname(path) _dirname(path)
    #define close _close
    #define read _read
    #define write _write
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/stat.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <libgen.h>
    #include <pwd.h>
    #include <sys/utsname.h>
#endif

#include <errno.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

// Environment variable operations
char* wyn_sys_get_env(const char* name) {
    if (!name) return NULL;
    
    char* value = getenv(name);
    return value ? strdup(value) : NULL;
}

WynSystemError wyn_sys_set_env(const char* name, const char* value) {
    if (!name || !value) return WYN_SYS_INVALID_ARGUMENT;
    
#ifdef _WIN32
    int result = _putenv_s(name, value);
#else
    int result = setenv(name, value, 1);
#endif
    return result == 0 ? WYN_SYS_OK : WYN_SYS_UNKNOWN_ERROR;
}

WynSystemError wyn_sys_unset_env(const char* name) {
    if (!name) return WYN_SYS_INVALID_ARGUMENT;
    
#ifdef _WIN32
    int result = _putenv_s(name, "");
#else
    int result = unsetenv(name);
#endif
    return result == 0 ? WYN_SYS_OK : WYN_SYS_UNKNOWN_ERROR;
}

extern char **environ;

char** wyn_sys_get_all_env(size_t* count) {
    if (!count) return NULL;
    
    // Count environment variables
    size_t env_count = 0;
    for (char** env = environ; *env; env++) {
        env_count++;
    }
    
    // Allocate array
    char** env_list = malloc((env_count + 1) * sizeof(char*));
    if (!env_list) {
        *count = 0;
        return NULL;
    }
    
    // Copy environment variables
    for (size_t i = 0; i < env_count; i++) {
        env_list[i] = strdup(environ[i]);
        if (!env_list[i]) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                free(env_list[j]);
            }
            free(env_list);
            *count = 0;
            return NULL;
        }
    }
    env_list[env_count] = NULL;
    
    *count = env_count;
    return env_list;
}

void wyn_sys_free_env_list(char** env_list, size_t count) {
    if (!env_list) return;
    
    for (size_t i = 0; i < count; i++) {
        free(env_list[i]);
    }
    free(env_list);
}

// Directory operations
char* wyn_sys_current_dir(void) {
    char* cwd = getcwd(NULL, 0);
    return cwd;  // getcwd allocates memory when size is 0
}

WynSystemError wyn_sys_set_current_dir(const char* path) {
    if (!path) return WYN_SYS_INVALID_ARGUMENT;
    
    int result = chdir(path);
    if (result == 0) return WYN_SYS_OK;
    
    switch (errno) {
        case ENOENT: return WYN_SYS_NOT_FOUND;
        case EACCES: return WYN_SYS_PERMISSION_DENIED;
        default: return WYN_SYS_UNKNOWN_ERROR;
    }
}

char* wyn_sys_home_dir(void) {
#ifdef _WIN32
    char* home = getenv("USERPROFILE");
    return home ? strdup(home) : NULL;
#else
    char* home = getenv("HOME");
    if (home) return strdup(home);
    
    // Fallback to passwd entry
    struct passwd* pw = getpwuid(getuid());
    return pw ? strdup(pw->pw_dir) : NULL;
#endif
}

char* wyn_sys_temp_dir(void) {
    char* tmpdir = getenv("TMPDIR");
    if (tmpdir) return strdup(tmpdir);
    
    tmpdir = getenv("TMP");
    if (tmpdir) return strdup(tmpdir);
    
    return strdup("/tmp");
}

// Process operations
WynProcess* wyn_sys_spawn_process(const char* command, char* const* args, WynSystemError* error) {
    if (!command) {
        if (error) *error = WYN_SYS_INVALID_ARGUMENT;
        return NULL;
    }

#ifdef _WIN32
    // Windows stub - not implemented
    if (error) *error = WYN_SYS_NOT_SUPPORTED;
    return NULL;
#else
    
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    
    // Create pipes
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        if (error) *error = WYN_SYS_IO_ERROR;
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        // Fork failed
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        if (error) *error = WYN_SYS_PROCESS_ERROR;
        return NULL;
    }
    
    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);   // Close write end of stdin
        close(stdout_pipe[0]);  // Close read end of stdout
        close(stderr_pipe[0]);  // Close read end of stderr
        
        // Redirect stdin, stdout, stderr
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        // Close pipe file descriptors
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        // Execute command
        execvp(command, args);
        _exit(127);  // If execvp fails
    }
    
    // Parent process
    close(stdin_pipe[0]);   // Close read end of stdin
    close(stdout_pipe[1]);  // Close write end of stdout
    close(stderr_pipe[1]);  // Close write end of stderr
    
    WynProcess* process = malloc(sizeof(WynProcess));
    if (!process) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        kill(pid, SIGTERM);
        if (error) *error = WYN_SYS_MEMORY_ERROR;
        return NULL;
    }
    
    process->pid = pid;
    process->stdin_fd = stdin_pipe[1];
    process->stdout_fd = stdout_pipe[0];
    process->stderr_fd = stderr_pipe[0];
    process->running = true;
    
    if (error) *error = WYN_SYS_OK;
    return process;
#endif
}

WynSystemError wyn_sys_wait_process(WynProcess* process, WynExitStatus* status) {
    if (!process) return WYN_SYS_INVALID_ARGUMENT;
    
#ifdef _WIN32
    // Windows stub - not implemented
    return WYN_SYS_NOT_SUPPORTED;
#else
    int wait_status;
    pid_t result = waitpid(process->pid, &wait_status, 0);
    
    if (result == -1) {
        return WYN_SYS_PROCESS_ERROR;
    }
    
    process->running = false;
    
    if (status) {
        if (WIFEXITED(wait_status)) {
            status->success = (WEXITSTATUS(wait_status) == 0);
            status->code = WEXITSTATUS(wait_status);
            status->terminated_by_signal = false;
            status->signal = 0;
        } else if (WIFSIGNALED(wait_status)) {
            status->success = false;
            status->code = -1;
            status->terminated_by_signal = true;
            status->signal = WTERMSIG(wait_status);
        }
    }
    
    return WYN_SYS_OK;
#endif
}

WynSystemError wyn_sys_kill_process(WynProcess* process) {
    if (!process) return WYN_SYS_INVALID_ARGUMENT;
    
    int result = kill(process->pid, SIGKILL);
    return result == 0 ? WYN_SYS_OK : WYN_SYS_PROCESS_ERROR;
}

WynSystemError wyn_sys_terminate_process(WynProcess* process) {
    if (!process) return WYN_SYS_INVALID_ARGUMENT;
    
    int result = kill(process->pid, SIGTERM);
    return result == 0 ? WYN_SYS_OK : WYN_SYS_PROCESS_ERROR;
}

void wyn_sys_free_process(WynProcess* process) {
    if (!process) return;
    
#ifdef _WIN32
    // Windows stub - process functions not implemented
#else
    if (process->stdin_fd >= 0) close(process->stdin_fd);
    if (process->stdout_fd >= 0) close(process->stdout_fd);
    if (process->stderr_fd >= 0) close(process->stderr_fd);
#endif
    
    free(process);
}

// Process I/O operations
ssize_t wyn_sys_process_write_stdin(WynProcess* process, const void* data, size_t size) {
    if (!process) return -1;
#ifdef _WIN32
    // Windows stub - not implemented
    return -1;
#else
    if (process->stdin_fd < 0) return -1;
    return write(process->stdin_fd, data, size);
#endif
}

ssize_t wyn_sys_process_read_stdout(WynProcess* process, void* buffer, size_t size) {
    if (!process) return -1;
#ifdef _WIN32
    // Windows stub - not implemented
    return -1;
#else
    if (process->stdout_fd < 0) return -1;
    return read(process->stdout_fd, buffer, size);
#endif
}

ssize_t wyn_sys_process_read_stderr(WynProcess* process, void* buffer, size_t size) {
    if (!process) return -1;
#ifdef _WIN32
    // Windows stub - not implemented
    return -1;
#else
    if (process->stderr_fd < 0) return -1;
    return read(process->stderr_fd, buffer, size);
#endif
}

WynSystemError wyn_sys_process_close_stdin(WynProcess* process) {
    if (!process) return WYN_SYS_INVALID_ARGUMENT;
    
#ifdef _WIN32
    // Windows stub - not implemented
    return WYN_SYS_NOT_SUPPORTED;
#else
    if (process->stdin_fd < 0) return WYN_SYS_INVALID_ARGUMENT;
    close(process->stdin_fd);
    process->stdin_fd = -1;
    return WYN_SYS_OK;
#endif
}

// Signal handling
WynSystemError wyn_sys_set_signal_handler(int signal, WynSignalHandler handler) {
    if (!handler) return WYN_SYS_INVALID_ARGUMENT;
    
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    int result = sigaction(signal, &sa, NULL);
    return result == 0 ? WYN_SYS_OK : WYN_SYS_UNKNOWN_ERROR;
}

WynSystemError wyn_sys_ignore_signal(int signal) {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    int result = sigaction(signal, &sa, NULL);
    return result == 0 ? WYN_SYS_OK : WYN_SYS_UNKNOWN_ERROR;
}

WynSystemError wyn_sys_default_signal(int signal) {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    int result = sigaction(signal, &sa, NULL);
    return result == 0 ? WYN_SYS_OK : WYN_SYS_UNKNOWN_ERROR;
}

WynSystemError wyn_sys_send_signal(pid_t pid, int signal) {
    int result = kill(pid, signal);
    return result == 0 ? WYN_SYS_OK : WYN_SYS_PROCESS_ERROR;
}

// System information
WynSystemInfo* wyn_sys_get_system_info(void) {
    WynSystemInfo* info = malloc(sizeof(WynSystemInfo));
    if (!info) return NULL;
    
    memset(info, 0, sizeof(WynSystemInfo));
    
    // Get system information using uname
    struct utsname uname_info;
    if (uname(&uname_info) == 0) {
        info->os_name = strdup(uname_info.sysname);
        info->os_version = strdup(uname_info.release);
        info->arch = strdup(uname_info.machine);
        info->hostname = strdup(uname_info.nodename);
    }
    
    // Get memory information (simplified)
    info->total_memory = 0;
    info->available_memory = 0;
    
#ifdef __APPLE__
    // macOS-specific memory info
    size_t size = sizeof(uint64_t);
    uint64_t memsize;
    if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) == 0) {
        info->total_memory = memsize;
        info->available_memory = memsize / 2;  // Simplified
    }
#endif
    
    // Get CPU count
    info->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    
    return info;
}

void wyn_sys_free_system_info(WynSystemInfo* info) {
    if (!info) return;
    
    free(info->os_name);
    free(info->os_version);
    free(info->arch);
    free(info->hostname);
    free(info);
}

size_t wyn_sys_get_page_size(void) {
    return getpagesize();
}

uint64_t wyn_sys_get_uptime_ms(void) {
    // Simplified implementation - would need platform-specific code
    return 0;
}

// File system utilities
bool wyn_sys_file_exists(const char* path) {
    if (!path) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES);
#else
    return access(path, F_OK) == 0;
#endif
}

bool wyn_sys_is_directory(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool wyn_sys_is_file(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISREG(st.st_mode);
    }
    return false;
}

WynSystemError wyn_sys_create_directory(const char* path) {
    if (!path) return WYN_SYS_INVALID_ARGUMENT;
    
    int result = mkdir(path, 0755);
    if (result == 0) return WYN_SYS_OK;
    
    switch (errno) {
        case EEXIST: return WYN_SYS_OK;  // Directory already exists
        case ENOENT: return WYN_SYS_NOT_FOUND;
        case EACCES: return WYN_SYS_PERMISSION_DENIED;
        default: return WYN_SYS_UNKNOWN_ERROR;
    }
}

WynSystemError wyn_sys_remove_file(const char* path) {
    if (!path) return WYN_SYS_INVALID_ARGUMENT;
    
    int result = unlink(path);
    if (result == 0) return WYN_SYS_OK;
    
    switch (errno) {
        case ENOENT: return WYN_SYS_NOT_FOUND;
        case EACCES: return WYN_SYS_PERMISSION_DENIED;
        default: return WYN_SYS_UNKNOWN_ERROR;
    }
}

WynSystemError wyn_sys_remove_directory(const char* path) {
    if (!path) return WYN_SYS_INVALID_ARGUMENT;
    
    int result = rmdir(path);
    if (result == 0) return WYN_SYS_OK;
    
    switch (errno) {
        case ENOENT: return WYN_SYS_NOT_FOUND;
        case EACCES: return WYN_SYS_PERMISSION_DENIED;
        default: return WYN_SYS_UNKNOWN_ERROR;
    }
}

// Path utilities
char* wyn_sys_join_path(const char* base, const char* component) {
    if (!base || !component) return NULL;
    
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    bool need_separator = (base_len > 0 && base[base_len - 1] != WYN_SYS_PATH_SEPARATOR);
    
    size_t total_len = base_len + comp_len + (need_separator ? 1 : 0) + 1;
    char* result = malloc(total_len);
    if (!result) return NULL;
    
    strcpy(result, base);
    if (need_separator) {
        strcat(result, WYN_SYS_PATH_SEPARATOR_STR);
    }
    strcat(result, component);
    
    return result;
}

char* wyn_sys_absolute_path(const char* path) {
    if (!path) return NULL;
    return realpath(path, NULL);
}

char* wyn_sys_basename(const char* path) {
    if (!path) return NULL;
    
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char* base = basename(path_copy);
    char* result = strdup(base);
    free(path_copy);
    
    return result;
}

char* wyn_sys_dirname(const char* path) {
    if (!path) return NULL;
    
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char* dir = dirname(path_copy);
    char* result = strdup(dir);
    free(path_copy);
    
    return result;
}

// Error handling
const char* wyn_sys_error_string(WynSystemError error) {
    switch (error) {
        case WYN_SYS_OK: return "Success";
        case WYN_SYS_NOT_FOUND: return "Not found";
        case WYN_SYS_PERMISSION_DENIED: return "Permission denied";
        case WYN_SYS_INVALID_ARGUMENT: return "Invalid argument";
        case WYN_SYS_PROCESS_ERROR: return "Process error";
        case WYN_SYS_MEMORY_ERROR: return "Memory error";
        case WYN_SYS_IO_ERROR: return "I/O error";
        case WYN_SYS_UNKNOWN_ERROR: return "Unknown error";
        default: return "Invalid error code";
    }
}
