#include "crossplatform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Target management
WynTarget* wyn_target_new(WynTargetArch arch, WynTargetOS os, WynTargetEnv env) {
    WynTarget* target = malloc(sizeof(WynTarget));
    if (!target) return NULL;
    
    memset(target, 0, sizeof(WynTarget));
    target->arch = arch;
    target->os = os;
    target->env = env;
    
    // Generate target triple
    const char* arch_str = wyn_target_arch_name(arch);
    const char* os_str = wyn_target_os_name(os);
    const char* env_str = wyn_target_env_name(env);
    
    size_t triple_len = strlen(arch_str) + strlen(os_str) + strlen(env_str) + 10;
    target->triple = malloc(triple_len);
    snprintf(target->triple, triple_len, "%s-unknown-%s-%s", arch_str, os_str, env_str);
    
    target->cpu = strdup("generic");
    target->is_native = false;
    
    return target;
}

void wyn_target_free(WynTarget* target) {
    if (!target) return;
    
    free(target->triple);
    free(target->cpu);
    
    for (size_t i = 0; i < target->feature_count; i++) {
        free(target->features[i]);
    }
    free(target->features);
    
    free(target);
}

WynTarget* wyn_target_parse(const char* triple) {
    if (!triple) return NULL;
    
    // Simple parsing for common target triples
    if (strstr(triple, "x86_64") && strstr(triple, "windows")) {
        return wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    } else if (strstr(triple, "x86_64") && strstr(triple, "linux")) {
        return wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    } else if (strstr(triple, "x86_64") && strstr(triple, "darwin")) {
        return wyn_target_new(WYN_ARCH_X86_64, WYN_OS_MACOS, WYN_ENV_GNU);
    } else if (strstr(triple, "aarch64") && strstr(triple, "darwin")) {
        return wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    } else if (strstr(triple, "wasm32")) {
        return wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    }
    
    // Default fallback
    return wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
}

char* wyn_target_to_string(const WynTarget* target) {
    if (!target) return NULL;
    return strdup(target->triple);
}

bool wyn_target_equals(const WynTarget* a, const WynTarget* b) {
    if (!a || !b) return false;
    return a->arch == b->arch && a->os == b->os && a->env == b->env;
}

WynTarget* wyn_target_get_host(void) {
    // Detect host platform
    WynTarget* host = NULL;
#ifdef _WIN32
    host = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
#elif __APPLE__
    #ifdef __aarch64__
        host = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    #else
        host = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_MACOS, WYN_ENV_GNU);
    #endif
#elif __linux__
    host = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
#else
    host = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
#endif
    
    if (host) {
        host->is_native = true;
    }
    
    return host;
}

// Target feature management
bool wyn_target_add_feature(WynTarget* target, const char* feature) {
    if (!target || !feature) return false;
    
    target->features = realloc(target->features, (target->feature_count + 1) * sizeof(char*));
    if (!target->features) return false;
    
    target->features[target->feature_count] = strdup(feature);
    target->feature_count++;
    
    return true;
}

bool wyn_target_has_feature(const WynTarget* target, const char* feature) {
    if (!target || !feature) return false;
    
    for (size_t i = 0; i < target->feature_count; i++) {
        if (strcmp(target->features[i], feature) == 0) {
            return true;
        }
    }
    
    return false;
}

char** wyn_target_get_default_features(WynTargetArch arch, size_t* count) {
    if (!count) return NULL;
    
    *count = 0;
    
    switch (arch) {
        case WYN_ARCH_X86_64: {
            *count = 3;
            char** features = malloc(3 * sizeof(char*));
            features[0] = strdup("sse2");
            features[1] = strdup("sse4.1");
            features[2] = strdup("popcnt");
            return features;
        }
        case WYN_ARCH_ARM64: {
            *count = 2;
            char** features = malloc(2 * sizeof(char*));
            features[0] = strdup("neon");
            features[1] = strdup("crc");
            return features;
        }
        case WYN_ARCH_WASM32: {
            *count = 1;
            char** features = malloc(1 * sizeof(char*));
            features[0] = strdup("bulk-memory");
            return features;
        }
        default:
            return NULL;
    }
}

