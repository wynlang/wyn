#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/platform.h"

void test_feature_flags() {
    printf("Testing feature flags...\n");
    
    // Create target and feature flags
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynFeatureFlags* flags = wyn_feature_flags_new(target);
    
    assert(flags != NULL);
    assert(flags->target == target);
    assert(flags->flag_count > 0); // Should have standard features
    
    // Add custom feature
    bool result = wyn_feature_flags_add(flags, "custom_feature", WYN_FEATURE_OPTIONAL, 
                                       "A custom feature for testing");
    assert(result == true);
    
    // Check feature exists but is not enabled by default
    WynFeatureFlag* feature = wyn_feature_flags_get(flags, "custom_feature");
    assert(feature != NULL);
    assert(strcmp(feature->name, "custom_feature") == 0);
    assert(feature->type == WYN_FEATURE_OPTIONAL);
    assert(feature->enabled == false);
    
    // Enable feature
    result = wyn_feature_flags_enable(flags, "custom_feature");
    assert(result == true);
    assert(wyn_feature_flags_is_enabled(flags, "custom_feature") == true);
    
    // Disable feature
    result = wyn_feature_flags_disable(flags, "custom_feature");
    assert(result == true);
    assert(wyn_feature_flags_is_enabled(flags, "custom_feature") == false);
    
    // Test required features (should be enabled by default)
    assert(wyn_feature_flags_is_enabled(flags, "unix") == true);
    assert(wyn_feature_flags_is_enabled(flags, "posix") == true);
    
    // List enabled features
    size_t count;
    char** enabled = wyn_feature_flags_list_enabled(flags, &count);
    assert(enabled != NULL);
    assert(count > 0);
    
    for (size_t i = 0; i < count; i++) {
        free(enabled[i]);
    }
    free(enabled);
    
    wyn_feature_flags_free(flags);
    wyn_target_free(target);
    
    printf("✓ Feature flags tests passed\n");
}

void test_cfg_expressions() {
    printf("Testing cfg expressions...\n");
    
    // Create target
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynFeatureFlags* flags = wyn_feature_flags_new(linux_target);
    
    // Test OS condition
    WynCfgExpression* linux_expr = wyn_cfg_parse("target_os = \"linux\"");
    assert(linux_expr != NULL);
    assert(linux_expr->op == WYN_CFG_EQUALS);
    assert(strcmp(linux_expr->key, "target_os") == 0);
    assert(strcmp(linux_expr->value, "linux") == 0);
    
    // Evaluate expressions
    assert(wyn_cfg_evaluate(linux_expr, linux_target, flags) == true);
    assert(wyn_cfg_evaluate(linux_expr, windows_target, flags) == false);
    
    // Test architecture condition
    WynCfgExpression* arch_expr = wyn_cfg_parse("target_arch = \"x86_64\"");
    assert(arch_expr != NULL);
    assert(wyn_cfg_evaluate(arch_expr, linux_target, flags) == true);
    assert(wyn_cfg_evaluate(arch_expr, windows_target, flags) == true);
    
    // Test feature condition
    wyn_feature_flags_add(flags, "test_feature", WYN_FEATURE_OPTIONAL, "Test feature");
    wyn_feature_flags_enable(flags, "test_feature");
    
    WynCfgExpression* feature_expr = wyn_cfg_parse("feature = \"test_feature\"");
    assert(feature_expr != NULL);
    assert(wyn_cfg_evaluate(feature_expr, linux_target, flags) == true);
    
    // Test string representation
    char* linux_str = wyn_cfg_to_string(linux_expr);
    assert(linux_str != NULL);
    assert(strstr(linux_str, "target_os") != NULL);
    assert(strstr(linux_str, "linux") != NULL);
    
    free(linux_str);
    wyn_cfg_expression_free(linux_expr);
    wyn_cfg_expression_free(arch_expr);
    wyn_cfg_expression_free(feature_expr);
    wyn_feature_flags_free(flags);
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    
    printf("✓ Cfg expression tests passed\n");
}

