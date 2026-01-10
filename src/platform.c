#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Feature flags management
WynFeatureFlags* wyn_feature_flags_new(WynTarget* target) {
    if (!target) return NULL;
    
    WynFeatureFlags* flags = malloc(sizeof(WynFeatureFlags));
    if (!flags) return NULL;
    
    memset(flags, 0, sizeof(WynFeatureFlags));
    flags->target = target;
    flags->flag_capacity = 32;
    flags->flags = malloc(flags->flag_capacity * sizeof(WynFeatureFlag));
    
    // Initialize standard features for target
    wyn_platform_init_standard_features(flags, target);
    
    return flags;
}

void wyn_feature_flags_free(WynFeatureFlags* flags) {
    if (!flags) return;
    
    for (size_t i = 0; i < flags->flag_count; i++) {
        free(flags->flags[i].name);
        free(flags->flags[i].description);
        free(flags->flags[i].since_version);
        
        for (size_t j = 0; j < flags->flags[i].dependency_count; j++) {
            free(flags->flags[i].dependencies[j]);
        }
        free(flags->flags[i].dependencies);
    }
    
    free(flags->flags);
    free(flags);
}

bool wyn_feature_flags_add(WynFeatureFlags* flags, const char* name, WynFeatureType type, 
                          const char* description) {
    if (!flags || !name) return false;
    
    // Check if flag already exists
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (strcmp(flags->flags[i].name, name) == 0) {
            return false; // Already exists
        }
    }
    
    // Resize if needed
    if (flags->flag_count >= flags->flag_capacity) {
        flags->flag_capacity *= 2;
        flags->flags = realloc(flags->flags, flags->flag_capacity * sizeof(WynFeatureFlag));
        if (!flags->flags) return false;
    }
    
    // Add new flag
    WynFeatureFlag* flag = &flags->flags[flags->flag_count];
    memset(flag, 0, sizeof(WynFeatureFlag));
    
    flag->name = strdup(name);
    flag->type = type;
    flag->enabled = (type == WYN_FEATURE_REQUIRED);
    flag->description = description ? strdup(description) : NULL;
    flag->since_version = strdup("1.0.0");
    
    flags->flag_count++;
    return true;
}

bool wyn_feature_flags_enable(WynFeatureFlags* flags, const char* name) {
    if (!flags || !name) return false;
    
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (strcmp(flags->flags[i].name, name) == 0) {
            flags->flags[i].enabled = true;
            return true;
        }
    }
    
    return false;
}

bool wyn_feature_flags_disable(WynFeatureFlags* flags, const char* name) {
    if (!flags || !name) return false;
    
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (strcmp(flags->flags[i].name, name) == 0) {
            if (flags->flags[i].type != WYN_FEATURE_REQUIRED) {
                flags->flags[i].enabled = false;
                return true;
            }
            return false; // Cannot disable required features
        }
    }
    
    return false;
}

bool wyn_feature_flags_is_enabled(WynFeatureFlags* flags, const char* name) {
    if (!flags || !name) return false;
    
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (strcmp(flags->flags[i].name, name) == 0) {
            return flags->flags[i].enabled;
        }
    }
    
    return false;
}

WynFeatureFlag* wyn_feature_flags_get(WynFeatureFlags* flags, const char* name) {
    if (!flags || !name) return NULL;
    
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (strcmp(flags->flags[i].name, name) == 0) {
            return &flags->flags[i];
        }
    }
    
    return NULL;
}

char** wyn_feature_flags_list_enabled(WynFeatureFlags* flags, size_t* count) {
    if (!flags || !count) return NULL;
    
    // Count enabled flags
    size_t enabled_count = 0;
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (flags->flags[i].enabled) {
            enabled_count++;
        }
    }
    
    if (enabled_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // Create array of enabled flag names
    char** enabled_flags = malloc(enabled_count * sizeof(char*));
    if (!enabled_flags) {
        *count = 0;
        return NULL;
    }
    
    size_t index = 0;
    for (size_t i = 0; i < flags->flag_count; i++) {
        if (flags->flags[i].enabled) {
            enabled_flags[index++] = strdup(flags->flags[i].name);
        }
    }
    
    *count = enabled_count;
    return enabled_flags;
}