// Cross-compiler management
WynCrossCompiler* wyn_cross_compiler_new(void) {
    WynCrossCompiler* compiler = malloc(sizeof(WynCrossCompiler));
    if (!compiler) return NULL;
    
    memset(compiler, 0, sizeof(WynCrossCompiler));
    
    compiler->host_target = wyn_target_get_host();
    compiler->host_target->is_native = true;
    
    compiler->target_capacity = 10;
    compiler->targets = malloc(compiler->target_capacity * sizeof(WynTarget*));
    compiler->platform_configs = malloc(compiler->target_capacity * sizeof(WynPlatformConfig*));
    
    // Add host target
    compiler->targets[0] = compiler->host_target;
    compiler->platform_configs[0] = wyn_platform_config_for_target(compiler->host_target);
    compiler->target_count = 1;
    
    return compiler;
}

void wyn_cross_compiler_free(WynCrossCompiler* compiler) {
    if (!compiler) return;
    
    for (size_t i = 0; i < compiler->target_count; i++) {
        if (compiler->targets[i] != compiler->host_target) {
            wyn_target_free(compiler->targets[i]);
        }
        wyn_platform_config_free(compiler->platform_configs[i]);
    }
    
    wyn_target_free(compiler->host_target);
    free(compiler->targets);
    free(compiler->platform_configs);
    free(compiler->toolchain_dir);
    free(compiler);
}

bool wyn_cross_compiler_add_target(WynCrossCompiler* compiler, WynTarget* target) {
    if (!compiler || !target) return false;
    
    // Check if target already exists
    for (size_t i = 0; i < compiler->target_count; i++) {
        if (wyn_target_equals(compiler->targets[i], target)) {
            return false; // Already exists
        }
    }
    
    // Resize if needed
    if (compiler->target_count >= compiler->target_capacity) {
        compiler->target_capacity *= 2;
        compiler->targets = realloc(compiler->targets, 
                                   compiler->target_capacity * sizeof(WynTarget*));
        compiler->platform_configs = realloc(compiler->platform_configs,
                                            compiler->target_capacity * sizeof(WynPlatformConfig*));
        if (!compiler->targets || !compiler->platform_configs) return false;
    }
    
    compiler->targets[compiler->target_count] = target;
    compiler->platform_configs[compiler->target_count] = wyn_platform_config_for_target(target);
    compiler->target_count++;
    
    return true;
}

WynTarget* wyn_cross_compiler_find_target(WynCrossCompiler* compiler, const char* triple) {
    if (!compiler || !triple) return NULL;
    
    for (size_t i = 0; i < compiler->target_count; i++) {
        if (strcmp(compiler->targets[i]->triple, triple) == 0) {
            return compiler->targets[i];
        }
    }
    
    return NULL;
}

// Platform configuration
WynPlatformConfig* wyn_platform_config_new(void) {
    WynPlatformConfig* config = malloc(sizeof(WynPlatformConfig));
    if (!config) return NULL;
    
    memset(config, 0, sizeof(WynPlatformConfig));
    return config;
}

void wyn_platform_config_free(WynPlatformConfig* config) {
    if (!config) return;
    
    free(config->linker);
    free(config->archiver);
    free(config->objcopy);
    free(config->sysroot);
    free(config->sdk_path);
    
    for (size_t i = 0; i < config->link_arg_count; i++) {
        free(config->link_args[i]);
    }
    free(config->link_args);
    
    for (size_t i = 0; i < config->lib_dir_count; i++) {
        free(config->lib_dirs[i]);
    }
    free(config->lib_dirs);
    
    for (size_t i = 0; i < config->system_lib_count; i++) {
        free(config->system_libs[i]);
    }
    free(config->system_libs);
    
    free(config);
}

WynPlatformConfig* wyn_platform_config_for_target(const WynTarget* target) {
    if (!target) return NULL;
    
    WynPlatformConfig* config = wyn_platform_config_new();
    if (!config) return NULL;
    
    // Set platform-specific defaults
    switch (target->os) {
        case WYN_OS_WINDOWS:
            config->linker = strdup("link.exe");
            config->archiver = strdup("lib.exe");
            wyn_platform_config_add_system_lib(config, "kernel32");
            wyn_platform_config_add_system_lib(config, "user32");
            break;
            
        case WYN_OS_MACOS:
            config->linker = strdup("ld");
            config->archiver = strdup("ar");
            wyn_platform_config_add_system_lib(config, "System");
            break;
            
        case WYN_OS_LINUX:
            config->linker = strdup("ld");
            config->archiver = strdup("ar");
            wyn_platform_config_add_system_lib(config, "c");
            wyn_platform_config_add_system_lib(config, "m");
            break;
            
        case WYN_OS_WASI:
            config->linker = strdup("wasm-ld");
            config->archiver = strdup("llvm-ar");
            break;
            
        default:
            config->linker = strdup("ld");
            config->archiver = strdup("ar");
            break;
    }
    
    return config;
}

