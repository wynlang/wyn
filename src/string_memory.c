#include "string_memory.h"
#include "arc_runtime.h"
#include "wyn_string.h"
#include "safe_memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Forward declaration
void wyn_string_destructor(void* data);

// String interning table for memory optimization
#define STRING_INTERN_TABLE_SIZE 1024
static WynString* intern_table[STRING_INTERN_TABLE_SIZE];
static bool intern_table_initialized = false;

// Hash function for string interning
static uint32_t hash_string(const char* str, size_t len) {
    uint32_t hash = 2166136261u; // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= (unsigned char)str[i];
        hash *= 16777619u; // FNV prime
    }
    return hash;
}

// Initialize string interning table
static void init_intern_table(void) {
    if (intern_table_initialized) return;
    
    for (int i = 0; i < STRING_INTERN_TABLE_SIZE; i++) {
        intern_table[i] = NULL;
    }
    intern_table_initialized = true;
}

// String interning implementation
WynString* wyn_string_intern(const char* cstr) {
    if (!cstr) return NULL;
    
    init_intern_table();
    
    size_t len = strlen(cstr);
    uint32_t hash = hash_string(cstr, len);
    uint32_t index = hash % STRING_INTERN_TABLE_SIZE;
    
    // Check if string already exists in intern table
    WynString* current = intern_table[index];
    while (current) {
        if (current->length == len && memcmp(current->data, cstr, len) == 0) {
            return current; // Return existing interned string
        }
        // Linear probing for collision resolution
        index = (index + 1) % STRING_INTERN_TABLE_SIZE;
        current = intern_table[index];
        
        // Prevent infinite loop
        if (index == hash % STRING_INTERN_TABLE_SIZE) break;
    }
    
    // Create new interned string
    WynString* new_string = wyn_string_new(cstr);
    if (new_string) {
        intern_table[index] = new_string;
    }
    
    return new_string;
}

// ARC-managed string creation
WynObject* wyn_string_create_arc(const char* cstr) {
    if (!cstr) return NULL;
    
    // Allocate ARC object for string
    WynObject* obj = wyn_arc_alloc(sizeof(WynString), WYN_TYPE_STRING, wyn_string_destructor);
    if (!obj) return NULL;
    
    WynString* str = (WynString*)wyn_arc_get_data(obj);
    
    // Initialize string data
    size_t len = strlen(cstr);
    str->length = len;
    str->capacity = len + 1;
    str->data = safe_malloc(str->capacity);
    
    if (!str->data) {
        wyn_arc_release(obj);
        return NULL;
    }
    
    memcpy(str->data, cstr, len + 1);
    str->is_heap_allocated = true;
    str->hash_valid = false;
    str->hash_cache = 0;
    
    return obj;
}

// String destructor for ARC
void wyn_string_destructor(void* data) {
    WynString* str = (WynString*)data;
    if (str && str->is_heap_allocated && str->data) {
        safe_free(str->data);
        str->data = NULL;
    }
}

// ARC-managed string concatenation
WynObject* wyn_string_concat_arc(WynObject* a_obj, WynObject* b_obj) {
    if (!a_obj || !b_obj) return NULL;
    
    WynString* a = (WynString*)wyn_arc_get_data(a_obj);
    WynString* b = (WynString*)wyn_arc_get_data(b_obj);
    
    if (!a || !b) return NULL;
    
    size_t new_length = a->length + b->length;
    
    // Allocate new ARC string object
    WynObject* result_obj = wyn_arc_alloc(sizeof(WynString), WYN_TYPE_STRING, wyn_string_destructor);
    if (!result_obj) return NULL;
    
    WynString* result = (WynString*)wyn_arc_get_data(result_obj);
    
    result->length = new_length;
    result->capacity = new_length + 1;
    result->data = safe_malloc(result->capacity);
    
    if (!result->data) {
        wyn_arc_release(result_obj);
        return NULL;
    }
    
    // Copy string data
    memcpy(result->data, a->data, a->length);
    memcpy(result->data + a->length, b->data, b->length);
    result->data[new_length] = '\0';
    
    result->is_heap_allocated = true;
    result->hash_valid = false;
    result->hash_cache = 0;
    
    return result_obj;
}

