#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/crossplatform.h"

void example_target_management() {
    printf("=== Target Management Example ===\n");
    
    // Create various targets
    WynTarget* linux_x64 = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_x64 = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* macos_arm64 = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    WynTarget* wasm32 = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    
    printf("Created targets:\n");
    printf("  - Linux x64: %s\n", linux_x64->triple);
    printf("  - Windows x64: %s\n", windows_x64->triple);
    printf("  - macOS ARM64: %s\n", macos_arm64->triple);
    printf("  - WebAssembly: %s\n", wasm32->triple);
    
    // Show target properties
    printf("\nTarget properties:\n");
    printf("  - Linux executable extension: '%s'\n", wyn_target_executable_extension(linux_x64));
    printf("  - Windows executable extension: '%s'\n", wyn_target_executable_extension(windows_x64));
    printf("  - Linux shared library extension: '%s'\n", wyn_target_library_extension(linux_x64, true));
    printf("  - Windows shared library extension: '%s'\n", wyn_target_library_extension(windows_x64, true));
    printf("  - WebAssembly executable extension: '%s'\n", wyn_target_executable_extension(wasm32));
    
    // Add CPU features
    wyn_target_add_feature(linux_x64, "sse4.2");
    wyn_target_add_feature(linux_x64, "avx2");
    wyn_target_add_feature(macos_arm64, "neon");
    
    printf("\nTarget features:\n");
    printf("  - Linux x64 has SSE4.2: %s\n", wyn_target_has_feature(linux_x64, "sse4.2") ? "yes" : "no");
    printf("  - Linux x64 has AVX2: %s\n", wyn_target_has_feature(linux_x64, "avx2") ? "yes" : "no");
    printf("  - macOS ARM64 has NEON: %s\n", wyn_target_has_feature(macos_arm64, "neon") ? "yes" : "no");
    
    wyn_target_free(linux_x64);
    wyn_target_free(windows_x64);
    wyn_target_free(macos_arm64);
    wyn_target_free(wasm32);
    
    printf("\n");
}

void example_host_detection() {
    printf("=== Host Detection Example ===\n");
    
    // Detect host platform
    WynTarget* host = wyn_target_get_host();
    
    printf("Host platform information:\n");
    printf("  - Target triple: %s\n", host->triple);
    printf("  - Architecture: %s\n", wyn_target_arch_name(host->arch));
    printf("  - Operating system: %s\n", wyn_target_os_name(host->os));
    printf("  - Environment: %s\n", wyn_target_env_name(host->env));
    printf("  - CPU model: %s\n", host->cpu);
    printf("  - Is native: %s\n", host->is_native ? "yes" : "no");
    printf("  - Is supported: %s\n", wyn_target_is_supported(host) ? "yes" : "no");
    
    // Get default features for host architecture
    size_t feature_count;
    char** default_features = wyn_target_get_default_features(host->arch, &feature_count);
    
    if (default_features && feature_count > 0) {
        printf("  - Default features (%zu):\n", feature_count);
        for (size_t i = 0; i < feature_count; i++) {
            printf("    * %s\n", default_features[i]);
            free(default_features[i]);
        }
        free(default_features);
    }
    
    wyn_target_free(host);
    printf("\n");
}

void example_cross_compiler_setup() {
    printf("=== Cross-Compiler Setup Example ===\n");
    
    // Create cross-compiler
    WynCrossCompiler* compiler = wyn_cross_compiler_new();
    compiler->verbose = true;
    
    printf("Cross-compiler initialized\n");
    printf("Host target: %s\n", compiler->host_target->triple);
    printf("Initial target count: %zu\n", compiler->target_count);
    
    // Add cross-compilation targets
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* wasm_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    
    wyn_cross_compiler_add_target(compiler, linux_target);
    wyn_cross_compiler_add_target(compiler, windows_target);
    wyn_cross_compiler_add_target(compiler, wasm_target);
    
    printf("\nAdded cross-compilation targets:\n");
    printf("  - Linux x64: %s\n", linux_target->triple);
    printf("  - Windows x64: %s\n", windows_target->triple);
    printf("  - WebAssembly: %s\n", wasm_target->triple);
    printf("Total targets: %zu\n", compiler->target_count);
    
    // Find targets by triple
    WynTarget* found_linux = wyn_cross_compiler_find_target(compiler, linux_target->triple);
    WynTarget* found_windows = wyn_cross_compiler_find_target(compiler, windows_target->triple);
    
    printf("\nTarget lookup:\n");
    printf("  - Found Linux target: %s\n", found_linux ? "yes" : "no");
    printf("  - Found Windows target: %s\n", found_windows ? "yes" : "no");
    printf("  - Found nonexistent target: %s\n", 
           wyn_cross_compiler_find_target(compiler, "nonexistent") ? "yes" : "no");
    
    wyn_cross_compiler_free(compiler);
    printf("\n");
}

