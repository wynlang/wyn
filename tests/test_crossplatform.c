#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/crossplatform.h"

void test_target_management() {
    printf("Testing target management...\n");
    
    // Create targets
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    assert(linux_target != NULL);
    assert(linux_target->arch == WYN_ARCH_X86_64);
    assert(linux_target->os == WYN_OS_LINUX);
    assert(linux_target->env == WYN_ENV_GNU);
    assert(linux_target->triple != NULL);
    
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    assert(windows_target != NULL);
    assert(windows_target->arch == WYN_ARCH_X86_64);
    assert(windows_target->os == WYN_OS_WINDOWS);
    assert(windows_target->env == WYN_ENV_MSVC);
    
    // Test target equality
    WynTarget* linux_target2 = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    assert(wyn_target_equals(linux_target, linux_target2) == true);
    assert(wyn_target_equals(linux_target, windows_target) == false);
    
    // Test target string representation
    char* linux_str = wyn_target_to_string(linux_target);
    assert(linux_str != NULL);
    assert(strstr(linux_str, "x86_64") != NULL);
    assert(strstr(linux_str, "linux") != NULL);
    
    free(linux_str);
    wyn_target_free(linux_target);
    wyn_target_free(linux_target2);
    wyn_target_free(windows_target);
    
    printf("✓ Target management tests passed\n");
}

void test_target_parsing() {
    printf("Testing target parsing...\n");
    
    // Test parsing common target triples
    WynTarget* linux_target = wyn_target_parse("x86_64-unknown-linux-gnu");
    assert(linux_target != NULL);
    assert(linux_target->arch == WYN_ARCH_X86_64);
    assert(linux_target->os == WYN_OS_LINUX);
    
    WynTarget* windows_target = wyn_target_parse("x86_64-pc-windows-msvc");
    assert(windows_target != NULL);
    assert(windows_target->arch == WYN_ARCH_X86_64);
    assert(windows_target->os == WYN_OS_WINDOWS);
    
    WynTarget* macos_target = wyn_target_parse("x86_64-apple-darwin");
    assert(macos_target != NULL);
    assert(macos_target->arch == WYN_ARCH_X86_64);
    assert(macos_target->os == WYN_OS_MACOS);
    
    WynTarget* wasm_target = wyn_target_parse("wasm32-unknown-wasi");
    assert(wasm_target != NULL);
    assert(wasm_target->arch == WYN_ARCH_WASM32);
    assert(wasm_target->os == WYN_OS_WASI);
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(macos_target);
    wyn_target_free(wasm_target);
    
    printf("✓ Target parsing tests passed\n");
}

void test_host_detection() {
    printf("Testing host detection...\n");
    
    WynTarget* host = wyn_target_get_host();
    assert(host != NULL);
    assert(host->is_native == true);
    assert(host->triple != NULL);
    assert(host->cpu != NULL);
    
    // Host should be a supported target
    assert(wyn_target_is_supported(host) == true);
    
    printf("Host target: %s\n", host->triple);
    
    wyn_target_free(host);
    
    printf("✓ Host detection tests passed\n");
}

