// Stdlib additions for parallel testing
// Add these functions to codegen.c in emit_runtime_functions()

// Process API
void emit_process_api() {
    // Process::exec - execute command and return result
    emit("typedef struct {\n");
    emit("    int exit_code;\n");
    emit("    char* stdout_str;\n");
    emit("    char* stderr_str;\n");
    emit("    int timeout;\n");
    emit("} ProcessResult;\n\n");
    
    emit("ProcessResult Process_exec(const char* cmd, char** args, int arg_count) {\n");
    emit("    ProcessResult result = {0, \"\", \"\", 0};\n");
    emit("    char full_cmd[4096];\n");
    emit("    snprintf(full_cmd, sizeof(full_cmd), \"%s\", cmd);\n");
    emit("    for (int i = 0; i < arg_count; i++) {\n");
    emit("        strcat(full_cmd, \" \");\n");
    emit("        strcat(full_cmd, args[i]);\n");
    emit("    }\n");
    emit("    FILE* pipe = popen(full_cmd, \"r\");\n");
    emit("    if (!pipe) {\n");
    emit("        result.exit_code = -1;\n");
    emit("        return result;\n");
    emit("    }\n");
    emit("    char* output = malloc(65536);\n");
    emit("    output[0] = 0;\n");
    emit("    char buffer[1024];\n");
    emit("    while (fgets(buffer, sizeof(buffer), pipe)) {\n");
    emit("        strcat(output, buffer);\n");
    emit("    }\n");
    emit("    result.exit_code = pclose(pipe);\n");
    emit("    #ifndef _WIN32\n");
    emit("    result.exit_code = WEXITSTATUS(result.exit_code);\n");
    emit("    #endif\n");
    emit("    result.stdout_str = output;\n");
    emit("    return result;\n");
    emit("}\n\n");
    
    // Process::exec_timeout - with timeout support
    emit("ProcessResult Process_exec_timeout(const char* cmd, char** args, int arg_count, int timeout_ms) {\n");
    emit("    ProcessResult result = {0, \"\", \"\", 0};\n");
    emit("    char full_cmd[4096];\n");
    emit("    int timeout_sec = timeout_ms / 1000;\n");
    emit("    if (timeout_sec < 1) timeout_sec = 1;\n");
    emit("    #ifndef _WIN32\n");
    emit("    snprintf(full_cmd, sizeof(full_cmd), \"timeout %ds %s\", timeout_sec, cmd);\n");
    emit("    #else\n");
    emit("    snprintf(full_cmd, sizeof(full_cmd), \"%s\", cmd);\n");
    emit("    #endif\n");
    emit("    for (int i = 0; i < arg_count; i++) {\n");
    emit("        strcat(full_cmd, \" \");\n");
    emit("        strcat(full_cmd, args[i]);\n");
    emit("    }\n");
    emit("    FILE* pipe = popen(full_cmd, \"r\");\n");
    emit("    if (!pipe) {\n");
    emit("        result.exit_code = -1;\n");
    emit("        return result;\n");
    emit("    }\n");
    emit("    char* output = malloc(65536);\n");
    emit("    output[0] = 0;\n");
    emit("    char buffer[1024];\n");
    emit("    while (fgets(buffer, sizeof(buffer), pipe)) {\n");
    emit("        strcat(output, buffer);\n");
    emit("    }\n");
    emit("    result.exit_code = pclose(pipe);\n");
    emit("    #ifndef _WIN32\n");
    emit("    if (WEXITSTATUS(result.exit_code) == 124) {\n");
    emit("        result.timeout = 1;\n");
    emit("    }\n");
    emit("    result.exit_code = WEXITSTATUS(result.exit_code);\n");
    emit("    #endif\n");
    emit("    result.stdout_str = output;\n");
    emit("    return result;\n");
    emit("}\n\n");
}

