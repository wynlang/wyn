#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform.h"

void example_feature_flags() {
    printf("=== Feature Flags Example ===\n");
    
    // Create target and feature flags
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynFeatureFlags* flags = wyn_feature_flags_new(target);
    
    printf("Platform: %s\n", target->triple);
    printf("Standard features automatically enabled:\n");
    
    // List enabled features
    size_t count;
    char** enabled = wyn_feature_flags_list_enabled(flags, &count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", enabled[i]);
        free(enabled[i]);
    }
    free(enabled);
    
    // Add custom features
    printf("\nAdding custom features:\n");
    wyn_feature_flags_add(flags, "networking", WYN_FEATURE_OPTIONAL, "Network support");
    wyn_feature_flags_add(flags, "graphics", WYN_FEATURE_OPTIONAL, "Graphics support");
    wyn_feature_flags_add(flags, "audio", WYN_FEATURE_EXPERIMENTAL, "Audio support");
    
    // Enable some features
    wyn_feature_flags_enable(flags, "networking");
    wyn_feature_flags_enable(flags, "graphics");
    
    printf("  - networking: %s\n", wyn_feature_flags_is_enabled(flags, "networking") ? "enabled" : "disabled");
    printf("  - graphics: %s\n", wyn_feature_flags_is_enabled(flags, "graphics") ? "enabled" : "disabled");
    printf("  - audio: %s\n", wyn_feature_flags_is_enabled(flags, "audio") ? "enabled" : "disabled");
    
    // Get feature details
    WynFeatureFlag* net_feature = wyn_feature_flags_get(flags, "networking");
    if (net_feature) {
        printf("\nNetworking feature details:\n");
        printf("  - Name: %s\n", net_feature->name);
        printf("  - Type: %s\n", net_feature->type == WYN_FEATURE_OPTIONAL ? "optional" : "other");
        printf("  - Description: %s\n", net_feature->description);
        printf("  - Enabled: %s\n", net_feature->enabled ? "yes" : "no");
    }
    
    wyn_feature_flags_free(flags);
    wyn_target_free(target);
    printf("\n");
}

void example_conditional_compilation() {
    printf("=== Conditional Compilation Example ===\n");
    
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynFeatureFlags* flags = wyn_feature_flags_new(linux_target);
    
    // Create cfg expressions
    printf("Testing cfg expressions:\n");
    
    WynCfgExpression* linux_expr = wyn_cfg_parse("target_os = \"linux\"");
    WynCfgExpression* windows_expr = wyn_cfg_parse("target_os = \"windows\"");
    WynCfgExpression* x64_expr = wyn_cfg_parse("target_arch = \"x86_64\"");
    
    printf("  - cfg(target_os = \"linux\") on Linux: %s\n", 
           wyn_cfg_evaluate(linux_expr, linux_target, flags) ? "true" : "false");
    printf("  - cfg(target_os = \"linux\") on Windows: %s\n", 
           wyn_cfg_evaluate(linux_expr, windows_target, flags) ? "true" : "false");
    printf("  - cfg(target_os = \"windows\") on Windows: %s\n", 
           wyn_cfg_evaluate(windows_expr, windows_target, flags) ? "true" : "false");
    printf("  - cfg(target_arch = \"x86_64\") on both: %s\n", 
           wyn_cfg_evaluate(x64_expr, linux_target, flags) ? "true" : "false");
    
    // Create conditional code blocks
    printf("\nConditional code blocks:\n");
    
    WynConditionalBlock* linux_block = wyn_conditional_block_new(
        wyn_cfg_parse("target_os = \"linux\""),
        "use std::os::unix::*;\nfn get_pid() -> u32 { unsafe { libc::getpid() as u32 } }"
    );
    
    WynConditionalBlock* windows_block = wyn_conditional_block_new(
        wyn_cfg_parse("target_os = \"windows\""),
        "use std::os::windows::*;\nfn get_pid() -> u32 { unsafe { kernel32::GetCurrentProcessId() } }"
    );
    
    linux_block->next = windows_block;
    
    // Generate platform-specific code
    char* linux_code = wyn_conditional_block_get_active_code(linux_block, linux_target, flags);
    char* windows_code = wyn_conditional_block_get_active_code(linux_block, windows_target, flags);
    
    printf("Active code for Linux target:\n%s\n", linux_code ? linux_code : "(none)");
    printf("Active code for Windows target:\n%s\n", windows_code ? windows_code : "(none)");
    
    free(linux_code);
    free(windows_code);
    wyn_cfg_expression_free(linux_expr);
    wyn_cfg_expression_free(windows_expr);
    wyn_cfg_expression_free(x64_expr);
    wyn_conditional_block_free(linux_block);
    wyn_feature_flags_free(flags);
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    printf("\n");
}