bool wyn_platform_config_add_system_lib(WynPlatformConfig* config, const char* lib) {
    if (!config || !lib) return false;
    
    config->system_libs = realloc(config->system_libs, 
                                 (config->system_lib_count + 1) * sizeof(char*));
    if (!config->system_libs) return false;
    
    config->system_libs[config->system_lib_count] = strdup(lib);
    config->system_lib_count++;
    
    return true;
}

// Cross-compilation
bool wyn_cross_compile(WynCrossCompiler* compiler, const char* source_file, 
                      const WynTarget* target, const char* output_file) {
    if (!compiler || !source_file || !target || !output_file) return false;
    
    // Find platform config for target
    WynPlatformConfig* config = NULL;
    for (size_t i = 0; i < compiler->target_count; i++) {
        if (wyn_target_equals(compiler->targets[i], target)) {
            config = compiler->platform_configs[i];
            break;
        }
    }
    
    if (!config) return false;
    
    // Build compilation command
    char compile_cmd[2048];
    snprintf(compile_cmd, sizeof(compile_cmd), 
             "clang --target=%s -c %s -o %s", 
             target->triple, source_file, output_file);
    
    if (compiler->verbose) {
        printf("Cross-compiling: %s\n", compile_cmd);
    }
    
    return system(compile_cmd) == 0;
}

// Target detection and validation
bool wyn_target_is_supported(const WynTarget* target) {
    if (!target) return false;
    
    // Check if architecture is supported
    switch (target->arch) {
        case WYN_ARCH_X86_64:
        case WYN_ARCH_ARM64:
        case WYN_ARCH_WASM32:
            break;
        default:
            return false;
    }
    
    // Check if OS is supported
    switch (target->os) {
        case WYN_OS_WINDOWS:
        case WYN_OS_MACOS:
        case WYN_OS_LINUX:
        case WYN_OS_WASI:
            return true;
        default:
            return false;
    }
}

char** wyn_get_supported_targets(size_t* count) {
    if (!count) return NULL;
    
    *count = 6;
    char** targets = malloc(6 * sizeof(char*));
    
    targets[0] = strdup("x86_64-unknown-linux-gnu");
    targets[1] = strdup("x86_64-pc-windows-msvc");
    targets[2] = strdup("x86_64-apple-darwin");
    targets[3] = strdup("aarch64-apple-darwin");
    targets[4] = strdup("aarch64-unknown-linux-gnu");
    targets[5] = strdup("wasm32-unknown-wasi");
    
    return targets;
}

// Platform-specific utilities
const char* wyn_target_arch_name(WynTargetArch arch) {
    switch (arch) {
        case WYN_ARCH_X86_64: return "x86_64";
        case WYN_ARCH_ARM64: return "aarch64";
        case WYN_ARCH_X86: return "i686";
        case WYN_ARCH_ARM: return "arm";
        case WYN_ARCH_WASM32: return "wasm32";
        case WYN_ARCH_WASM64: return "wasm64";
        default: return "unknown";
    }
}

const char* wyn_target_os_name(WynTargetOS os) {
    switch (os) {
        case WYN_OS_WINDOWS: return "windows";
        case WYN_OS_MACOS: return "darwin";
        case WYN_OS_LINUX: return "linux";
        case WYN_OS_FREEBSD: return "freebsd";
        case WYN_OS_WASI: return "wasi";
        case WYN_OS_BROWSER: return "emscripten";
        default: return "unknown";
    }
}

const char* wyn_target_env_name(WynTargetEnv env) {
    switch (env) {
        case WYN_ENV_GNU: return "gnu";
        case WYN_ENV_MSVC: return "msvc";
        case WYN_ENV_MUSL: return "musl";
        case WYN_ENV_WASI: return "wasi";
        case WYN_ENV_EMSCRIPTEN: return "emscripten";
        default: return "unknown";
    }
}

