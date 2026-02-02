// Wyn Runtime Library - Public API
// C ABI compatible for easy FFI and self-hosting

#ifndef WYN_RUNTIME_H
#define WYN_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

// ============================================================================
// System Module
// ============================================================================

// Get command line argument count
int wyn_system_argc(void);

// Get command line argument by index (returns NULL if out of bounds)
const char* wyn_system_argv(int index);

// Get environment variable (returns NULL if not found)
const char* wyn_system_env(const char* name);

// Set environment variable (returns 0 on success, -1 on error)
int wyn_system_set_env(const char* name, const char* value);

// Execute command and return exit code
int wyn_system_exec(const char* command);

// ============================================================================
// File Module
// ============================================================================

// Read entire file into string (returns NULL on error, caller must free)
char* wyn_file_read(const char* path);

// Write string to file (returns 0 on success, -1 on error)
int wyn_file_write(const char* path, const char* content);

// Check if file exists (returns 1 if exists, 0 if not)
int wyn_file_exists(const char* path);

// Check if path is a file (returns 1 if file, 0 if not)
int wyn_file_is_file(const char* path);

// Check if path is a directory (returns 1 if directory, 0 if not)
int wyn_file_is_dir(const char* path);

// Path operations
char* wyn_file_join(const char* a, const char* b);
char* wyn_file_basename(const char* path);
char* wyn_file_dirname(const char* path);
char* wyn_file_extension(const char* path);
char* wyn_file_cwd(void);

// ============================================================================
// Time Module
// ============================================================================

// Get current timestamp in milliseconds since epoch
int64_t wyn_time_now(void);

// Sleep for specified milliseconds
void wyn_time_sleep(int64_t ms);

// ============================================================================
// Array Module (Core)
// ============================================================================

// Array structure (opaque to Wyn code)
typedef struct {
    void* data;
    size_t length;
    size_t capacity;
    size_t element_size;
} WynArray;

// Create array
WynArray* wyn_array_new(size_t element_size);

// Push element
void wyn_array_push(WynArray* arr, const void* element);

// Pop element (returns 1 if success, 0 if empty)
int wyn_array_pop(WynArray* arr, void* out);

// Get element at index
void* wyn_array_get(WynArray* arr, size_t index);

// Array length
size_t wyn_array_len(WynArray* arr);

// Free array
void wyn_array_free(WynArray* arr);

#endif // WYN_RUNTIME_H