// Filesystem API
void emit_fs_api() {
    emit("#include <dirent.h>\n");
    emit("#include <sys/stat.h>\n\n");
    
    // Fs::read_dir - list directory contents
    emit("typedef struct {\n");
    emit("    char** files;\n");
    emit("    int count;\n");
    emit("} DirListing;\n\n");
    
    emit("DirListing Fs_read_dir(const char* path) {\n");
    emit("    DirListing listing = {NULL, 0};\n");
    emit("    DIR* dir = opendir(path);\n");
    emit("    if (!dir) return listing;\n");
    emit("    \n");
    emit("    // Count entries first\n");
    emit("    struct dirent* entry;\n");
    emit("    int count = 0;\n");
    emit("    while ((entry = readdir(dir)) != NULL) {\n");
    emit("        if (strcmp(entry->d_name, \".\") != 0 && strcmp(entry->d_name, \"..\") != 0) {\n");
    emit("            count++;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Allocate array\n");
    emit("    listing.files = malloc(count * sizeof(char*));\n");
    emit("    listing.count = count;\n");
    emit("    \n");
    emit("    // Read entries\n");
    emit("    rewinddir(dir);\n");
    emit("    int i = 0;\n");
    emit("    while ((entry = readdir(dir)) != NULL && i < count) {\n");
    emit("        if (strcmp(entry->d_name, \".\") != 0 && strcmp(entry->d_name, \"..\") != 0) {\n");
    emit("            listing.files[i] = strdup(entry->d_name);\n");
    emit("            i++;\n");
    emit("        }\n");
    emit("    }\n");
    emit("    \n");
    emit("    closedir(dir);\n");
    emit("    return listing;\n");
    emit("}\n\n");
    
    // Fs::exists
    emit("int Fs_exists(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    return stat(path, &st) == 0;\n");
    emit("}\n\n");
    
    // Fs::is_file
    emit("int Fs_is_file(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    if (stat(path, &st) != 0) return 0;\n");
    emit("    return S_ISREG(st.st_mode);\n");
    emit("}\n\n");
    
    // Fs::is_dir
    emit("int Fs_is_dir(const char* path) {\n");
    emit("    struct stat st;\n");
    emit("    if (stat(path, &st) != 0) return 0;\n");
    emit("    return S_ISDIR(st.st_mode);\n");
    emit("}\n\n");
    
    // Fs::read_file - read entire file as string
    emit("const char* Fs_read_file(const char* path) {\n");
    emit("    FILE* f = fopen(path, \"r\");\n");
    emit("    if (!f) return \"\";\n");
    emit("    \n");
    emit("    // Get file size\n");
    emit("    fseek(f, 0, SEEK_END);\n");
    emit("    long size = ftell(f);\n");
    emit("    fseek(f, 0, SEEK_SET);\n");
    emit("    \n");
    emit("    // Allocate buffer\n");
    emit("    char* buffer = malloc(size + 1);\n");
    emit("    if (!buffer) {\n");
    emit("        fclose(f);\n");
    emit("        return \"\";\n");
    emit("    }\n");
    emit("    \n");
    emit("    // Read file\n");
    emit("    size_t read = fread(buffer, 1, size, f);\n");
    emit("    buffer[read] = '\\\\0';\n");
    emit("    fclose(f);\n");
    emit("    \n");
    emit("    return buffer;\n");
    emit("}\n\n");
}

// Time API
void emit_time_api() {
    emit("#include <sys/time.h>\n\n");
    
    // Time::now - milliseconds since epoch
    emit("long Time_now() {\n");
    emit("    struct timeval tv;\n");
    emit("    gettimeofday(&tv, NULL);\n");
    emit("    return tv.tv_sec * 1000 + tv.tv_usec / 1000;\n");
    emit("}\n\n");
    
    // Time::sleep - sleep for milliseconds
    emit("void Time_sleep(int ms) {\n");
    emit("    #ifdef _WIN32\n");
    emit("    Sleep(ms);\n");
    emit("    #else\n");
    emit("    usleep(ms * 1000);\n");
    emit("    #endif\n");
    emit("}\n\n");
}

// Task API (high-level wrapper over spawn)
void emit_task_api() {
    emit("// Task API - high-level wrapper over spawn\n");
    emit("typedef struct {\n");
    emit("    void* result;\n");
    emit("    int ready;\n");
    emit("    pthread_mutex_t lock;\n");
    emit("} WynTaskHandle;\n\n");
    
    emit("typedef struct {\n");
    emit("    void* (*func)(void*);\n");
    emit("    void* arg;\n");
    emit("    WynTaskHandle* handle;\n");
    emit("} WynTaskWrapper;\n\n");
    
    emit("void* task_wrapper_func(void* arg) {\n");
    emit("    WynTaskWrapper* wrapper = (WynTaskWrapper*)arg;\n");
    emit("    void* result = wrapper->func(wrapper->arg);\n");
    emit("    pthread_mutex_lock(&wrapper->handle->lock);\n");
    emit("    wrapper->handle->result = result;\n");
    emit("    wrapper->handle->ready = 1;\n");
    emit("    pthread_mutex_unlock(&wrapper->handle->lock);\n");
    emit("    free(wrapper);\n");
    emit("    return NULL;\n");
    emit("}\n\n");
    
    emit("WynTaskHandle* Task_spawn(void* (*func)(void*), void* arg) {\n");
    emit("    WynTaskHandle* handle = malloc(sizeof(WynTaskHandle));\n");
    emit("    handle->result = NULL;\n");
    emit("    handle->ready = 0;\n");
    emit("    pthread_mutex_init(&handle->lock, NULL);\n");
    emit("    \n");
    emit("    WynTaskWrapper* wrapper = malloc(sizeof(WynTaskWrapper));\n");
    emit("    wrapper->func = func;\n");
    emit("    wrapper->arg = arg;\n");
    emit("    wrapper->handle = handle;\n");
    emit("    \n");
    emit("    wyn_spawn(task_wrapper_func, wrapper);\n");
    emit("    return handle;\n");
    emit("}\n\n");
    
    emit("void* Task_await(WynTaskHandle* handle) {\n");
    emit("    while (1) {\n");
    emit("        pthread_mutex_lock(&handle->lock);\n");
    emit("        if (handle->ready) {\n");
    emit("            void* result = handle->result;\n");
    emit("            pthread_mutex_unlock(&handle->lock);\n");
    emit("            pthread_mutex_destroy(&handle->lock);\n");
    emit("            free(handle);\n");
    emit("            return result;\n");
    emit("        }\n");
    emit("        pthread_mutex_unlock(&handle->lock);\n");
    emit("        usleep(100);\n");
    emit("    }\n");
    emit("}\n\n");
    
    emit("int Task_is_ready(WynTaskHandle* handle) {\n");
    emit("    pthread_mutex_lock(&handle->lock);\n");
    emit("    int ready = handle->ready;\n");
    emit("    pthread_mutex_unlock(&handle->lock);\n");
    emit("    return ready;\n");
    emit("}\n\n");
}
