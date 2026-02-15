#ifndef WYN_CROSSPLATFORM_H
#define WYN_CROSSPLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynTarget WynTarget;
typedef struct WynCrossCompiler WynCrossCompiler;
typedef struct WynPlatformConfig WynPlatformConfig;

// Target architectures
typedef enum {
    WYN_ARCH_X86_64,
    WYN_ARCH_ARM64,
    WYN_ARCH_X86,
    WYN_ARCH_ARM,
    WYN_ARCH_WASM32,
    WYN_ARCH_WASM64
} WynTargetArch;

// Target operating systems
typedef enum {
    WYN_OS_WINDOWS,
    WYN_OS_MACOS,
    WYN_OS_LINUX,
    WYN_OS_FREEBSD,
    WYN_OS_WASI,
    WYN_OS_BROWSER
} WynTargetOS;

// Target environments
typedef enum {
    WYN_ENV_GNU,
    WYN_ENV_MSVC,
    WYN_ENV_MUSL,
    WYN_ENV_WASI,
    WYN_ENV_EMSCRIPTEN
} WynTargetEnv;

// Compilation target
typedef struct WynTarget {
    WynTargetArch arch;
    WynTargetOS os;
    WynTargetEnv env;
    char* triple;           // Target triple (e.g., "x86_64-pc-windows-msvc")
    char* cpu;              // CPU model (e.g., "generic", "native")
    char** features;        // CPU features (e.g., "sse4.2", "avx2")
    size_t feature_count;
    bool is_native;         // True if this is the host target
} WynTarget;

// Platform-specific configuration
typedef struct WynPlatformConfig {
    char* linker;
    char* archiver;
    char* objcopy;
    char** link_args;
    size_t link_arg_count;
    char** lib_dirs;
    size_t lib_dir_count;
    char** system_libs;
    size_t system_lib_count;
    char* sysroot;
    char* sdk_path;
} WynPlatformConfig;

// Cross-compiler state
typedef struct WynCrossCompiler {
    WynTarget* host_target;
    WynTarget** targets;
    size_t target_count;
    size_t target_capacity;
    WynPlatformConfig** platform_configs;
    char* toolchain_dir;
    bool verbose;
} WynCrossCompiler;

// Target management
WynTarget* wyn_target_new(WynTargetArch arch, WynTargetOS os, WynTargetEnv env);
void wyn_target_free(WynTarget* target);
WynTarget* wyn_target_parse(const char* triple);
char* wyn_target_to_string(const WynTarget* target);
bool wyn_target_equals(const WynTarget* a, const WynTarget* b);
WynTarget* wyn_target_get_host(void);

// Target feature management
bool wyn_target_add_feature(WynTarget* target, const char* feature);
bool wyn_target_has_feature(const WynTarget* target, const char* feature);
char** wyn_target_get_default_features(WynTargetArch arch, size_t* count);

// Cross-compiler management
WynCrossCompiler* wyn_cross_compiler_new(void);
void wyn_cross_compiler_free(WynCrossCompiler* compiler);
bool wyn_cross_compiler_add_target(WynCrossCompiler* compiler, WynTarget* target);
WynTarget* wyn_cross_compiler_find_target(WynCrossCompiler* compiler, const char* triple);
bool wyn_cross_compiler_set_toolchain_dir(WynCrossCompiler* compiler, const char* dir);

// Platform configuration
WynPlatformConfig* wyn_platform_config_new(void);
void wyn_platform_config_free(WynPlatformConfig* config);
WynPlatformConfig* wyn_platform_config_for_target(const WynTarget* target);
bool wyn_platform_config_add_link_arg(WynPlatformConfig* config, const char* arg);
bool wyn_platform_config_add_lib_dir(WynPlatformConfig* config, const char* dir);
bool wyn_platform_config_add_system_lib(WynPlatformConfig* config, const char* lib);

// Cross-compilation
bool wyn_cross_compile(WynCrossCompiler* compiler, const char* source_file, 
                      const WynTarget* target, const char* output_file);
bool wyn_cross_compile_multiple(WynCrossCompiler* compiler, char** source_files, 
                               size_t source_count, const WynTarget* target, 
                               const char* output_file);

// Target detection and validation
bool wyn_target_is_supported(const WynTarget* target);
bool wyn_target_can_cross_compile(const WynTarget* host, const WynTarget* target);
char** wyn_get_supported_targets(size_t* count);

// Platform-specific utilities
const char* wyn_target_arch_name(WynTargetArch arch);
const char* wyn_target_os_name(WynTargetOS os);
const char* wyn_target_env_name(WynTargetEnv env);
const char* wyn_target_executable_extension(const WynTarget* target);
const char* wyn_target_library_extension(const WynTarget* target, bool shared);
const char* wyn_target_object_extension(const WynTarget* target);

// Conditional compilation support
typedef struct {
    char* condition;
    bool enabled;
} WynConditionalFlag;

bool wyn_evaluate_cfg_condition(const char* condition, const WynTarget* target);
WynConditionalFlag** wyn_get_target_cfg_flags(const WynTarget* target, size_t* count);

// WebAssembly specific
typedef struct {
    bool enable_simd;
    bool enable_threads;
    bool enable_bulk_memory;
    bool enable_reference_types;
    char* import_memory;
    char* export_memory;
    uint32_t initial_memory;
    uint32_t max_memory;
} WynWasmConfig;

WynWasmConfig* wyn_wasm_config_new(void);
void wyn_wasm_config_free(WynWasmConfig* config);
bool wyn_cross_compile_wasm(WynCrossCompiler* compiler, const char* source_file,
                           const WynWasmConfig* wasm_config, const char* output_file);

// Toolchain detection
typedef struct {
    char* name;
    char* version;
    char* path;
    WynTarget** supported_targets;
    size_t supported_target_count;
} WynToolchain;

WynToolchain** wyn_detect_toolchains(size_t* count);
void wyn_toolchain_free(WynToolchain* toolchain);
WynToolchain* wyn_find_toolchain_for_target(const WynTarget* target);

// Build system integration
bool wyn_setup_cross_compilation(const char* project_dir, const WynTarget* target);
bool wyn_generate_cross_build_script(const char* project_dir, const WynTarget* target, 
                                    const char* script_path);

#endif // WYN_CROSSPLATFORM_H
