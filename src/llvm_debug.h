#ifndef LLVM_DEBUG_H
#define LLVM_DEBUG_H

#ifdef WITH_LLVM

#include <stdio.h>
#include <stdbool.h>

// Debug flags
extern bool llvm_debug_enabled;
extern bool llvm_verbose_enabled;

// Debug macros
#define LLVM_DEBUG(fmt, ...) \
    do { if (llvm_debug_enabled) fprintf(stderr, "[LLVM DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

#define LLVM_VERBOSE(fmt, ...) \
    do { if (llvm_verbose_enabled) fprintf(stderr, "[LLVM VERBOSE] " fmt "\n", ##__VA_ARGS__); } while(0)

#define LLVM_ERROR(fmt, ...) \
    fprintf(stderr, "[LLVM ERROR] " fmt "\n", ##__VA_ARGS__)

#define LLVM_WARN(fmt, ...) \
    fprintf(stderr, "[LLVM WARN] " fmt "\n", ##__VA_ARGS__)

// Initialize debug system
void llvm_debug_init(void);

// Check if debug is enabled
bool llvm_is_debug_enabled(void);

#else
#define LLVM_DEBUG(fmt, ...)
#define LLVM_VERBOSE(fmt, ...)
#define LLVM_ERROR(fmt, ...)
#define LLVM_WARN(fmt, ...)
#define llvm_debug_init()
#define llvm_is_debug_enabled() false
#endif

#endif // LLVM_DEBUG_H