// ARC-managed string copy
WynObject* wyn_string_copy_arc(WynObject* src_obj) {
    if (!src_obj) return NULL;
    
    WynString* src = (WynString*)wyn_arc_get_data(src_obj);
    if (!src) return NULL;
    
    return wyn_string_create_arc(src->data);
}

// String assignment with proper ARC management
void wyn_string_assign_arc(WynObject** dest, WynObject* src) {
    if (!dest) return;
    
    // Release old string if exists
    if (*dest) {
        wyn_arc_release(*dest);
    }
    
    // Retain new string
    *dest = src ? wyn_arc_retain(src) : NULL;
}

// Memory leak detection for strings
typedef struct StringLeak {
    WynObject* obj;
    const char* file;
    int line;
    struct StringLeak* next;
} StringLeak;

static StringLeak* leak_list = NULL;

void wyn_string_track_allocation(WynObject* obj, const char* file, int line) {
    if (!obj) return;
    
    StringLeak* leak = safe_malloc(sizeof(StringLeak));
    if (!leak) return;
    
    leak->obj = obj;
    leak->file = file;
    leak->line = line;
    leak->next = leak_list;
    leak_list = leak;
}

void wyn_string_untrack_allocation(WynObject* obj) {
    if (!obj) return;
    
    StringLeak** current = &leak_list;
    while (*current) {
        if ((*current)->obj == obj) {
            StringLeak* to_remove = *current;
            *current = (*current)->next;
            safe_free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
}

void wyn_string_check_leaks(void) {
    StringLeak* current = leak_list;
    int leak_count = 0;
    
    while (current) {
        if (wyn_arc_is_valid(current->obj)) {
            printf("String leak detected: %s:%d (ref_count: %u)\n", 
                   current->file, current->line, 
                   wyn_arc_get_ref_count(current->obj));
            leak_count++;
        }
        current = current->next;
    }
    
    if (leak_count > 0) {
        printf("Total string leaks: %d\n", leak_count);
    } else {
        printf("No string leaks detected.\n");
    }
}

// Cleanup function for string interning table
void wyn_string_cleanup_intern_table(void) {
    if (!intern_table_initialized) return;
    
    for (int i = 0; i < STRING_INTERN_TABLE_SIZE; i++) {
        if (intern_table[i]) {
            wyn_string_free(intern_table[i]);
            intern_table[i] = NULL;
        }
    }
    intern_table_initialized = false;
}

// String memory statistics
StringMemoryStats wyn_string_get_memory_stats(void) {
    StringMemoryStats stats = {0};
    
    // Count interned strings
    for (int i = 0; i < STRING_INTERN_TABLE_SIZE; i++) {
        if (intern_table[i]) {
            stats.interned_strings++;
            stats.interned_memory += sizeof(WynString) + intern_table[i]->capacity;
        }
    }
    
    // Count tracked allocations
    StringLeak* current = leak_list;
    while (current) {
        if (wyn_arc_is_valid(current->obj)) {
            stats.tracked_strings++;
            WynString* str = (WynString*)wyn_arc_get_data(current->obj);
            if (str) {
                stats.tracked_memory += sizeof(WynString) + str->capacity;
            }
        }
        current = current->next;
    }
    
    return stats;
}

void wyn_string_print_memory_stats(void) {
    StringMemoryStats stats = wyn_string_get_memory_stats();
    
    printf("=== String Memory Statistics ===\n");
    printf("Interned strings: %zu\n", stats.interned_strings);
    printf("Interned memory: %zu bytes\n", stats.interned_memory);
    printf("Tracked strings: %zu\n", stats.tracked_strings);
    printf("Tracked memory: %zu bytes\n", stats.tracked_memory);
    printf("================================\n");
}