void example_platform_constants() {
    printf("=== Platform Constants Example ===\n");
    
    // Test different platforms
    WynTarget* targets[] = {
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC),
        wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI)
    };
    
    const char* names[] = {"Linux", "Windows", "macOS", "WebAssembly"};
    
    for (int i = 0; i < 4; i++) {
        WynPlatformConstants* constants = wyn_platform_get_constants(targets[i]);
        
        printf("%s platform constants:\n", names[i]);
        printf("  - Path separator: '%s'\n", constants->path_separator);
        printf("  - Line ending: ");
        if (strcmp(constants->line_ending, "\n") == 0) {
            printf("LF (Unix)\n");
        } else if (strcmp(constants->line_ending, "\r\n") == 0) {
            printf("CRLF (Windows)\n");
        } else {
            printf("'%s'\n", constants->line_ending);
        }
        printf("  - Library prefix: '%s'\n", constants->library_prefix);
        printf("  - Library suffix: '%s'\n", constants->library_suffix);
        printf("  - Executable suffix: '%s'\n", constants->executable_suffix);
        printf("  - Max path length: %zu\n", constants->max_path_length);
        printf("  - Page size: %zu bytes\n", constants->page_size);
        printf("\n");
        
        wyn_platform_constants_free(constants);
        wyn_target_free(targets[i]);
    }
}

void example_platform_capabilities() {
    printf("=== Platform Capabilities Example ===\n");
    
    WynTarget* targets[] = {
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC),
        wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI),
        wyn_target_new(WYN_ARCH_WASM32, WYN_OS_BROWSER, WYN_ENV_EMSCRIPTEN)
    };
    
    const char* names[] = {"Linux", "Windows", "macOS", "WASI", "Browser"};
    
    printf("Platform capability matrix:\n");
    printf("Platform    | Filesystem | Network | Threads | GUI | Unix-like | POSIX\n");
    printf("------------|------------|---------|---------|-----|-----------|------\n");
    
    for (int i = 0; i < 5; i++) {
        printf("%-11s | %-10s | %-7s | %-7s | %-3s | %-9s | %s\n",
               names[i],
               wyn_platform_has_filesystem(targets[i]) ? "Yes" : "No",
               wyn_platform_has_network(targets[i]) ? "Yes" : "No",
               wyn_platform_has_threads(targets[i]) ? "Yes" : "No",
               wyn_platform_has_gui(targets[i]) ? "Yes" : "No",
               wyn_platform_is_unix_like(targets[i]) ? "Yes" : "No",
               wyn_platform_supports_posix(targets[i]) ? "Yes" : "No");
        
        wyn_target_free(targets[i]);
    }
    printf("\n");
}