// Conditional compilation
WynCfgExpression* wyn_cfg_expression_new(WynCfgOperator op, const char* key, const char* value) {
    WynCfgExpression* expr = malloc(sizeof(WynCfgExpression));
    if (!expr) return NULL;
    
    memset(expr, 0, sizeof(WynCfgExpression));
    expr->op = op;
    expr->key = key ? strdup(key) : NULL;
    expr->value = value ? strdup(value) : NULL;
    
    return expr;
}

void wyn_cfg_expression_free(WynCfgExpression* expr) {
    if (!expr) return;
    
    free(expr->key);
    free(expr->value);
    wyn_cfg_expression_free(expr->left);
    wyn_cfg_expression_free(expr->right);
    free(expr);
}

WynCfgExpression* wyn_cfg_parse(const char* cfg_string) {
    if (!cfg_string) return NULL;
    
    // Simple parsing for basic cfg expressions
    if (strstr(cfg_string, "target_os")) {
        const char* equals_pos = strstr(cfg_string, "=");
        if (equals_pos) {
            // Extract value (remove quotes)
            const char* value_start = equals_pos + 1;
            while (*value_start == ' ' || *value_start == '"') value_start++;
            
            char* value = strdup(value_start);
            char* value_end = strrchr(value, '"');
            if (value_end) *value_end = '\0';
            
            WynCfgExpression* expr = wyn_cfg_expression_new(WYN_CFG_EQUALS, "target_os", value);
            free(value);
            return expr;
        }
    }
    
    if (strstr(cfg_string, "target_arch")) {
        const char* equals_pos = strstr(cfg_string, "=");
        if (equals_pos) {
            const char* value_start = equals_pos + 1;
            while (*value_start == ' ' || *value_start == '"') value_start++;
            
            char* value = strdup(value_start);
            char* value_end = strrchr(value, '"');
            if (value_end) *value_end = '\0';
            
            WynCfgExpression* expr = wyn_cfg_expression_new(WYN_CFG_EQUALS, "target_arch", value);
            free(value);
            return expr;
        }
    }
    
    if (strstr(cfg_string, "feature")) {
        const char* equals_pos = strstr(cfg_string, "=");
        if (equals_pos) {
            const char* value_start = equals_pos + 1;
            while (*value_start == ' ' || *value_start == '"') value_start++;
            
            char* value = strdup(value_start);
            char* value_end = strrchr(value, '"');
            if (value_end) *value_end = '\0';
            
            WynCfgExpression* expr = wyn_cfg_expression_new(WYN_CFG_EQUALS, "feature", value);
            free(value);
            return expr;
        }
    }
    
    return NULL;
}

bool wyn_cfg_evaluate(WynCfgExpression* expr, WynTarget* target, WynFeatureFlags* flags) {
    if (!expr || !target) return false;
    
    switch (expr->op) {
        case WYN_CFG_EQUALS:
            if (strcmp(expr->key, "target_os") == 0) {
                const char* target_os = wyn_target_os_name(target->os);
                return strcmp(expr->value, target_os) == 0;
            }
            if (strcmp(expr->key, "target_arch") == 0) {
                const char* target_arch = wyn_target_arch_name(target->arch);
                return strcmp(expr->value, target_arch) == 0;
            }
            if (strcmp(expr->key, "feature") == 0 && flags) {
                return wyn_feature_flags_is_enabled(flags, expr->value);
            }
            break;
            
        case WYN_CFG_NOT:
            if (expr->left) {
                return !wyn_cfg_evaluate(expr->left, target, flags);
            }
            break;
            
        case WYN_CFG_AND:
            if (expr->left && expr->right) {
                return wyn_cfg_evaluate(expr->left, target, flags) && 
                       wyn_cfg_evaluate(expr->right, target, flags);
            }
            break;
            
        case WYN_CFG_OR:
            if (expr->left && expr->right) {
                return wyn_cfg_evaluate(expr->left, target, flags) || 
                       wyn_cfg_evaluate(expr->right, target, flags);
            }
            break;
            
        default:
            break;
    }
    
    return false;
}

