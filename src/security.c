#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

// Global memory tracking variables
MemoryBlock* memory_blocks = NULL;
size_t total_allocated = 0;
bool memory_tracking_enabled = false;

// Initialize memory tracking system
void init_memory_tracking(void) {
    memory_blocks = NULL;
    total_allocated = 0;
    memory_tracking_enabled = true;
}

// Cleanup memory tracking and report leaks
void cleanup_memory_tracking(void) {
    if (!memory_tracking_enabled) return;
    
    if (has_memory_leaks()) {
        print_memory_leaks();
    }
    
    // Free all tracked blocks
    MemoryBlock* current = memory_blocks;
    while (current) {
        MemoryBlock* next = current->next;
        free(current->ptr);
        free(current);
        current = next;
    }
    
    memory_blocks = NULL;
    total_allocated = 0;
    memory_tracking_enabled = false;
}

// Add memory block to tracking list
static void track_allocation(void* ptr, size_t size, const char* file, int line) {
    if (!memory_tracking_enabled || !ptr) return;
    
    MemoryBlock* block = malloc(sizeof(MemoryBlock));
    if (!block) return; // Can't track, but don't fail the allocation
    
    block->ptr = ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->next = memory_blocks;
    memory_blocks = block;
    total_allocated += size;
}

// Remove memory block from tracking list
static void untrack_allocation(void* ptr) {
    if (!memory_tracking_enabled || !ptr) return;
    
    MemoryBlock** current = &memory_blocks;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemoryBlock* to_remove = *current;
            *current = (*current)->next;
            total_allocated -= to_remove->size;
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
}

// Safe malloc with overflow checking and tracking
void* safe_malloc_checked(size_t size, const char* file, int line) {
    // Check for zero size
    if (size == 0) {
        report_security_error("INVALID_SIZE", "Attempted to allocate zero bytes", file, line);
        return NULL;
    }
    
    // Check for excessive size
    if (!is_safe_allocation_size(size)) {
        report_security_error("EXCESSIVE_ALLOCATION", "Allocation size exceeds safety limit", file, line);
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (!ptr) {
        report_security_error("ALLOCATION_FAILED", "Memory allocation failed", file, line);
        return NULL;
    }
    
    // Initialize memory to zero for security
    memset(ptr, 0, size);
    
    track_allocation(ptr, size, file, line);
    return ptr;
}

// Safe calloc with overflow checking
void* safe_calloc_checked(size_t nmemb, size_t size, const char* file, int line) {
    // Check for overflow in multiplication
    size_t total_size;
    if (!check_mul_overflow_size_t(nmemb, size, &total_size)) {
        report_security_error("INTEGER_OVERFLOW", "Integer overflow in calloc", file, line);
        return NULL;
    }
    
    return safe_malloc_checked(total_size, file, line);
}

// Safe realloc with overflow checking
void* safe_realloc_checked(void* ptr, size_t size, const char* file, int line) {
    if (size == 0) {
        safe_free_checked(ptr, file, line);
        return NULL;
    }
    
    if (!is_safe_allocation_size(size)) {
        report_security_error("EXCESSIVE_ALLOCATION", "Realloc size exceeds safety limit", file, line);
        return NULL;
    }
    
    // If ptr is NULL, behave like malloc
    if (!ptr) {
        return safe_malloc_checked(size, file, line);
    }
    
    untrack_allocation(ptr);
    
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        report_security_error("REALLOC_FAILED", "Memory reallocation failed", file, line);
        // Re-track the original pointer since realloc failed
        track_allocation(ptr, 0, file, line); // Size unknown
        return NULL;
    }
    
    track_allocation(new_ptr, size, file, line);
    return new_ptr;
}

// Safe free with tracking
void safe_free_checked(void* ptr, const char* file __attribute__((unused)), int line __attribute__((unused))) {
    if (!ptr) return;
    
    untrack_allocation(ptr);
    free(ptr);
}