void test_conditional_blocks() {
    printf("Testing conditional blocks...\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynFeatureFlags* flags = wyn_feature_flags_new(target);
    
    // Create conditional expressions
    WynCfgExpression* linux_expr = wyn_cfg_parse("target_os = \"linux\"");
    WynCfgExpression* windows_expr = wyn_cfg_parse("target_os = \"windows\"");
    
    // Create conditional blocks
    WynConditionalBlock* linux_block = wyn_conditional_block_new(linux_expr, 
                                                                "println(\"Running on Linux\");");
    WynConditionalBlock* windows_block = wyn_conditional_block_new(windows_expr, 
                                                                  "println(\"Running on Windows\");");
    
    assert(linux_block != NULL);
    assert(windows_block != NULL);
    
    // Link blocks
    linux_block->next = windows_block;
    
    // Evaluate blocks
    assert(wyn_conditional_block_evaluate(linux_block, target, flags) == true);
    assert(wyn_conditional_block_evaluate(windows_block, target, flags) == false);
    
    // Get active code
    char* active_code = wyn_conditional_block_get_active_code(linux_block, target, flags);
    assert(active_code != NULL);
    assert(strstr(active_code, "Running on Linux") != NULL);
    assert(strstr(active_code, "Running on Windows") == NULL);
    
    free(active_code);
    wyn_conditional_block_free(linux_block); // This will free windows_block too
    wyn_feature_flags_free(flags);
    wyn_target_free(target);
    
    printf("✓ Conditional block tests passed\n");
}

void test_platform_api() {
    printf("Testing platform API...\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    
    // Create platform API
    WynPlatformAPI* api = wyn_platform_api_new(WYN_PLATFORM_FILESYSTEM, target);
    assert(api != NULL);
    assert(api->category == WYN_PLATFORM_FILESYSTEM);
    assert(api->target == target);
    assert(api->feature_flags != NULL);
    
    // Add dummy function (pass NULL for function pointer in test)
    bool result = wyn_platform_api_add_function(api, "open_file", (WynPlatformFunction)1, "fn open_file(path: str) -> File");
    assert(result == true);
    assert(api->function_count == 1);
    
    // Get function
    WynPlatformAPIFunction* func = wyn_platform_api_get_function(api, "open_file");
    assert(func != NULL);
    assert(strcmp(func->name, "open_file") == 0);
    assert(func->signature != NULL);
    
    // Check availability
    assert(wyn_platform_api_is_available(api, "open_file") == true);
    assert(wyn_platform_api_is_available(api, "nonexistent") == false);
    
    wyn_platform_api_free(api);
    wyn_target_free(target);
    
    printf("✓ Platform API tests passed\n");
}

void test_platform_interface() {
    printf("Testing platform interface...\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    
    // Create platform interface
    WynPlatformInterface* interface = wyn_platform_interface_new(target);
    assert(interface != NULL);
    assert(interface->target == target);
    assert(interface->feature_flags != NULL);
    
    // Create and add APIs
    WynPlatformAPI* fs_api = wyn_platform_api_new(WYN_PLATFORM_FILESYSTEM, target);
    WynPlatformAPI* net_api = wyn_platform_api_new(WYN_PLATFORM_NETWORK, target);
    
    bool result = wyn_platform_interface_add_api(interface, fs_api);
    assert(result == true);
    assert(interface->api_count == 1);
    
    result = wyn_platform_interface_add_api(interface, net_api);
    assert(result == true);
    assert(interface->api_count == 2);
    
    // Get APIs by category
    WynPlatformAPI* found_fs = wyn_platform_interface_get_api(interface, WYN_PLATFORM_FILESYSTEM);
    WynPlatformAPI* found_net = wyn_platform_interface_get_api(interface, WYN_PLATFORM_NETWORK);
    
    assert(found_fs == fs_api);
    assert(found_net == net_api);
    
    // Test nonexistent category
    WynPlatformAPI* not_found = wyn_platform_interface_get_api(interface, WYN_PLATFORM_GUI);
    assert(not_found == NULL);
    
    wyn_platform_interface_free(interface);
    wyn_target_free(target);
    
    printf("✓ Platform interface tests passed\n");
}

void test_platform_constants() {
    printf("Testing platform constants...\n");
    
    // Test Linux constants
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynPlatformConstants* linux_constants = wyn_platform_get_constants(linux_target);
    
    assert(linux_constants != NULL);
    assert(strcmp(linux_constants->path_separator, "/") == 0);
    assert(strcmp(linux_constants->line_ending, "\n") == 0);
    assert(strcmp(linux_constants->library_prefix, "lib") == 0);
    assert(strcmp(linux_constants->library_suffix, ".so") == 0);
    assert(strcmp(linux_constants->executable_suffix, "") == 0);
    assert(linux_constants->max_path_length == 4096);
    assert(linux_constants->page_size == 4096);
    
    // Test Windows constants
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynPlatformConstants* windows_constants = wyn_platform_get_constants(windows_target);
    
    assert(windows_constants != NULL);
    assert(strcmp(windows_constants->path_separator, "\\") == 0);
    assert(strcmp(windows_constants->line_ending, "\r\n") == 0);
    assert(strcmp(windows_constants->library_prefix, "") == 0);
    assert(strcmp(windows_constants->library_suffix, ".dll") == 0);
    assert(strcmp(windows_constants->executable_suffix, ".exe") == 0);
    assert(windows_constants->max_path_length == 260);
    
    // Test WASI constants
    WynTarget* wasi_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    WynPlatformConstants* wasi_constants = wyn_platform_get_constants(wasi_target);
    
    assert(wasi_constants != NULL);
    assert(strcmp(wasi_constants->path_separator, "/") == 0);
    assert(strcmp(wasi_constants->executable_suffix, ".wasm") == 0);
    assert(wasi_constants->page_size == 65536); // WASM page size
    
    wyn_platform_constants_free(linux_constants);
    wyn_platform_constants_free(windows_constants);
    wyn_platform_constants_free(wasi_constants);
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(wasi_target);
    
    printf("✓ Platform constants tests passed\n");
}

void test_platform_utilities() {
    printf("Testing platform utilities...\n");
    
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* macos_target = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    WynTarget* wasi_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    
    // Test Unix-like detection
    assert(wyn_platform_is_unix_like(linux_target) == true);
    assert(wyn_platform_is_unix_like(macos_target) == true);
    assert(wyn_platform_is_unix_like(windows_target) == false);
    assert(wyn_platform_is_unix_like(wasi_target) == false);
    
    // Test Windows-like detection
    assert(wyn_platform_is_windows_like(windows_target) == true);
    assert(wyn_platform_is_windows_like(linux_target) == false);
    assert(wyn_platform_is_windows_like(macos_target) == false);
    
    // Test POSIX support
    assert(wyn_platform_supports_posix(linux_target) == true);
    assert(wyn_platform_supports_posix(macos_target) == true);
    assert(wyn_platform_supports_posix(windows_target) == false);
    
    // Test Win32 support
    assert(wyn_platform_supports_win32(windows_target) == true);
    assert(wyn_platform_supports_win32(linux_target) == false);
    
    // Test capability detection
    assert(wyn_platform_has_filesystem(linux_target) == true);
    assert(wyn_platform_has_filesystem(windows_target) == true);
    assert(wyn_platform_has_network(linux_target) == true);
    assert(wyn_platform_has_threads(linux_target) == true);
    assert(wyn_platform_has_gui(linux_target) == true);
    
    // WASI has limited capabilities
    assert(wyn_platform_has_filesystem(wasi_target) == true);
    assert(wyn_platform_has_network(wasi_target) == false);
    assert(wyn_platform_has_threads(wasi_target) == false);
    
    // Test string getters
    assert(strcmp(wyn_platform_get_arch_string(linux_target), "x86_64") == 0);
    assert(strcmp(wyn_platform_get_os_string(linux_target), "linux") == 0);
    assert(strcmp(wyn_platform_get_arch_string(macos_target), "aarch64") == 0);
    assert(strcmp(wyn_platform_get_os_string(macos_target), "darwin") == 0);
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(macos_target);
    wyn_target_free(wasi_target);
    
    printf("✓ Platform utility tests passed\n");
}

void test_standard_features() {
    printf("Testing standard features...\n");
    
    // Test Linux features
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynFeatureFlags* linux_flags = wyn_feature_flags_new(linux_target);
    
    assert(wyn_feature_flags_is_enabled(linux_flags, "unix") == true);
    assert(wyn_feature_flags_is_enabled(linux_flags, "posix") == true);
    assert(wyn_feature_flags_is_enabled(linux_flags, "x86_64") == true);
    
    // Test Windows features
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynFeatureFlags* windows_flags = wyn_feature_flags_new(windows_target);
    
    assert(wyn_feature_flags_is_enabled(windows_flags, "windows") == true);
    assert(wyn_feature_flags_is_enabled(windows_flags, "unicode") == true);
    assert(wyn_feature_flags_is_enabled(windows_flags, "x86_64") == true);
    
    // Test WASI features
    WynTarget* wasi_target = wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI);
    WynFeatureFlags* wasi_flags = wyn_feature_flags_new(wasi_target);
    
    assert(wyn_feature_flags_is_enabled(wasi_flags, "wasm") == true);
    assert(wyn_feature_flags_is_enabled(wasi_flags, "wasi") == true);
    assert(wyn_feature_flags_is_enabled(wasi_flags, "wasm32") == true);
    
    wyn_feature_flags_free(linux_flags);
    wyn_feature_flags_free(windows_flags);
    wyn_feature_flags_free(wasi_flags);
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(wasi_target);
    
    printf("✓ Standard features tests passed\n");
}

int main() {
    printf("Running Platform Abstraction Tests\n");
    printf("===================================\n\n");
    
    test_feature_flags();
    test_cfg_expressions();
    test_conditional_blocks();
    test_platform_api();
    test_platform_interface();
    test_platform_constants();
    test_platform_utilities();
    test_standard_features();
    
    printf("\n✓ All platform abstraction tests passed!\n");
    printf("Platform abstraction provides:\n");
    printf("  - Feature flags with optional/required/experimental types\n");
    printf("  - Conditional compilation with cfg expressions\n");
    printf("  - Platform-specific API management\n");
    printf("  - Unified platform interface\n");
    printf("  - Platform constants and utilities\n");
    printf("  - Standard feature detection per platform\n");
    printf("  - Cross-platform capability detection\n");
    
    return 0;
}