char* wyn_cfg_to_string(WynCfgExpression* expr) {
    if (!expr) return NULL;
    
    switch (expr->op) {
        case WYN_CFG_EQUALS: {
            size_t len = strlen(expr->key) + strlen(expr->value) + 10;
            char* result = malloc(len);
            snprintf(result, len, "%s = \"%s\"", expr->key, expr->value);
            return result;
        }
        
        case WYN_CFG_NOT: {
            char* left_str = wyn_cfg_to_string(expr->left);
            if (!left_str) return NULL;
            
            size_t len = strlen(left_str) + 10;
            char* result = malloc(len);
            snprintf(result, len, "not(%s)", left_str);
            free(left_str);
            return result;
        }
        
        case WYN_CFG_AND: {
            char* left_str = wyn_cfg_to_string(expr->left);
            char* right_str = wyn_cfg_to_string(expr->right);
            if (!left_str || !right_str) {
                free(left_str);
                free(right_str);
                return NULL;
            }
            
            size_t len = strlen(left_str) + strlen(right_str) + 10;
            char* result = malloc(len);
            snprintf(result, len, "(%s) and (%s)", left_str, right_str);
            free(left_str);
            free(right_str);
            return result;
        }
        
        default:
            return strdup("unknown");
    }
}

// Conditional blocks
WynConditionalBlock* wyn_conditional_block_new(WynCfgExpression* condition, const char* code) {
    if (!condition || !code) return NULL;
    
    WynConditionalBlock* block = malloc(sizeof(WynConditionalBlock));
    if (!block) return NULL;
    
    memset(block, 0, sizeof(WynConditionalBlock));
    block->condition = condition;
    block->code_block = strdup(code);
    block->is_active = false;
    
    return block;
}

void wyn_conditional_block_free(WynConditionalBlock* block) {
    if (!block) return;
    
    wyn_cfg_expression_free(block->condition);
    free(block->code_block);
    
    if (block->next) {
        wyn_conditional_block_free(block->next);
    }
    
    free(block);
}

bool wyn_conditional_block_evaluate(WynConditionalBlock* block, WynTarget* target, 
                                   WynFeatureFlags* flags) {
    if (!block || !target) return false;
    
    block->is_active = wyn_cfg_evaluate(block->condition, target, flags);
    return block->is_active;
}

char* wyn_conditional_block_get_active_code(WynConditionalBlock* blocks, WynTarget* target, 
                                           WynFeatureFlags* flags) {
    if (!blocks || !target) return NULL;
    
    size_t total_length = 0;
    WynConditionalBlock* current = blocks;
    
    // First pass: calculate total length and evaluate blocks
    while (current) {
        wyn_conditional_block_evaluate(current, target, flags);
        if (current->is_active) {
            total_length += strlen(current->code_block) + 1; // +1 for newline
        }
        current = current->next;
    }
    
    if (total_length == 0) return NULL;
    
    // Second pass: concatenate active code blocks
    char* result = malloc(total_length + 1);
    if (!result) return NULL;
    
    result[0] = '\0';
    current = blocks;
    
    while (current) {
        if (current->is_active) {
            strcat(result, current->code_block);
            strcat(result, "\n");
        }
        current = current->next;
    }
    
    return result;
}