// Safe string duplication
char* safe_strdup_checked(const char* str, const char* file, int line) {
    if (!str) {
        report_security_error("NULL_STRING", "Attempted to duplicate NULL string", file, line);
        return NULL;
    }
    
    size_t len = strlen(str);
    if (!is_safe_string_length(len)) {
        report_security_error("STRING_TOO_LONG", "String exceeds maximum safe length", file, line);
        return NULL;
    }
    
    char* copy = safe_malloc_checked(len + 1, file, line);
    if (!copy) return NULL;
    
    memcpy(copy, str, len + 1);
    return copy;
}

// Safe string copy with bounds checking
int safe_strcpy(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        errno = ERANGE;
        return -1;
    }
    
    memcpy(dest, src, src_len + 1);
    return 0;
}

// Safe string concatenation with bounds checking
int safe_strcat(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        errno = ERANGE;
        return -1;
    }
    
    memcpy(dest + dest_len, src, src_len + 1);
    return 0;
}

// Safe snprintf wrapper
int safe_snprintf(char* str, size_t size, const char* format, ...) {
    if (!str || !format || size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    
    // Ensure null termination
    if (result >= 0 && (size_t)result < size) {
        str[result] = '\0';
    } else {
        str[size - 1] = '\0';
        return -1;
    }
    
    return result;
}

// Check for size_t multiplication overflow
bool check_mul_overflow_size_t(size_t a, size_t b, size_t* result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    
    if (a > SIZE_MAX / b) {
        return false; // Overflow would occur
    }
    
    *result = a * b;
    return true;
}

// Check for size_t addition overflow
bool check_add_overflow_size_t(size_t a, size_t b, size_t* result) {
    if (a > SIZE_MAX - b) {
        return false; // Overflow would occur
    }
    
    *result = a + b;
    return true;
}

// Check for int multiplication overflow
bool check_mul_overflow_int(int a, int b, int* result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    
    if ((a > 0 && b > 0 && a > INT_MAX / b) ||
        (a < 0 && b < 0 && a < INT_MAX / b) ||
        (a > 0 && b < 0 && b < INT_MIN / a) ||
        (a < 0 && b > 0 && a < INT_MIN / b)) {
        return false;
    }
    
    *result = a * b;
    return true;
}

// Validate buffer pointer and size
bool validate_buffer(const void* buf, size_t size) {
    return buf != NULL && size > 0 && size <= MAX_SAFE_ALLOCATION_SIZE;
}

// Validate string pointer and length
bool validate_string(const char* str, size_t max_len) {
    if (!str) return false;
    
    size_t len = strnlen(str, max_len + 1);
    return len <= max_len;
}

// Check if allocation size is safe
bool is_safe_allocation_size(size_t size) {
    return size > 0 && size <= MAX_SAFE_ALLOCATION_SIZE;
}

// Check if string length is safe
bool is_safe_string_length(size_t len) {
    return len <= MAX_SAFE_STRING_LENGTH;
}

// Get total allocated memory
size_t get_allocated_memory(void) {
    return total_allocated;
}

// Get number of allocations
size_t get_allocation_count(void) {
    size_t count = 0;
    MemoryBlock* current = memory_blocks;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

// Check if there are memory leaks
bool has_memory_leaks(void) {
    return memory_blocks != NULL;
}

// Print memory leak report
void print_memory_leaks(void) {
    if (!memory_blocks) {
        printf("✅ No memory leaks detected\n");
        return;
    }
    
    printf("🚨 MEMORY LEAKS DETECTED:\n");
    printf("========================\n");
    
    size_t leak_count = 0;
    size_t total_leaked = 0;
    
    MemoryBlock* current = memory_blocks;
    while (current) {
        printf("Leak #%zu: %zu bytes at %p\n", ++leak_count, current->size, current->ptr);
        printf("  Allocated at: %s:%d\n", current->file, current->line);
        total_leaked += current->size;
        current = current->next;
    }
    
    printf("========================\n");
    printf("Total leaks: %zu allocations, %zu bytes\n", leak_count, total_leaked);
}

// Report security error
void report_security_error(const char* type, const char* message, const char* file, int line) {
    fprintf(stderr, "🚨 SECURITY ERROR [%s]: %s\n", type, message);
    fprintf(stderr, "   Location: %s:%d\n", file, line);
}

// Validate pointer (basic check)
bool is_valid_pointer(const void* ptr) {
    return ptr != NULL;
}
