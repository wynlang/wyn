#ifndef WYN_PLATFORM_H
#define WYN_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "crossplatform.h"

// Forward declarations
typedef struct WynPlatformAPI WynPlatformAPI;
typedef struct WynFeatureFlags WynFeatureFlags;
typedef struct WynConditionalBlock WynConditionalBlock;

// Platform-specific API categories
typedef enum {
    WYN_PLATFORM_FILESYSTEM,
    WYN_PLATFORM_NETWORK,
    WYN_PLATFORM_PROCESS,
    WYN_PLATFORM_THREAD,
    WYN_PLATFORM_MEMORY,
    WYN_PLATFORM_TIME,
    WYN_PLATFORM_CRYPTO,
    WYN_PLATFORM_GUI
} WynPlatformAPICategory;

// Feature flag types
typedef enum {
    WYN_FEATURE_OPTIONAL,
    WYN_FEATURE_REQUIRED,
    WYN_FEATURE_DEPRECATED,
    WYN_FEATURE_EXPERIMENTAL
} WynFeatureType;

// Conditional compilation operators
typedef enum {
    WYN_CFG_EQUALS,
    WYN_CFG_NOT_EQUALS,
    WYN_CFG_AND,
    WYN_CFG_OR,
    WYN_CFG_NOT,
    WYN_CFG_ANY,
    WYN_CFG_ALL
} WynCfgOperator;

// Feature flag definition
typedef struct {
    char* name;
    WynFeatureType type;
    bool enabled;
    char* description;
    char** dependencies;
    size_t dependency_count;
    char* since_version;
} WynFeatureFlag;

// Feature flags collection
typedef struct WynFeatureFlags {
    WynFeatureFlag* flags;
    size_t flag_count;
    size_t flag_capacity;
    WynTarget* target;
} WynFeatureFlags;

// Conditional compilation expression
typedef struct WynCfgExpression {
    WynCfgOperator op;
    char* key;
    char* value;
    struct WynCfgExpression* left;
    struct WynCfgExpression* right;
} WynCfgExpression;

// Conditional compilation block
typedef struct WynConditionalBlock {
    WynCfgExpression* condition;
    char* code_block;
    bool is_active;
    struct WynConditionalBlock* next;
} WynConditionalBlock;

// Platform-specific function pointer
typedef void* (*WynPlatformFunction)(void* args);

// Platform API function
typedef struct {
    char* name;
    WynPlatformFunction function;
    char* signature;
    bool is_available;
} WynPlatformAPIFunction;

// Platform API
typedef struct WynPlatformAPI {
    WynPlatformAPICategory category;
    WynTarget* target;
    WynPlatformAPIFunction* functions;
    size_t function_count;
    WynFeatureFlags* feature_flags;
    bool initialized;
} WynPlatformAPI;

// Feature flags management
WynFeatureFlags* wyn_feature_flags_new(WynTarget* target);
void wyn_feature_flags_free(WynFeatureFlags* flags);
bool wyn_feature_flags_add(WynFeatureFlags* flags, const char* name, WynFeatureType type, 
                          const char* description);
bool wyn_feature_flags_enable(WynFeatureFlags* flags, const char* name);
bool wyn_feature_flags_disable(WynFeatureFlags* flags, const char* name);
bool wyn_feature_flags_is_enabled(WynFeatureFlags* flags, const char* name);
WynFeatureFlag* wyn_feature_flags_get(WynFeatureFlags* flags, const char* name);
char** wyn_feature_flags_list_enabled(WynFeatureFlags* flags, size_t* count);

// Conditional compilation
WynCfgExpression* wyn_cfg_expression_new(WynCfgOperator op, const char* key, const char* value);
void wyn_cfg_expression_free(WynCfgExpression* expr);
WynCfgExpression* wyn_cfg_parse(const char* cfg_string);
bool wyn_cfg_evaluate(WynCfgExpression* expr, WynTarget* target, WynFeatureFlags* flags);
char* wyn_cfg_to_string(WynCfgExpression* expr);