// Platform API management
WynPlatformAPI* wyn_platform_api_new(WynPlatformAPICategory category, WynTarget* target) {
    if (!target) return NULL;
    
    WynPlatformAPI* api = malloc(sizeof(WynPlatformAPI));
    if (!api) return NULL;
    
    memset(api, 0, sizeof(WynPlatformAPI));
    api->category = category;
    api->target = target;
    api->feature_flags = wyn_feature_flags_new(target);
    api->initialized = false;
    
    return api;
}

void wyn_platform_api_free(WynPlatformAPI* api) {
    if (!api) return;
    
    for (size_t i = 0; i < api->function_count; i++) {
        free(api->functions[i].name);
        free(api->functions[i].signature);
    }
    free(api->functions);
    
    wyn_feature_flags_free(api->feature_flags);
    free(api);
}

bool wyn_platform_api_add_function(WynPlatformAPI* api, const char* name, 
                                   WynPlatformFunction function, const char* signature) {
    if (!api || !name || !function) return false;
    
    api->functions = realloc(api->functions, (api->function_count + 1) * sizeof(WynPlatformAPIFunction));
    if (!api->functions) return false;
    
    WynPlatformAPIFunction* func = &api->functions[api->function_count];
    func->name = strdup(name);
    func->function = function;
    func->signature = signature ? strdup(signature) : NULL;
    func->is_available = true;
    
    api->function_count++;
    return true;
}

WynPlatformAPIFunction* wyn_platform_api_get_function(WynPlatformAPI* api, const char* name) {
    if (!api || !name) return NULL;
    
    for (size_t i = 0; i < api->function_count; i++) {
        if (strcmp(api->functions[i].name, name) == 0) {
            return &api->functions[i];
        }
    }
    
    return NULL;
}

bool wyn_platform_api_is_available(WynPlatformAPI* api, const char* name) {
    WynPlatformAPIFunction* func = wyn_platform_api_get_function(api, name);
    return func && func->is_available;
}

// Unified platform interface
WynPlatformInterface* wyn_platform_interface_new(WynTarget* target) {
    if (!target) return NULL;
    
    WynPlatformInterface* interface = malloc(sizeof(WynPlatformInterface));
    if (!interface) return NULL;
    
    memset(interface, 0, sizeof(WynPlatformInterface));
    interface->target = target;
    interface->feature_flags = wyn_feature_flags_new(target);
    
    return interface;
}

void wyn_platform_interface_free(WynPlatformInterface* interface) {
    if (!interface) return;
    
    for (size_t i = 0; i < interface->api_count; i++) {
        wyn_platform_api_free(interface->apis[i]);
    }
    free(interface->apis);
    
    wyn_feature_flags_free(interface->feature_flags);
    wyn_conditional_block_free(interface->conditional_blocks);
    free(interface);
}

bool wyn_platform_interface_add_api(WynPlatformInterface* interface, WynPlatformAPI* api) {
    if (!interface || !api) return false;
    
    interface->apis = realloc(interface->apis, (interface->api_count + 1) * sizeof(WynPlatformAPI*));
    if (!interface->apis) return false;
    
    interface->apis[interface->api_count] = api;
    interface->api_count++;
    
    return true;
}

WynPlatformAPI* wyn_platform_interface_get_api(WynPlatformInterface* interface, 
                                               WynPlatformAPICategory category) {
    if (!interface) return NULL;
    
    for (size_t i = 0; i < interface->api_count; i++) {
        if (interface->apis[i]->category == category) {
            return interface->apis[i];
        }
    }
    
    return NULL;
}

