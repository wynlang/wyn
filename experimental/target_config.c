#include "target_config.h"

#ifdef WITH_LLVM

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "safe_memory.h"
#include "error.h"

// Target machine configuration
bool configure_target_machine(LLVMCodegenContext* ctx, TargetConfig* config) {
    if (!ctx || !config) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Invalid context or config for target machine");
        return false;
    }
    
    // Initialize all LLVM targets
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    
    char* error = NULL;
    LLVMTargetRef target;
    
    // Get target from triple
    if (LLVMGetTargetFromTriple(config->triple, &target, &error)) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to get target: %s", error ? error : "unknown error");
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, error_msg);
        if (error) LLVMDisposeMessage(error);
        return false;
    }
    
    // Create target machine
    ctx->target_machine = LLVMCreateTargetMachine(
        target, config->triple, config->cpu, config->features,
        config->opt_level, LLVMRelocDefault, LLVMCodeModelDefault
    );
    
    if (!ctx->target_machine) {
        report_error(ERR_CODEGEN_FAILED, NULL, 0, 0, "Failed to create target machine");
        return false;
    }
    
    return true;
}

// Create native target configuration
TargetConfig* create_native_target_config(LLVMCodeGenOptLevel opt_level) {
    TargetType native_type = detect_native_target();
    return create_target_config(native_type, opt_level);
}

// Create target configuration for specific target
TargetConfig* create_target_config(TargetType type, LLVMCodeGenOptLevel opt_level) {
    TargetConfig* config = safe_malloc(sizeof(TargetConfig));
    if (!config) {
        report_error(ERR_OUT_OF_MEMORY, NULL, 0, 0, "Failed to allocate target config");
        return NULL;
    }
    
    config->type = type;
    config->triple = get_target_triple(type);
    config->cpu = get_target_cpu(type);
    config->features = get_target_features(type);
    config->opt_level = opt_level;
    
    return config;
}

// Destroy target configuration
void destroy_target_config(TargetConfig* config) {
    if (config) {
        safe_free(config);
    }
}

// Detect native target platform
TargetType detect_native_target(void) {
#if defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
        return TARGET_AARCH64_MACOS;
    #else
        return TARGET_X86_64_MACOS;
    #endif
#elif defined(__linux__)
    #if defined(__aarch64__) || defined(__arm64__)
        return TARGET_AARCH64_LINUX;
    #else
        return TARGET_X86_64_LINUX;
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    return TARGET_X86_64_WINDOWS;
#else
    return TARGET_NATIVE; // Fallback
#endif
}

// Get target triple string
const char* get_target_triple(TargetType type) {
    switch (type) {
        case TARGET_X86_64_LINUX:   return "x86_64-unknown-linux-gnu";
        case TARGET_X86_64_WINDOWS: return "x86_64-pc-windows-msvc";
        case TARGET_X86_64_MACOS:   return "x86_64-apple-darwin";
        case TARGET_AARCH64_LINUX:  return "aarch64-unknown-linux-gnu";
        case TARGET_AARCH64_MACOS:  return "arm64-apple-darwin";
        case TARGET_WASM32:         return "wasm32-unknown-unknown";
        case TARGET_NATIVE:
        default:
            return LLVMGetDefaultTargetTriple();
    }
}

// Get target CPU string
const char* get_target_cpu(TargetType type) {
    switch (type) {
        case TARGET_X86_64_LINUX:
        case TARGET_X86_64_WINDOWS:
        case TARGET_X86_64_MACOS:   return "x86-64";
        case TARGET_AARCH64_LINUX:  return "generic";
        case TARGET_AARCH64_MACOS:  return "apple-m1";
        case TARGET_WASM32:         return "";
        case TARGET_NATIVE:
        default:
            return "generic";
    }
}

// Get target features string
const char* get_target_features(TargetType type) {
    switch (type) {
        case TARGET_X86_64_LINUX:
        case TARGET_X86_64_WINDOWS:
        case TARGET_X86_64_MACOS:   return "";
        case TARGET_AARCH64_LINUX:
        case TARGET_AARCH64_MACOS:  return "";
        case TARGET_WASM32:         return "";
        case TARGET_NATIVE:
        default:
            return "";
    }
}

// Parse optimization level from string
LLVMCodeGenOptLevel parse_optimization_level(const char* opt_str) {
    if (!opt_str) return LLVMCodeGenLevelDefault;
    
    if (strcmp(opt_str, "0") == 0 || strcmp(opt_str, "none") == 0) {
        return LLVMCodeGenLevelNone;
    } else if (strcmp(opt_str, "1") == 0 || strcmp(opt_str, "less") == 0) {
        return LLVMCodeGenLevelLess;
    } else if (strcmp(opt_str, "2") == 0 || strcmp(opt_str, "default") == 0) {
        return LLVMCodeGenLevelDefault;
    } else if (strcmp(opt_str, "3") == 0 || strcmp(opt_str, "aggressive") == 0) {
        return LLVMCodeGenLevelAggressive;
    }
    
    return LLVMCodeGenLevelDefault;
}

// Convert optimization level to string
const char* optimization_level_to_string(LLVMCodeGenOptLevel level) {
    switch (level) {
        case LLVMCodeGenLevelNone:       return "none";
        case LLVMCodeGenLevelLess:       return "less";
        case LLVMCodeGenLevelDefault:    return "default";
        case LLVMCodeGenLevelAggressive: return "aggressive";
        default:                         return "default";
    }
}

#endif // WITH_LLVM
