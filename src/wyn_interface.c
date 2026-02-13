// C interface functions for Wyn compiler
// These functions provide file I/O and argument access to Wyn programs

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#include <sys/socket.h>
#endif
#include "io.h"  // Use existing file functions

// Global storage for command line arguments
static int global_argc = 0;
static char** global_argv = NULL;

// Global storage for file operations (extern for generated code)
char* global_filename = NULL;
char* global_file_content = NULL;
static int global_filename_valid = 0;
static int global_content_valid = 0;

// Initialize command line arguments (called from C main)
void wyn_init_args(int argc, char** argv) {
    global_argc = argc;
    global_argv = argv;
}

// Get argument count
int wyn_get_argc(void) {
    return global_argc;
}

// Get argument by index
const char* wyn_get_argv(int index) {
    if (index < 0 || index >= global_argc) {
        return "";
    }
    return global_argv[index];
}

// Read file contents
char* wyn_read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return NULL;  // Return NULL on error
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    
    return buffer;
}

// Store argument in global and return validity
int wyn_store_argv(int index) {
    if (index < 0 || index >= global_argc) {
        global_filename_valid = 0;
        return 0;
    }
    global_filename = global_argv[index];
    global_filename_valid = 1;
    return 1;
}

// Get stored filename validity
int wyn_get_filename_valid(void) {
    return global_filename_valid;
}

// Store file content in global and return validity
int wyn_store_file_content(const char* path) {
    if (global_file_content) {
        free(global_file_content);
        global_file_content = NULL;
    }
    
    global_file_content = wyn_read_file(path);
    global_content_valid = (global_file_content != NULL);
    return global_content_valid;
}

// Get stored content validity
int wyn_get_content_valid(void) {
    return global_content_valid;
}

// Write file contents
int wyn_write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) {
        return 0;  // Failed
    }
    
    fprintf(f, "%s", content);
    fclose(f);
    return 1;  // Success
}


// Stub implementations for unimplemented v1.4 features
// TODO: Implement these properly
int _write(const char* path, const char* content) {
    return wyn_write_file(path, content);
}

int _exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

char* _read(const char* path) {
    return wyn_read_file(path);
}

long _file_size(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

char* _get_cwd(void) {
    static char cwd[1024];
    #ifdef _WIN32
    _getcwd(cwd, sizeof(cwd));
    #else
    getcwd(cwd, sizeof(cwd));
    #endif
    return cwd;
}

long _now(void) {
    return (long)time(NULL);
}

// Additional stubs for file explorer example
struct WynArray { void* data; int count; int capacity; };

struct WynArray _list_dir(const char* path) {
    (void)path;
    struct WynArray arr = {0};
    return arr;
}

int _is_dir(const char* path) {
    #ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
    #else
    struct stat st;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
    #endif
}

int _is_file(const char* path) {
    return _exists(path) && !_is_dir(path);
}

const char* _extension(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

char* _path_join(const char* dir, const char* file) {
    static char result[2048];
    snprintf(result, sizeof(result), "%s/%s", dir, file);
    return result;
}

const char* _basename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    #ifdef _WIN32
    const char* last_backslash = strrchr(path, '\\');
    if (last_backslash > last_slash) last_slash = last_backslash;
    #endif
    return last_slash ? last_slash + 1 : path;
}

char* _dirname(const char* path) {
    static char result[2048];
    const char* last_slash = strrchr(path, '/');
    #ifdef _WIN32
    const char* last_backslash = strrchr(path, '\\');
    if (last_backslash > last_slash) last_slash = last_backslash;
    #endif
    if (last_slash) {
        size_t len = last_slash - path;
        memcpy(result, path, len);
        result[len] = '\0';
        return result;
    }
    return ".";
}

// Additional stubs for system_info example
const char* _env(const char* name) {
    return getenv(name);
}

int _set_env(const char* name, const char* value) {
    #ifdef _WIN32
    return _putenv_s(name, value) == 0 ? 1 : 0;
    #else
    return setenv(name, value, 1) == 0 ? 1 : 0;
    #endif
}

const char* _format(long timestamp) {
    static char buf[64];
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}

struct WynArray _args(void) {
    struct WynArray arr = {0};
    return arr;
}

int _exec_code(const char* cmd) {
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS || TARGET_OS_SIMULATOR
    return -1;
#else
    return system(cmd);
#endif
#elif defined(__ANDROID__)
    return -1;
#else
    return system(cmd);
#endif
}

// Additional stubs for http_server example
int _listen(int sockfd, int backlog) {
    #ifdef _WIN32
    return listen(sockfd, backlog);
    #else
    return listen(sockfd, backlog);
    #endif
}

void wyn_sleep(int seconds) {
    #ifdef _WIN32
    Sleep(seconds * 1000);
    #else
    sleep(seconds);
    #endif
}

int _close(int fd) {
    #ifdef _WIN32
    return closesocket(fd);
    #else
    return close(fd);
    #endif
}