// Standard platform features
void wyn_platform_init_standard_features(WynFeatureFlags* flags, WynTarget* target) {
    if (!flags || !target) return;
    
    // Add standard features based on target
    switch (target->os) {
        case WYN_OS_WINDOWS:
            wyn_feature_flags_add(flags, "windows", WYN_FEATURE_REQUIRED, "Windows platform support");
            wyn_feature_flags_add(flags, "win32", WYN_FEATURE_OPTIONAL, "Win32 API support");
            wyn_feature_flags_add(flags, "unicode", WYN_FEATURE_REQUIRED, "Unicode support");
            break;
            
        case WYN_OS_LINUX:
            wyn_feature_flags_add(flags, "unix", WYN_FEATURE_REQUIRED, "Unix-like platform support");
            wyn_feature_flags_add(flags, "posix", WYN_FEATURE_REQUIRED, "POSIX API support");
            wyn_feature_flags_add(flags, "threads", WYN_FEATURE_OPTIONAL, "Threading support");
            break;
            
        case WYN_OS_MACOS:
            wyn_feature_flags_add(flags, "unix", WYN_FEATURE_REQUIRED, "Unix-like platform support");
            wyn_feature_flags_add(flags, "posix", WYN_FEATURE_REQUIRED, "POSIX API support");
            wyn_feature_flags_add(flags, "cocoa", WYN_FEATURE_OPTIONAL, "Cocoa framework support");
            break;
            
        case WYN_OS_WASI:
            wyn_feature_flags_add(flags, "wasm", WYN_FEATURE_REQUIRED, "WebAssembly support");
            wyn_feature_flags_add(flags, "wasi", WYN_FEATURE_REQUIRED, "WASI API support");
            break;
            
        default:
            break;
    }
    
    // Add architecture-specific features
    switch (target->arch) {
        case WYN_ARCH_X86_64:
            wyn_feature_flags_add(flags, "x86_64", WYN_FEATURE_REQUIRED, "x86_64 architecture");
            wyn_feature_flags_add(flags, "sse2", WYN_FEATURE_OPTIONAL, "SSE2 instructions");
            break;
            
        case WYN_ARCH_ARM64:
            wyn_feature_flags_add(flags, "aarch64", WYN_FEATURE_REQUIRED, "ARM64 architecture");
            wyn_feature_flags_add(flags, "neon", WYN_FEATURE_OPTIONAL, "NEON SIMD instructions");
            break;
            
        case WYN_ARCH_WASM32:
            wyn_feature_flags_add(flags, "wasm32", WYN_FEATURE_REQUIRED, "WebAssembly 32-bit");
            wyn_feature_flags_add(flags, "simd128", WYN_FEATURE_OPTIONAL, "WASM SIMD support");
            break;
            
        default:
            break;
    }
}

bool wyn_platform_has_filesystem(WynTarget* target) {
    if (!target) return false;
    return target->os != WYN_OS_BROWSER; // Most platforms have filesystem except browser
}

bool wyn_platform_has_network(WynTarget* target) {
    if (!target) return false;
    return target->os != WYN_OS_WASI; // Most platforms have network except basic WASI
}

bool wyn_platform_has_threads(WynTarget* target) {
    if (!target) return false;
    return target->os != WYN_OS_BROWSER && target->os != WYN_OS_WASI;
}

bool wyn_platform_has_gui(WynTarget* target) {
    if (!target) return false;
    return target->os == WYN_OS_WINDOWS || target->os == WYN_OS_MACOS || 
           target->os == WYN_OS_LINUX || target->os == WYN_OS_BROWSER;
}