void example_platform_interface() {
    printf("=== Platform Interface Example ===\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    
    // Create unified platform interface
    WynPlatformInterface* interface = wyn_platform_interface_new(target);
    
    printf("Created platform interface for: %s\n", target->triple);
    printf("Standard features available: %zu\n", interface->feature_flags->flag_count);
    
    // Create platform-specific APIs
    WynPlatformAPI* fs_api = wyn_platform_api_new(WYN_PLATFORM_FILESYSTEM, target);
    WynPlatformAPI* net_api = wyn_platform_api_new(WYN_PLATFORM_NETWORK, target);
    WynPlatformAPI* proc_api = wyn_platform_api_new(WYN_PLATFORM_PROCESS, target);
    
    // Add mock functions to APIs
    wyn_platform_api_add_function(fs_api, "open", (WynPlatformFunction)1, "fn open(path: str) -> File");
    wyn_platform_api_add_function(fs_api, "read", (WynPlatformFunction)2, "fn read(file: File, buf: &mut [u8]) -> usize");
    wyn_platform_api_add_function(net_api, "connect", (WynPlatformFunction)3, "fn connect(addr: str) -> Socket");
    wyn_platform_api_add_function(proc_api, "spawn", (WynPlatformFunction)4, "fn spawn(cmd: str) -> Process");
    
    // Add APIs to interface
    wyn_platform_interface_add_api(interface, fs_api);
    wyn_platform_interface_add_api(interface, net_api);
    wyn_platform_interface_add_api(interface, proc_api);
    
    printf("Added APIs: %zu\n", interface->api_count);
    
    // Access APIs by category
    WynPlatformAPI* found_fs = wyn_platform_interface_get_api(interface, WYN_PLATFORM_FILESYSTEM);
    if (found_fs) {
        printf("\nFilesystem API functions:\n");
        for (size_t i = 0; i < found_fs->function_count; i++) {
            printf("  - %s: %s\n", found_fs->functions[i].name, 
                   found_fs->functions[i].signature ? found_fs->functions[i].signature : "no signature");
        }
    }
    
    WynPlatformAPI* found_net = wyn_platform_interface_get_api(interface, WYN_PLATFORM_NETWORK);
    if (found_net) {
        printf("\nNetwork API functions:\n");
        for (size_t i = 0; i < found_net->function_count; i++) {
            printf("  - %s: %s\n", found_net->functions[i].name,
                   found_net->functions[i].signature ? found_net->functions[i].signature : "no signature");
        }
    }
    
    wyn_platform_interface_free(interface);
    wyn_target_free(target);
    printf("\n");
}

void example_feature_based_compilation() {
    printf("=== Feature-Based Compilation Example ===\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynFeatureFlags* flags = wyn_feature_flags_new(target);
    
    // Add application-specific features
    wyn_feature_flags_add(flags, "database", WYN_FEATURE_OPTIONAL, "Database support");
    wyn_feature_flags_add(flags, "encryption", WYN_FEATURE_OPTIONAL, "Encryption support");
    wyn_feature_flags_add(flags, "logging", WYN_FEATURE_REQUIRED, "Logging support");
    
    // Enable some features
    wyn_feature_flags_enable(flags, "database");
    wyn_feature_flags_enable(flags, "encryption");
    
    printf("Application features:\n");
    printf("  - database: %s\n", wyn_feature_flags_is_enabled(flags, "database") ? "enabled" : "disabled");
    printf("  - encryption: %s\n", wyn_feature_flags_is_enabled(flags, "encryption") ? "enabled" : "disabled");
    printf("  - logging: %s\n", wyn_feature_flags_is_enabled(flags, "logging") ? "enabled" : "disabled");
    
    // Create feature-dependent code blocks
    WynConditionalBlock* db_block = wyn_conditional_block_new(
        wyn_cfg_parse("feature = \"database\""),
        "mod database {\n    pub fn connect() -> Connection { ... }\n}"
    );
    
    WynConditionalBlock* crypto_block = wyn_conditional_block_new(
        wyn_cfg_parse("feature = \"encryption\""),
        "mod crypto {\n    pub fn encrypt(data: &[u8]) -> Vec<u8> { ... }\n}"
    );
    
    db_block->next = crypto_block;
    
    // Generate feature-dependent code
    char* active_code = wyn_conditional_block_get_active_code(db_block, target, flags);
    
    printf("\nGenerated code with enabled features:\n");
    printf("%s\n", active_code ? active_code : "(no conditional code)");
    
    // Disable database feature and regenerate
    wyn_feature_flags_disable(flags, "database");
    free(active_code);
    active_code = wyn_conditional_block_get_active_code(db_block, target, flags);
    
    printf("Generated code with database disabled:\n");
    printf("%s\n", active_code ? active_code : "(no conditional code)");
    
    free(active_code);
    wyn_conditional_block_free(db_block);
    wyn_feature_flags_free(flags);
    wyn_target_free(target);
    printf("\n");
}

void example_cross_platform_code() {
    printf("=== Cross-Platform Code Example ===\n");
    
    WynTarget* targets[] = {
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC),
        wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI)
    };
    
    const char* names[] = {"Linux", "Windows", "WebAssembly"};
    
    // Create platform-specific implementations
    WynConditionalBlock* linux_impl = wyn_conditional_block_new(
        wyn_cfg_parse("target_os = \"linux\""),
        "fn get_current_dir() -> String {\n    std::env::current_dir().unwrap().to_string_lossy().to_string()\n}"
    );
    
    WynConditionalBlock* windows_impl = wyn_conditional_block_new(
        wyn_cfg_parse("target_os = \"windows\""),
        "fn get_current_dir() -> String {\n    std::env::current_dir().unwrap().to_string_lossy().replace('/', \"\\\\\")\n}"
    );
    
    WynConditionalBlock* wasm_impl = wyn_conditional_block_new(
        wyn_cfg_parse("target_os = \"wasi\""),
        "fn get_current_dir() -> String {\n    \"/\".to_string() // WASI has limited filesystem\n}"
    );
    
    linux_impl->next = windows_impl;
    windows_impl->next = wasm_impl;
    
    // Generate code for each platform
    for (int i = 0; i < 3; i++) {
        WynFeatureFlags* flags = wyn_feature_flags_new(targets[i]);
        char* code = wyn_conditional_block_get_active_code(linux_impl, targets[i], flags);
        
        printf("%s implementation:\n", names[i]);
        printf("%s\n", code ? code : "(no implementation)");
        
        free(code);
        wyn_feature_flags_free(flags);
        wyn_target_free(targets[i]);
    }
    
    wyn_conditional_block_free(linux_impl);
    printf("\n");
}

int main() {
    printf("Wyn Platform Abstraction Examples\n");
    printf("==================================\n\n");
    
    example_feature_flags();
    example_conditional_compilation();
    example_platform_constants();
    example_platform_capabilities();
    example_platform_interface();
    example_feature_based_compilation();
    example_cross_platform_code();
    
    printf("Platform Abstraction Features:\n");
    printf("  ✓ Feature flags with optional/required/experimental types\n");
    printf("  ✓ Conditional compilation with cfg expressions and attributes\n");
    printf("  ✓ Platform-specific API management and function registration\n");
    printf("  ✓ Unified platform interface for multi-API coordination\n");
    printf("  ✓ Platform constants and utilities for cross-platform development\n");
    printf("  ✓ Standard feature detection and capability matrix\n");
    printf("  ✓ Feature-based compilation for modular applications\n");
    printf("  ✓ Cross-platform code generation with conditional blocks\n");
    printf("\nComplete platform abstraction for Wyn development!\n");
    
    return 0;
}
