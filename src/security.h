#ifndef SECURITY_H
#define SECURITY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Memory tracking structure
typedef struct MemoryBlock {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    struct MemoryBlock* next;
} MemoryBlock;

// Global memory tracking
extern MemoryBlock* memory_blocks;
extern size_t total_allocated;
extern bool memory_tracking_enabled;

// Safe memory operations with overflow checking
void* safe_malloc_checked(size_t size, const char* file, int line);
void* safe_calloc_checked(size_t nmemb, size_t size, const char* file, int line);
void* safe_realloc_checked(void* ptr, size_t size, const char* file, int line);
void safe_free_checked(void* ptr, const char* file, int line);
char* safe_strdup_checked(const char* str, const char* file, int line);

// Macros for automatic file/line tracking
#define safe_malloc(size) safe_malloc_checked(size, __FILE__, __LINE__)
#define safe_calloc(nmemb, size) safe_calloc_checked(nmemb, size, __FILE__, __LINE__)
#define safe_realloc(ptr, size) safe_realloc_checked(ptr, size, __FILE__, __LINE__)
#define safe_free(ptr) safe_free_checked(ptr, __FILE__, __LINE__)
#define safe_strdup(str) safe_strdup_checked(str, __FILE__, __LINE__)

// Safe string operations with bounds checking
int safe_strcpy(char* dest, size_t dest_size, const char* src);
int safe_strcat(char* dest, size_t dest_size, const char* src);
int safe_snprintf(char* str, size_t size, const char* format, ...);
int safe_strncat(char* dest, size_t dest_size, const char* src, size_t n);

// Integer overflow checking
bool check_mul_overflow_size_t(size_t a, size_t b, size_t* result);
bool check_add_overflow_size_t(size_t a, size_t b, size_t* result);
bool check_mul_overflow_int(int a, int b, int* result);
bool check_add_overflow_int(int a, int b, int* result);

// Buffer validation
bool validate_buffer(const void* buf, size_t size);
bool validate_string(const char* str, size_t max_len);

// Memory tracking functions
void init_memory_tracking(void);
void cleanup_memory_tracking(void);
size_t get_allocated_memory(void);
size_t get_allocation_count(void);
void print_memory_leaks(void);
bool has_memory_leaks(void);

// Security validation functions
bool is_valid_pointer(const void* ptr);
bool is_safe_string_length(size_t len);
bool is_safe_allocation_size(size_t size);

// Error reporting
void report_security_error(const char* type, const char* message, const char* file, int line);

// Security configuration
#define MAX_SAFE_ALLOCATION_SIZE (1024 * 1024 * 100)  // 100MB max
#define MAX_SAFE_STRING_LENGTH (1024 * 1024)          // 1MB max string
#define SECURITY_CANARY_VALUE 0xDEADBEEF

#endif // SECURITY_H