// Platform-specific constants
WynPlatformConstants* wyn_platform_get_constants(WynTarget* target) {
    if (!target) return NULL;
    
    WynPlatformConstants* constants = malloc(sizeof(WynPlatformConstants));
    if (!constants) return NULL;
    
    memset(constants, 0, sizeof(WynPlatformConstants));
    
    switch (target->os) {
        case WYN_OS_WINDOWS:
            constants->path_separator = strdup("\\");
            constants->line_ending = strdup("\r\n");
            constants->library_prefix = strdup("");
            constants->library_suffix = strdup(".dll");
            constants->executable_suffix = strdup(".exe");
            constants->max_path_length = 260;
            constants->page_size = 4096;
            break;
            
        case WYN_OS_LINUX:
        case WYN_OS_MACOS:
            constants->path_separator = strdup("/");
            constants->line_ending = strdup("\n");
            constants->library_prefix = strdup("lib");
            constants->library_suffix = target->os == WYN_OS_MACOS ? strdup(".dylib") : strdup(".so");
            constants->executable_suffix = strdup("");
            constants->max_path_length = 4096;
            constants->page_size = 4096;
            break;
            
        case WYN_OS_WASI:
            constants->path_separator = strdup("/");
            constants->line_ending = strdup("\n");
            constants->library_prefix = strdup("");
            constants->library_suffix = strdup(".wasm");
            constants->executable_suffix = strdup(".wasm");
            constants->max_path_length = 1024;
            constants->page_size = 65536; // WASM page size
            break;
            
        default:
            constants->path_separator = strdup("/");
            constants->line_ending = strdup("\n");
            constants->library_prefix = strdup("lib");
            constants->library_suffix = strdup(".so");
            constants->executable_suffix = strdup("");
            constants->max_path_length = 4096;
            constants->page_size = 4096;
            break;
    }
    
    return constants;
}

void wyn_platform_constants_free(WynPlatformConstants* constants) {
    if (!constants) return;
    
    free(constants->path_separator);
    free(constants->line_ending);
    free(constants->library_prefix);
    free(constants->library_suffix);
    free(constants->executable_suffix);
    free(constants);
}

// Utility functions
bool wyn_platform_is_unix_like(WynTarget* target) {
    if (!target) return false;
    return target->os == WYN_OS_LINUX || target->os == WYN_OS_MACOS || target->os == WYN_OS_FREEBSD;
}

bool wyn_platform_is_windows_like(WynTarget* target) {
    if (!target) return false;
    return target->os == WYN_OS_WINDOWS;
}

bool wyn_platform_supports_posix(WynTarget* target) {
    return wyn_platform_is_unix_like(target);
}

bool wyn_platform_supports_win32(WynTarget* target) {
    return wyn_platform_is_windows_like(target);
}

const char* wyn_platform_get_arch_string(WynTarget* target) {
    if (!target) return "unknown";
    return wyn_target_arch_name(target->arch);
}

const char* wyn_platform_get_os_string(WynTarget* target) {
    if (!target) return "unknown";
    return wyn_target_os_name(target->os);
}

// Stub implementations for unimplemented features
WynPlatformAPI* wyn_platform_filesystem_api(WynTarget* target) {
    (void)target;
    return NULL; // Stub
}

WynPlatformAPI* wyn_platform_network_api(WynTarget* target) {
    (void)target;
    return NULL; // Stub
}

WynPlatformAPI* wyn_platform_process_api(WynTarget* target) {
    (void)target;
    return NULL; // Stub
}

WynPlatformAPI* wyn_platform_thread_api(WynTarget* target) {
    (void)target;
    return NULL; // Stub
}

WynPlatformAPI* wyn_platform_memory_api(WynTarget* target) {
    (void)target;
    return NULL; // Stub
}

WynPlatformAPI* wyn_platform_time_api(WynTarget* target) {
    (void)target;
    return NULL; // Stub
}

bool wyn_platform_interface_add_conditional_block(WynPlatformInterface* interface, 
                                                  WynConditionalBlock* block) {
    (void)interface; (void)block;
    return false; // Stub
}

char* wyn_platform_generate_cfg_code(WynConditionalBlock* blocks, WynTarget* target, 
                                     WynFeatureFlags* flags) {
    (void)blocks; (void)target; (void)flags;
    return NULL; // Stub
}

bool wyn_platform_preprocess_source(const char* source, WynTarget* target, 
                                    WynFeatureFlags* flags, char** output) {
    (void)source; (void)target; (void)flags; (void)output;
    return false; // Stub
}