void test_target_features() {
    printf("Testing target features...\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    
    // Add features
    bool result = wyn_target_add_feature(target, "sse4.2");
    assert(result == true);
    assert(target->feature_count == 1);
    
    result = wyn_target_add_feature(target, "avx2");
    assert(result == true);
    assert(target->feature_count == 2);
    
    // Check features
    assert(wyn_target_has_feature(target, "sse4.2") == true);
    assert(wyn_target_has_feature(target, "avx2") == true);
    assert(wyn_target_has_feature(target, "nonexistent") == false);
    
    // Test default features
    size_t count;
    char** default_features = wyn_target_get_default_features(WYN_ARCH_X86_64, &count);
    assert(default_features != NULL);
    assert(count > 0);
    
    for (size_t i = 0; i < count; i++) {
        free(default_features[i]);
    }
    free(default_features);
    
    wyn_target_free(target);
    
    printf("✓ Target feature tests passed\n");
}

void test_cross_compiler() {
    printf("Testing cross-compiler...\n");
    
    // Create cross-compiler
    WynCrossCompiler* compiler = wyn_cross_compiler_new();
    assert(compiler != NULL);
    assert(compiler->host_target != NULL);
    assert(compiler->target_count == 1); // Host target is added by default
    
    // Add additional targets
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    bool result = wyn_cross_compiler_add_target(compiler, linux_target);
    assert(result == true);
    assert(compiler->target_count == 2);
    
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    result = wyn_cross_compiler_add_target(compiler, windows_target);
    assert(result == true);
    assert(compiler->target_count == 3);
    
    // Find targets
    WynTarget* found = wyn_cross_compiler_find_target(compiler, linux_target->triple);
    assert(found == linux_target);
    
    WynTarget* not_found = wyn_cross_compiler_find_target(compiler, "nonexistent-target");
    assert(not_found == NULL);
    
    wyn_cross_compiler_free(compiler);
    
    printf("✓ Cross-compiler tests passed\n");
}

void test_platform_config() {
    printf("Testing platform configuration...\n");
    
    // Test Linux configuration
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynPlatformConfig* linux_config = wyn_platform_config_for_target(linux_target);
    assert(linux_config != NULL);
    assert(linux_config->linker != NULL);
    assert(linux_config->archiver != NULL);
    assert(linux_config->system_lib_count > 0);
    
    // Test Windows configuration
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynPlatformConfig* windows_config = wyn_platform_config_for_target(windows_target);
    assert(windows_config != NULL);
    assert(windows_config->linker != NULL);
    assert(windows_config->archiver != NULL);
    
    // Test WASI configuration
    WynTarget* wasi_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    WynPlatformConfig* wasi_config = wyn_platform_config_for_target(wasi_target);
    assert(wasi_config != NULL);
    assert(wasi_config->linker != NULL);
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(wasi_target);
    wyn_platform_config_free(linux_config);
    wyn_platform_config_free(windows_config);
    wyn_platform_config_free(wasi_config);
    
    printf("✓ Platform configuration tests passed\n");
}

void test_target_utilities() {
    printf("Testing target utilities...\n");
    
    // Test architecture names
    assert(strcmp(wyn_target_arch_name(WYN_ARCH_X86_64), "x86_64") == 0);
    assert(strcmp(wyn_target_arch_name(WYN_ARCH_ARM64), "aarch64") == 0);
    assert(strcmp(wyn_target_arch_name(WYN_ARCH_WASM32), "wasm32") == 0);
    
    // Test OS names
    assert(strcmp(wyn_target_os_name(WYN_OS_LINUX), "linux") == 0);
    assert(strcmp(wyn_target_os_name(WYN_OS_WINDOWS), "windows") == 0);
    assert(strcmp(wyn_target_os_name(WYN_OS_MACOS), "darwin") == 0);
    assert(strcmp(wyn_target_os_name(WYN_OS_WASI), "wasi") == 0);
    
    // Test environment names
    assert(strcmp(wyn_target_env_name(WYN_ENV_GNU), "gnu") == 0);
    assert(strcmp(wyn_target_env_name(WYN_ENV_MSVC), "msvc") == 0);
    assert(strcmp(wyn_target_env_name(WYN_ENV_WASI), "wasi") == 0);
    
    // Test file extensions
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    assert(strcmp(wyn_target_executable_extension(windows_target), ".exe") == 0);
    assert(strcmp(wyn_target_library_extension(windows_target, true), ".dll") == 0);
    assert(strcmp(wyn_target_library_extension(windows_target, false), ".lib") == 0);
    assert(strcmp(wyn_target_object_extension(windows_target), ".obj") == 0);
    
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    assert(strcmp(wyn_target_executable_extension(linux_target), "") == 0);
    assert(strcmp(wyn_target_library_extension(linux_target, true), ".so") == 0);
    assert(strcmp(wyn_target_library_extension(linux_target, false), ".a") == 0);
    assert(strcmp(wyn_target_object_extension(linux_target), ".o") == 0);
    
    wyn_target_free(windows_target);
    wyn_target_free(linux_target);
    
    printf("✓ Target utility tests passed\n");
}

void test_supported_targets() {
    printf("Testing supported targets...\n");
    
    // Get supported targets
    size_t count;
    char** targets = wyn_get_supported_targets(&count);
    assert(targets != NULL);
    assert(count > 0);
    
    printf("Supported targets (%zu):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", targets[i]);
        
        // Parse each target to ensure it's valid
        WynTarget* target = wyn_target_parse(targets[i]);
        assert(target != NULL);
        assert(wyn_target_is_supported(target) == true);
        
        wyn_target_free(target);
        free(targets[i]);
    }
    free(targets);
    
    printf("✓ Supported target tests passed\n");
}

void test_conditional_compilation() {
    printf("Testing conditional compilation...\n");
    
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    
    // Test OS conditions
    assert(wyn_evaluate_cfg_condition("target_os = \"linux\"", linux_target) == true);
    assert(wyn_evaluate_cfg_condition("target_os = \"linux\"", windows_target) == false);
    assert(wyn_evaluate_cfg_condition("target_os = \"windows\"", windows_target) == true);
    assert(wyn_evaluate_cfg_condition("target_os = \"windows\"", linux_target) == false);
    
    // Test architecture conditions
    assert(wyn_evaluate_cfg_condition("target_arch = \"x86_64\"", linux_target) == true);
    assert(wyn_evaluate_cfg_condition("target_arch = \"x86_64\"", windows_target) == true);
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    
    printf("✓ Conditional compilation tests passed\n");
}

void test_wasm_config() {
    printf("Testing WebAssembly configuration...\n");
    
    WynWasmConfig* config = wyn_wasm_config_new();
    assert(config != NULL);
    assert(config->enable_bulk_memory == true);
    assert(config->initial_memory == 1024 * 1024);
    assert(config->max_memory == 16 * 1024 * 1024);
    
    wyn_wasm_config_free(config);
    
    printf("✓ WebAssembly configuration tests passed\n");
}

int main() {
    printf("Running Cross-Platform Compilation Tests\n");
    printf("========================================\n\n");
    
    test_target_management();
    test_target_parsing();
    test_host_detection();
    test_target_features();
    test_cross_compiler();
    test_platform_config();
    test_target_utilities();
    test_supported_targets();
    test_conditional_compilation();
    test_wasm_config();
    
    printf("\n✓ All cross-platform compilation tests passed!\n");
    printf("Cross-platform compilation provides:\n");
    printf("  - Multi-target support (x86_64, ARM64, WebAssembly)\n");
    printf("  - Cross-compilation from any host to any target\n");
    printf("  - Platform-specific configuration and toolchains\n");
    printf("  - Target feature detection and management\n");
    printf("  - Conditional compilation support\n");
    printf("  - WebAssembly-specific configuration\n");
    printf("  - File extension and naming conventions per platform\n");
    
    return 0;
}
