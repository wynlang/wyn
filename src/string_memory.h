#ifndef STRING_MEMORY_H
#define STRING_MEMORY_H

#include "wyn_string.h"
#include "arc_runtime.h"
#include <stddef.h>
#include <stdbool.h>

// String memory management with ARC integration

// String interning for memory optimization
WynString* wyn_string_intern(const char* cstr);
void wyn_string_cleanup_intern_table(void);

// ARC-managed string operations
WynObject* wyn_string_create_arc(const char* cstr);
void wyn_string_destructor(void* data);
WynObject* wyn_string_concat_arc(WynObject* a_obj, WynObject* b_obj);
WynObject* wyn_string_copy_arc(WynObject* src_obj);
void wyn_string_assign_arc(WynObject** dest, WynObject* src);

// Memory leak detection
void wyn_string_track_allocation(WynObject* obj, const char* file, int line);
void wyn_string_untrack_allocation(WynObject* obj);
void wyn_string_check_leaks(void);

// Macros for tracked allocation
#define WYN_STRING_CREATE(cstr) \
    ({ WynObject* _obj = wyn_string_create_arc(cstr); \
       wyn_string_track_allocation(_obj, __FILE__, __LINE__); \
       _obj; })

#define WYN_STRING_RELEASE(obj) \
    do { \
        if (obj) { \
            wyn_string_untrack_allocation(obj); \
            wyn_arc_release(obj); \
            obj = NULL; \
        } \
    } while(0)

// String memory statistics
typedef struct {
    size_t interned_strings;
    size_t interned_memory;
    size_t tracked_strings;
    size_t tracked_memory;
} StringMemoryStats;

StringMemoryStats wyn_string_get_memory_stats(void);
void wyn_string_print_memory_stats(void);

#endif // STRING_MEMORY_H