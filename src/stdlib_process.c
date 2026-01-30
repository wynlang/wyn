// Stdlib Process API Implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

typedef struct {
    int exit_code;
    char* stdout_str;
    char* stderr_str;
    int timeout;
} ProcessResult;

ProcessResult Process_exec(const char* cmd, char** args, int arg_count) {
    ProcessResult result = {0, NULL, NULL, 0};
    char full_cmd[4096];
    
    snprintf(full_cmd, sizeof(full_cmd), "%s", cmd);
    for (int i = 0; i < arg_count; i++) {
        strcat(full_cmd, " ");
        strcat(full_cmd, args[i]);
    }
    
    FILE* pipe = popen(full_cmd, "r");
    if (!pipe) {
        result.exit_code = -1;
        result.stdout_str = strdup("");
        result.stderr_str = strdup("");
        return result;
    }
    
    char* output = malloc(65536);
    output[0] = 0;
    char buffer[1024];
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strcat(output, buffer);
    }
    
    result.exit_code = pclose(pipe);
    
#ifndef _WIN32
    result.exit_code = WEXITSTATUS(result.exit_code);
#endif
    
    result.stdout_str = output;
    result.stderr_str = strdup("");
    return result;
}

ProcessResult Process_exec_timeout(const char* cmd, char** args, int arg_count, int timeout_ms) {
    ProcessResult result = {0, NULL, NULL, 0};
    char full_cmd[4096];
    
    int timeout_sec = timeout_ms / 1000;
    if (timeout_sec < 1) timeout_sec = 1;
    
#ifndef _WIN32
    snprintf(full_cmd, sizeof(full_cmd), "timeout %ds %s", timeout_sec, cmd);
#else
    snprintf(full_cmd, sizeof(full_cmd), "%s", cmd);
#endif
    
    for (int i = 0; i < arg_count; i++) {
        strcat(full_cmd, " ");
        strcat(full_cmd, args[i]);
    }
    
    FILE* pipe = popen(full_cmd, "r");
    if (!pipe) {
        result.exit_code = -1;
        result.stdout_str = strdup("");
        result.stderr_str = strdup("");
        return result;
    }
    
    char* output = malloc(65536);
    output[0] = 0;
    char buffer[1024];
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strcat(output, buffer);
    }
    
    result.exit_code = pclose(pipe);
    
#ifndef _WIN32
    if (WEXITSTATUS(result.exit_code) == 124) {
        result.timeout = 1;
    }
    result.exit_code = WEXITSTATUS(result.exit_code);
#endif
    
    result.stdout_str = output;
    result.stderr_str = strdup("");
    return result;
}