void example_platform_configuration() {
    printf("=== Platform Configuration Example ===\n");
    
    // Create targets for different platforms
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* macos_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_MACOS, WYN_ENV_GNU);
    WynTarget* wasi_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    
    // Get platform configurations
    WynPlatformConfig* linux_config = wyn_platform_config_for_target(linux_target);
    WynPlatformConfig* windows_config = wyn_platform_config_for_target(windows_target);
    WynPlatformConfig* macos_config = wyn_platform_config_for_target(macos_target);
    WynPlatformConfig* wasi_config = wyn_platform_config_for_target(wasi_target);
    
    printf("Platform configurations:\n");
    
    printf("  Linux:\n");
    printf("    - Linker: %s\n", linux_config->linker);
    printf("    - Archiver: %s\n", linux_config->archiver);
    printf("    - System libraries: %zu\n", linux_config->system_lib_count);
    for (size_t i = 0; i < linux_config->system_lib_count; i++) {
        printf("      * %s\n", linux_config->system_libs[i]);
    }
    
    printf("  Windows:\n");
    printf("    - Linker: %s\n", windows_config->linker);
    printf("    - Archiver: %s\n", windows_config->archiver);
    printf("    - System libraries: %zu\n", windows_config->system_lib_count);
    for (size_t i = 0; i < windows_config->system_lib_count; i++) {
        printf("      * %s\n", windows_config->system_libs[i]);
    }
    
    printf("  macOS:\n");
    printf("    - Linker: %s\n", macos_config->linker);
    printf("    - Archiver: %s\n", macos_config->archiver);
    printf("    - System libraries: %zu\n", macos_config->system_lib_count);
    for (size_t i = 0; i < macos_config->system_lib_count; i++) {
        printf("      * %s\n", macos_config->system_libs[i]);
    }
    
    printf("  WebAssembly/WASI:\n");
    printf("    - Linker: %s\n", wasi_config->linker);
    printf("    - Archiver: %s\n", wasi_config->archiver);
    printf("    - System libraries: %zu\n", wasi_config->system_lib_count);
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(macos_target);
    wyn_target_free(wasi_target);
    wyn_platform_config_free(linux_config);
    wyn_platform_config_free(windows_config);
    wyn_platform_config_free(macos_config);
    wyn_platform_config_free(wasi_config);
    
    printf("\n");
}

void example_supported_targets() {
    printf("=== Supported Targets Example ===\n");
    
    // Get all supported targets
    size_t count;
    char** targets = wyn_get_supported_targets(&count);
    
    printf("Supported compilation targets (%zu):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  %zu. %s\n", i + 1, targets[i]);
        
        // Parse and show details
        WynTarget* target = wyn_target_parse(targets[i]);
        if (target) {
            printf("     - Architecture: %s\n", wyn_target_arch_name(target->arch));
            printf("     - OS: %s\n", wyn_target_os_name(target->os));
            printf("     - Environment: %s\n", wyn_target_env_name(target->env));
            printf("     - Executable ext: '%s'\n", wyn_target_executable_extension(target));
            printf("     - Shared lib ext: '%s'\n", wyn_target_library_extension(target, true));
            printf("     - Static lib ext: '%s'\n", wyn_target_library_extension(target, false));
            printf("     - Object ext: '%s'\n", wyn_target_object_extension(target));
            wyn_target_free(target);
        }
        printf("\n");
        
        free(targets[i]);
    }
    free(targets);
    
    printf("\n");
}

void example_conditional_compilation() {
    printf("=== Conditional Compilation Example ===\n");
    
    // Create targets for testing
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* macos_arm_target = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    WynTarget* wasm_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    
    // Test OS-specific conditions
    printf("OS-specific conditional compilation:\n");
    printf("  Linux target:\n");
    printf("    - cfg(target_os = \"linux\"): %s\n", 
           wyn_evaluate_cfg_condition("target_os = \"linux\"", linux_target) ? "true" : "false");
    printf("    - cfg(target_os = \"windows\"): %s\n", 
           wyn_evaluate_cfg_condition("target_os = \"windows\"", linux_target) ? "true" : "false");
    
    printf("  Windows target:\n");
    printf("    - cfg(target_os = \"windows\"): %s\n", 
           wyn_evaluate_cfg_condition("target_os = \"windows\"", windows_target) ? "true" : "false");
    printf("    - cfg(target_os = \"linux\"): %s\n", 
           wyn_evaluate_cfg_condition("target_os = \"linux\"", windows_target) ? "true" : "false");
    
    // Test architecture-specific conditions
    printf("\nArchitecture-specific conditional compilation:\n");
    printf("  x86_64 targets:\n");
    printf("    - Linux cfg(target_arch = \"x86_64\"): %s\n", 
           wyn_evaluate_cfg_condition("target_arch = \"x86_64\"", linux_target) ? "true" : "false");
    printf("    - Windows cfg(target_arch = \"x86_64\"): %s\n", 
           wyn_evaluate_cfg_condition("target_arch = \"x86_64\"", windows_target) ? "true" : "false");
    
    printf("  ARM64 target:\n");
    printf("    - macOS cfg(target_arch = \"aarch64\"): %s\n", 
           wyn_evaluate_cfg_condition("target_arch = \"aarch64\"", macos_arm_target) ? "true" : "false");
    printf("    - macOS cfg(target_arch = \"x86_64\"): %s\n", 
           wyn_evaluate_cfg_condition("target_arch = \"x86_64\"", macos_arm_target) ? "true" : "false");
    
    printf("  WebAssembly target:\n");
    printf("    - WASM cfg(target_arch = \"wasm32\"): %s\n", 
           wyn_evaluate_cfg_condition("target_arch = \"wasm32\"", wasm_target) ? "true" : "false");
    printf("    - WASM cfg(target_os = \"wasi\"): %s\n", 
           wyn_evaluate_cfg_condition("target_os = \"wasi\"", wasm_target) ? "true" : "false");
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(macos_arm_target);
    wyn_target_free(wasm_target);
    
    printf("\n");
}

