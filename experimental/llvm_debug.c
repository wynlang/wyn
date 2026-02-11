#include "llvm_debug.h"

#ifdef WITH_LLVM

#include <stdlib.h>
#include <string.h>

bool llvm_debug_enabled = false;
bool llvm_verbose_enabled = false;

void llvm_debug_init(void) {
    const char* debug_env = getenv("WYN_LLVM_DEBUG");
    const char* verbose_env = getenv("WYN_LLVM_VERBOSE");
    
    llvm_debug_enabled = (debug_env != NULL && strcmp(debug_env, "1") == 0);
    llvm_verbose_enabled = (verbose_env != NULL && strcmp(verbose_env, "1") == 0);
    
    if (llvm_debug_enabled) {
        fprintf(stderr, "[LLVM DEBUG] Debug mode enabled\n");
    }
    if (llvm_verbose_enabled) {
        fprintf(stderr, "[LLVM VERBOSE] Verbose mode enabled\n");
    }
}

bool llvm_is_debug_enabled(void) {
    return llvm_debug_enabled;
}

#endif // WITH_LLVM
