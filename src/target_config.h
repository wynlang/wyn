#ifndef TARGET_CONFIG_H
#define TARGET_CONFIG_H

#ifdef WITH_LLVM

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdbool.h>
#include "llvm_context.h"

// Target platform enumeration
typedef enum {
    TARGET_NATIVE,
    TARGET_X86_64_LINUX,
    TARGET_X86_64_WINDOWS,
    TARGET_X86_64_MACOS,
    TARGET_AARCH64_LINUX,
    TARGET_AARCH64_MACOS,
    TARGET_WASM32
} TargetType;

// Target configuration structure
typedef struct {
    TargetType type;
    const char* triple;
    const char* cpu;
    const char* features;
    LLVMCodeGenOptLevel opt_level;
} TargetConfig;

// Target machine configuration functions
bool configure_target_machine(LLVMCodegenContext* ctx, TargetConfig* config);
TargetConfig* create_native_target_config(LLVMCodeGenOptLevel opt_level);
TargetConfig* create_target_config(TargetType type, LLVMCodeGenOptLevel opt_level);
void destroy_target_config(TargetConfig* config);

// Target detection and setup
TargetType detect_native_target(void);
const char* get_target_triple(TargetType type);
const char* get_target_cpu(TargetType type);
const char* get_target_features(TargetType type);

// Optimization level utilities
LLVMCodeGenOptLevel parse_optimization_level(const char* opt_str);
const char* optimization_level_to_string(LLVMCodeGenOptLevel level);

#else
// Fallback when LLVM is not available
typedef struct { bool dummy; } TargetConfig;
static inline bool configure_target_machine(void* ctx, TargetConfig* config) { return false; }
static inline TargetConfig* create_native_target_config(int opt_level) { return NULL; }
static inline void destroy_target_config(TargetConfig* config) { (void)config; }
#endif

#endif // TARGET_CONFIG_H