const char* wyn_target_executable_extension(const WynTarget* target) {
    if (!target) return "";
    
    switch (target->os) {
        case WYN_OS_WINDOWS: return ".exe";
        case WYN_OS_WASI: return ".wasm";
        default: return "";
    }
}

const char* wyn_target_library_extension(const WynTarget* target, bool shared) {
    if (!target) return "";
    
    switch (target->os) {
        case WYN_OS_WINDOWS:
            return shared ? ".dll" : ".lib";
        case WYN_OS_MACOS:
            return shared ? ".dylib" : ".a";
        case WYN_OS_WASI:
            return ".wasm";
        default:
            return shared ? ".so" : ".a";
    }
}

const char* wyn_target_object_extension(const WynTarget* target) {
    if (!target) return ".o";
    
    switch (target->os) {
        case WYN_OS_WINDOWS: return ".obj";
        default: return ".o";
    }
}

// Conditional compilation support
bool wyn_evaluate_cfg_condition(const char* condition, const WynTarget* target) {
    if (!condition || !target) return false;
    
    // Simple condition evaluation
    if (strstr(condition, "target_os")) {
        const char* os_name = wyn_target_os_name(target->os);
        return strstr(condition, os_name) != NULL;
    }
    
    if (strstr(condition, "target_arch")) {
        const char* arch_name = wyn_target_arch_name(target->arch);
        return strstr(condition, arch_name) != NULL;
    }
    
    return false;
}

// WebAssembly specific
WynWasmConfig* wyn_wasm_config_new(void) {
    WynWasmConfig* config = malloc(sizeof(WynWasmConfig));
    if (!config) return NULL;
    
    memset(config, 0, sizeof(WynWasmConfig));
    config->enable_bulk_memory = true;
    config->initial_memory = 1024 * 1024; // 1MB
    config->max_memory = 16 * 1024 * 1024; // 16MB
    
    return config;
}

void wyn_wasm_config_free(WynWasmConfig* config) {
    if (!config) return;
    
    free(config->import_memory);
    free(config->export_memory);
    free(config);
}

// Cross-platform feature implementations (minimal/no-op)
bool wyn_cross_compiler_set_toolchain_dir(WynCrossCompiler* compiler, const char* dir) {
    (void)compiler; (void)dir;
    return false; // Not implemented
}

bool wyn_platform_config_add_link_arg(WynPlatformConfig* config, const char* arg) {
    (void)config; (void)arg;
    return false; // Not implemented
}

bool wyn_platform_config_add_lib_dir(WynPlatformConfig* config, const char* dir) {
    (void)config; (void)dir;
    return false; // Not implemented
}

bool wyn_cross_compile_multiple(WynCrossCompiler* compiler, char** source_files, 
                               size_t source_count, const WynTarget* target, 
                               const char* output_file) {
    (void)compiler; (void)source_files; (void)source_count; (void)target; (void)output_file;
    return false; // Not implemented
}

bool wyn_target_can_cross_compile(const WynTarget* host, const WynTarget* target) {
    (void)host; (void)target;
    return true; // Assume cross-compilation possible
}

WynConditionalFlag** wyn_get_target_cfg_flags(const WynTarget* target, size_t* count) {
    (void)target;
    if (count) *count = 0;
    return NULL; // No flags
}

bool wyn_cross_compile_wasm(WynCrossCompiler* compiler, const char* source_file,
                           const WynWasmConfig* wasm_config, const char* output_file) {
    (void)compiler; (void)source_file; (void)wasm_config; (void)output_file;
    return false; // Not implemented
}

WynToolchain** wyn_detect_toolchains(size_t* count) {
    if (count) *count = 0;
    return NULL; // No toolchains detected
}

void wyn_toolchain_free(WynToolchain* toolchain) {
    (void)toolchain; // No-op
}

WynToolchain* wyn_find_toolchain_for_target(const WynTarget* target) {
    (void)target;
    return NULL; // Not found
}

bool wyn_setup_cross_compilation(const char* project_dir, const WynTarget* target) {
    (void)project_dir; (void)target;
    return false; // Not implemented
}

bool wyn_generate_cross_build_script(const char* project_dir, const WynTarget* target, 
                                    const char* script_path) {
    (void)project_dir; (void)target; (void)script_path;
    return false; // Not implemented
}