// Conditional blocks
WynConditionalBlock* wyn_conditional_block_new(WynCfgExpression* condition, const char* code);
void wyn_conditional_block_free(WynConditionalBlock* block);
bool wyn_conditional_block_evaluate(WynConditionalBlock* block, WynTarget* target, 
                                   WynFeatureFlags* flags);
char* wyn_conditional_block_get_active_code(WynConditionalBlock* blocks, WynTarget* target, 
                                           WynFeatureFlags* flags);

// Platform API management
WynPlatformAPI* wyn_platform_api_new(WynPlatformAPICategory category, WynTarget* target);
void wyn_platform_api_free(WynPlatformAPI* api);
bool wyn_platform_api_add_function(WynPlatformAPI* api, const char* name, 
                                   WynPlatformFunction function, const char* signature);
WynPlatformAPIFunction* wyn_platform_api_get_function(WynPlatformAPI* api, const char* name);
bool wyn_platform_api_is_available(WynPlatformAPI* api, const char* name);

// Platform-specific implementations
WynPlatformAPI* wyn_platform_filesystem_api(WynTarget* target);
WynPlatformAPI* wyn_platform_network_api(WynTarget* target);
WynPlatformAPI* wyn_platform_process_api(WynTarget* target);
WynPlatformAPI* wyn_platform_thread_api(WynTarget* target);
WynPlatformAPI* wyn_platform_memory_api(WynTarget* target);
WynPlatformAPI* wyn_platform_time_api(WynTarget* target);

// Unified platform interface
typedef struct {
    WynTarget* target;
    WynFeatureFlags* feature_flags;
    WynPlatformAPI** apis;
    size_t api_count;
    WynConditionalBlock* conditional_blocks;
} WynPlatformInterface;

WynPlatformInterface* wyn_platform_interface_new(WynTarget* target);
void wyn_platform_interface_free(WynPlatformInterface* interface);
bool wyn_platform_interface_add_api(WynPlatformInterface* interface, WynPlatformAPI* api);
WynPlatformAPI* wyn_platform_interface_get_api(WynPlatformInterface* interface, 
                                               WynPlatformAPICategory category);
bool wyn_platform_interface_add_conditional_block(WynPlatformInterface* interface, 
                                                  WynConditionalBlock* block);

// Standard platform features
void wyn_platform_init_standard_features(WynFeatureFlags* flags, WynTarget* target);
bool wyn_platform_has_filesystem(WynTarget* target);
bool wyn_platform_has_network(WynTarget* target);
bool wyn_platform_has_threads(WynTarget* target);
bool wyn_platform_has_gui(WynTarget* target);

// Platform-specific constants
typedef struct {
    char* path_separator;
    char* line_ending;
    char* library_prefix;
    char* library_suffix;
    char* executable_suffix;
    size_t max_path_length;
    size_t page_size;
} WynPlatformConstants;

WynPlatformConstants* wyn_platform_get_constants(WynTarget* target);
void wyn_platform_constants_free(WynPlatformConstants* constants);

// Utility functions
bool wyn_platform_is_unix_like(WynTarget* target);
bool wyn_platform_is_windows_like(WynTarget* target);
bool wyn_platform_supports_posix(WynTarget* target);
bool wyn_platform_supports_win32(WynTarget* target);
const char* wyn_platform_get_arch_string(WynTarget* target);
const char* wyn_platform_get_os_string(WynTarget* target);

// Code generation helpers
char* wyn_platform_generate_cfg_code(WynConditionalBlock* blocks, WynTarget* target, 
                                     WynFeatureFlags* flags);
bool wyn_platform_preprocess_source(const char* source, WynTarget* target, 
                                    WynFeatureFlags* flags, char** output);

#endif // WYN_PLATFORM_H