void example_webassembly_config() {
    printf("=== WebAssembly Configuration Example ===\n");
    
    // Create WebAssembly configuration
    WynWasmConfig* config = wyn_wasm_config_new();
    
    printf("Default WebAssembly configuration:\n");
    printf("  - Enable SIMD: %s\n", config->enable_simd ? "yes" : "no");
    printf("  - Enable threads: %s\n", config->enable_threads ? "yes" : "no");
    printf("  - Enable bulk memory: %s\n", config->enable_bulk_memory ? "yes" : "no");
    printf("  - Enable reference types: %s\n", config->enable_reference_types ? "yes" : "no");
    printf("  - Initial memory: %u bytes (%u KB)\n", config->initial_memory, config->initial_memory / 1024);
    printf("  - Maximum memory: %u bytes (%u MB)\n", config->max_memory, config->max_memory / (1024 * 1024));
    
    // Customize configuration
    printf("\nCustomizing WebAssembly configuration:\n");
    config->enable_simd = true;
    config->enable_threads = true;
    config->enable_reference_types = true;
    config->initial_memory = 2 * 1024 * 1024; // 2MB
    config->max_memory = 64 * 1024 * 1024;    // 64MB
    
    printf("  - Enable SIMD: %s\n", config->enable_simd ? "yes" : "no");
    printf("  - Enable threads: %s\n", config->enable_threads ? "yes" : "no");
    printf("  - Enable reference types: %s\n", config->enable_reference_types ? "yes" : "no");
    printf("  - Initial memory: %u bytes (%u MB)\n", config->initial_memory, config->initial_memory / (1024 * 1024));
    printf("  - Maximum memory: %u bytes (%u MB)\n", config->max_memory, config->max_memory / (1024 * 1024));
    
    wyn_wasm_config_free(config);
    printf("\n");
}

void example_cross_compilation_workflow() {
    printf("=== Cross-Compilation Workflow Example ===\n");
    
    // Create cross-compiler
    WynCrossCompiler* compiler = wyn_cross_compiler_new();
    
    // Add targets for cross-compilation
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    
    wyn_cross_compiler_add_target(compiler, linux_target);
    wyn_cross_compiler_add_target(compiler, windows_target);
    
    printf("Cross-compilation workflow:\n");
    printf("  1. Host platform: %s\n", compiler->host_target->triple);
    printf("  2. Available targets: %zu\n", compiler->target_count);
    
    // Simulate cross-compilation (would fail without actual source files)
    printf("  3. Cross-compiling to Linux:\n");
    printf("     Command: clang --target=%s -c source.wyn -o output.o\n", linux_target->triple);
    
    printf("  4. Cross-compiling to Windows:\n");
    printf("     Command: clang --target=%s -c source.wyn -o output.obj\n", windows_target->triple);
    
    printf("  5. Output files:\n");
    printf("     - Linux object: output%s\n", wyn_target_object_extension(linux_target));
    printf("     - Windows object: output%s\n", wyn_target_object_extension(windows_target));
    printf("     - Linux executable: program%s\n", wyn_target_executable_extension(linux_target));
    printf("     - Windows executable: program%s\n", wyn_target_executable_extension(windows_target));
    
    wyn_cross_compiler_free(compiler);
    printf("\n");
}

int main() {
    printf("Wyn Cross-Platform Compilation Examples\n");
    printf("========================================\n\n");
    
    example_target_management();
    example_host_detection();
    example_cross_compiler_setup();
    example_platform_configuration();
    example_supported_targets();
    example_conditional_compilation();
    example_webassembly_config();
    example_cross_compilation_workflow();
    
    printf("Cross-Platform Compilation Features:\n");
    printf("  ✓ Multi-target support (x86_64, ARM64, WebAssembly)\n");
    printf("  ✓ Cross-compilation from any host to any target\n");
    printf("  ✓ Platform-specific toolchain configuration\n");
    printf("  ✓ Target feature detection and CPU optimization\n");
    printf("  ✓ Conditional compilation with cfg attributes\n");
    printf("  ✓ WebAssembly-specific configuration and features\n");
    printf("  ✓ File extension and naming conventions per platform\n");
    printf("  ✓ Host platform detection and native compilation\n");
    printf("\nReady for multi-platform Wyn development!\n");
    
    return 0;
}
