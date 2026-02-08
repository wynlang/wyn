#!/bin/bash
# Patch codegen.c to add new stdlib functions

cd "$(dirname "$0")"

echo "Adding stdlib functions to codegen.c..."

# Find the emit_runtime_functions() in codegen.c
CODEGEN="../src/codegen.c"

if [ ! -f "$CODEGEN" ]; then
    echo "Error: codegen.c not found"
    exit 1
fi

# Create backup
cp "$CODEGEN" "$CODEGEN.backup"

# Add the new functions before the closing brace of emit_runtime_functions()
# We'll add them after System_set_env

cat >> "$CODEGEN" << 'EOF'

// Process API additions
void emit_process_stdlib() {
    emit("typedef struct { int exit_code; char* stdout_str; char* stderr_str; int timeout; } ProcessResult;\n\n");
    
    emit("ProcessResult Process_exec(const char* cmd, char** args, int arg_count) {\n");
    emit("    ProcessResult result = {0, \"\", \"\", 0};\n");
    emit("    char full_cmd[4096]; snprintf(full_cmd, sizeof(full_cmd), \"%s\", cmd);\n");
    emit("    for (int i = 0; i < arg_count; i++) { strcat(full_cmd, \" \"); strcat(full_cmd, args[i]); }\n");
    emit("    FILE* pipe = popen(full_cmd, \"r\");\n");
    emit("    if (!pipe) { result.exit_code = -1; return result; }\n");
    emit("    char* output = malloc(65536); output[0] = 0; char buffer[1024];\n");
    emit("    while (fgets(buffer, sizeof(buffer), pipe)) strcat(output, buffer);\n");
    emit("    result.exit_code = pclose(pipe);\n");
    emit("    #ifndef _WIN32\n result.exit_code = WEXITSTATUS(result.exit_code);\n #endif\n");
    emit("    result.stdout_str = output; return result;\n}\n\n");
    
    emit("ProcessResult Process_exec_timeout(const char* cmd, char** args, int arg_count, int timeout_ms) {\n");
    emit("    ProcessResult result = {0, \"\", \"\", 0}; char full_cmd[4096];\n");
    emit("    int timeout_sec = timeout_ms / 1000; if (timeout_sec < 1) timeout_sec = 1;\n");
    emit("    #ifndef _WIN32\n snprintf(full_cmd, sizeof(full_cmd), \"timeout %ds %s\", timeout_sec, cmd);\n");
    emit("    #else\n snprintf(full_cmd, sizeof(full_cmd), \"%s\", cmd);\n #endif\n");
    emit("    for (int i = 0; i < arg_count; i++) { strcat(full_cmd, \" \"); strcat(full_cmd, args[i]); }\n");
    emit("    FILE* pipe = popen(full_cmd, \"r\");\n");
    emit("    if (!pipe) { result.exit_code = -1; return result; }\n");
    emit("    char* output = malloc(65536); output[0] = 0; char buffer[1024];\n");
    emit("    while (fgets(buffer, sizeof(buffer), pipe)) strcat(output, buffer);\n");
    emit("    result.exit_code = pclose(pipe);\n");
    emit("    #ifndef _WIN32\n if (WEXITSTATUS(result.exit_code) == 124) result.timeout = 1;\n");
    emit("    result.exit_code = WEXITSTATUS(result.exit_code);\n #endif\n");
    emit("    result.stdout_str = output; return result;\n}\n\n");
}

void emit_fs_stdlib() {
    emit("#include <dirent.h>\n#include <sys/stat.h>\n\n");
    emit("typedef struct { char** files; int count; } DirListing;\n\n");
    
    emit("DirListing Fs_read_dir(const char* path) {\n");
    emit("    DirListing listing = {NULL, 0}; DIR* dir = opendir(path);\n");
    emit("    if (!dir) return listing; struct dirent* entry; int count = 0;\n");
    emit("    while ((entry = readdir(dir)) != NULL)\n");
    emit("        if (strcmp(entry->d_name, \".\") != 0 && strcmp(entry->d_name, \"..\") != 0) count++;\n");
    emit("    listing.files = malloc(count * sizeof(char*)); listing.count = count;\n");
    emit("    rewinddir(dir); int i = 0;\n");
    emit("    while ((entry = readdir(dir)) != NULL && i < count)\n");
    emit("        if (strcmp(entry->d_name, \".\") != 0 && strcmp(entry->d_name, \"..\") != 0)\n");
    emit("            listing.files[i++] = strdup(entry->d_name);\n");
    emit("    closedir(dir); return listing;\n}\n\n");
    
    emit("int Fs_exists(const char* path) { struct stat st; return stat(path, &st) == 0; }\n");
    emit("int Fs_is_file(const char* path) { struct stat st; if (stat(path, &st) != 0) return 0; return S_ISREG(st.st_mode); }\n");
    emit("int Fs_is_dir(const char* path) { struct stat st; if (stat(path, &st) != 0) return 0; return S_ISDIR(st.st_mode); }\n\n");
}

void emit_time_stdlib() {
    emit("#include <sys/time.h>\n\n");
    emit("long Time_now() { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec * 1000 + tv.tv_usec / 1000; }\n");
    emit("void Time_sleep(int ms) {\n #ifdef _WIN32\n Sleep(ms);\n #else\n usleep(ms * 1000);\n #endif\n}\n\n");
}

EOF

echo "✓ Stdlib functions added to codegen.c"
echo ""
echo "Note: You need to call these functions in emit_runtime_functions():"
echo "  - emit_process_stdlib();"
echo "  - emit_fs_stdlib();"
echo "  - emit_time_stdlib();"
echo ""
echo "Backup saved to: $CODEGEN.backup"
