#ifndef WYN_ECOSYSTEM_H
#define WYN_ECOSYSTEM_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct WynEcosystem WynEcosystem;
typedef struct WynIDEPlugin WynIDEPlugin;
typedef struct WynPackageRegistry WynPackageRegistry;
typedef struct WynCommunityTool WynCommunityTool;

// IDE types
typedef enum {
    WYN_IDE_VSCODE,
    WYN_IDE_INTELLIJ,
    WYN_IDE_VIM,
    WYN_IDE_EMACS,
    WYN_IDE_SUBLIME,
    WYN_IDE_ATOM,
    WYN_IDE_GENERIC_LSP
} WynIDEType;

// Plugin features
typedef enum {
    WYN_FEATURE_SYNTAX_HIGHLIGHTING,
    WYN_FEATURE_CODE_COMPLETION,
    WYN_FEATURE_ERROR_CHECKING,
    WYN_FEATURE_GO_TO_DEFINITION,
    WYN_FEATURE_FIND_REFERENCES,
    WYN_FEATURE_REFACTORING,
    WYN_FEATURE_DEBUGGING,
    WYN_FEATURE_FORMATTING,
    WYN_FEATURE_SNIPPETS,
    WYN_FEATURE_LIVE_TEMPLATES
} WynPluginFeature;

// Community tool types
typedef enum {
    WYN_TOOL_FORMATTER,
    WYN_TOOL_LINTER,
    WYN_TOOL_DOCUMENTATION_GENERATOR,
    WYN_TOOL_TEST_RUNNER,
    WYN_TOOL_PACKAGE_MANAGER,
    WYN_TOOL_BUILD_SYSTEM,
    WYN_TOOL_DEPLOYMENT,
    WYN_TOOL_MONITORING,
    WYN_TOOL_PROFILER,
    WYN_TOOL_DEBUGGER
} WynToolType;

// Package category
typedef enum {
    WYN_PKG_CORE,
    WYN_PKG_WEB,
    WYN_PKG_NETWORKING,
    WYN_PKG_DATABASE,
    WYN_PKG_GRAPHICS,
    WYN_PKG_AUDIO,
    WYN_PKG_CRYPTO,
    WYN_PKG_MATH,
    WYN_PKG_TESTING,
    WYN_PKG_UTILITY
} WynPackageCategory;

// IDE plugin structure
typedef struct WynIDEPlugin {
    WynIDEType ide_type;
    char* name;
    char* version;
    WynPluginFeature* features;
    size_t feature_count;
    char* installation_url;
    char* documentation_url;
    bool is_official;
} WynIDEPlugin;

// Package registry entry
typedef struct {
    char* name;
    char* version;
    char* description;
    char* author;
    WynPackageCategory category;
    char** dependencies;
    size_t dependency_count;
    size_t download_count;
    double rating;
    char* repository_url;
    char* documentation_url;
} WynPackageEntry;

// Package registry
typedef struct WynPackageRegistry {
    WynPackageEntry* packages;
    size_t package_count;
    size_t package_capacity;
    char* registry_url;
    char* api_endpoint;
} WynPackageRegistry;

// Community tool
typedef struct WynCommunityTool {
    WynToolType type;
    char* name;
    char* description;
    char* author;
    char* version;
    char* installation_command;
    char* usage_example;
    char* repository_url;
    bool is_official;
} WynCommunityTool;

// Ecosystem manager
typedef struct WynEcosystem {
    WynIDEPlugin* ide_plugins;
    size_t plugin_count;
    WynPackageRegistry* registry;
    WynCommunityTool* tools;
    size_t tool_count;
    char* ecosystem_version;
} WynEcosystem;

// Ecosystem management functions
WynEcosystem* wyn_ecosystem_new(void);
void wyn_ecosystem_free(WynEcosystem* ecosystem);
bool wyn_ecosystem_initialize(WynEcosystem* ecosystem);
bool wyn_ecosystem_update(WynEcosystem* ecosystem);

// IDE plugin management
WynIDEPlugin* wyn_ide_plugin_new(WynIDEType ide_type, const char* name, const char* version);
void wyn_ide_plugin_free(WynIDEPlugin* plugin);
bool wyn_ide_plugin_add_feature(WynIDEPlugin* plugin, WynPluginFeature feature);
bool wyn_ide_plugin_install(WynIDEPlugin* plugin);
bool wyn_ide_plugin_update(WynIDEPlugin* plugin);

// Package registry functions
WynPackageRegistry* wyn_package_registry_new(const char* registry_url);
void wyn_package_registry_free(WynPackageRegistry* registry);
bool wyn_package_registry_add_package(WynPackageRegistry* registry, const WynPackageEntry* package);
WynPackageEntry* wyn_package_registry_search(WynPackageRegistry* registry, const char* query, size_t* result_count);
WynPackageEntry* wyn_package_registry_get_package(WynPackageRegistry* registry, const char* name);
bool wyn_package_registry_sync(WynPackageRegistry* registry);

// Community tool management
WynCommunityTool* wyn_community_tool_new(WynToolType type, const char* name, const char* description);
void wyn_community_tool_free(WynCommunityTool* tool);
bool wyn_community_tool_install(WynCommunityTool* tool);
bool wyn_community_tool_run(WynCommunityTool* tool, const char* args);

// Built-in IDE plugin generators
bool wyn_generate_vscode_plugin(WynEcosystem* ecosystem);
bool wyn_generate_intellij_plugin(WynEcosystem* ecosystem);
bool wyn_generate_vim_plugin(WynEcosystem* ecosystem);
bool wyn_generate_emacs_plugin(WynEcosystem* ecosystem);

// Standard library expansion
typedef struct {
    char* module_name;
    char* description;
    char** functions;
    size_t function_count;
    char* documentation_url;
} WynStdLibModule;

bool wyn_expand_standard_library(WynEcosystem* ecosystem);
bool wyn_add_stdlib_module(WynEcosystem* ecosystem, const WynStdLibModule* module);
WynStdLibModule* wyn_create_web_module(void);
WynStdLibModule* wyn_create_crypto_module(void);
WynStdLibModule* wyn_create_graphics_module(void);
WynStdLibModule* wyn_create_audio_module(void);

// Package ecosystem development
bool wyn_bootstrap_package_ecosystem(WynEcosystem* ecosystem);
bool wyn_create_package_template(const char* package_name, WynPackageCategory category);
bool wyn_publish_package(const char* package_path, WynPackageRegistry* registry);
bool wyn_validate_package(const char* package_path);

// Community integration
typedef struct {
    char* platform_name;
    char* api_endpoint;
    char* webhook_url;
    bool notifications_enabled;
} WynCommunityPlatform;

bool wyn_integrate_github(WynEcosystem* ecosystem, const char* organization);
bool wyn_integrate_discord(WynEcosystem* ecosystem, const char* server_id);
bool wyn_integrate_reddit(WynEcosystem* ecosystem, const char* subreddit);
bool wyn_setup_community_forums(WynEcosystem* ecosystem);

// Developer experience tools
typedef struct {
    char* tool_name;
    char* command;
    char* description;
    bool is_interactive;
} WynDevTool;

WynDevTool* wyn_create_project_generator(void);
WynDevTool* wyn_create_dependency_manager(void);
WynDevTool* wyn_create_test_framework(void);
WynDevTool* wyn_create_documentation_generator(void);

// Ecosystem metrics and analytics
typedef struct {
    size_t total_packages;
    size_t active_developers;
    size_t monthly_downloads;
    size_t github_stars;
    size_t community_members;
    double ecosystem_health_score;
} WynEcosystemMetrics;

WynEcosystemMetrics* wyn_collect_ecosystem_metrics(WynEcosystem* ecosystem);
bool wyn_generate_ecosystem_report(WynEcosystem* ecosystem, const char* output_file);
double wyn_calculate_ecosystem_health(WynEcosystem* ecosystem);

// Plugin development framework
typedef struct {
    char* plugin_name;
    char* target_ide;
    char* manifest_content;
    char* source_code;
    char* build_script;
} WynPluginTemplate;

WynPluginTemplate* wyn_create_plugin_template(WynIDEType ide_type, const char* plugin_name);
bool wyn_build_plugin(WynPluginTemplate* template, const char* output_dir);
bool wyn_test_plugin(WynPluginTemplate* template);
void wyn_plugin_template_free(WynPluginTemplate* template);

// Language server integration
typedef struct {
    char* server_name;
    char* executable_path;
    char** supported_features;
    size_t feature_count;
    int port;
    bool is_running;
} WynLanguageServer;

WynLanguageServer* wyn_language_server_new(void);
void wyn_language_server_free(WynLanguageServer* server);
bool wyn_language_server_start(WynLanguageServer* server);
bool wyn_language_server_stop(WynLanguageServer* server);
bool wyn_language_server_register_capability(WynLanguageServer* server, const char* capability);

// Ecosystem validation and quality assurance
bool wyn_validate_ecosystem_health(WynEcosystem* ecosystem);
bool wyn_check_plugin_compatibility(WynIDEPlugin* plugin);
bool wyn_verify_package_integrity(WynPackageEntry* package);
bool wyn_audit_security_vulnerabilities(WynEcosystem* ecosystem);

// Utility functions
const char* wyn_ide_type_name(WynIDEType type);
const char* wyn_plugin_feature_name(WynPluginFeature feature);
const char* wyn_tool_type_name(WynToolType type);
const char* wyn_package_category_name(WynPackageCategory category);

// Configuration and settings
typedef struct {
    char* default_registry_url;
    char* plugin_directory;
    char* cache_directory;
    bool auto_update_enabled;
    bool telemetry_enabled;
} WynEcosystemConfig;

WynEcosystemConfig* wyn_ecosystem_config_load(const char* config_file);
bool wyn_ecosystem_config_save(WynEcosystemConfig* config, const char* config_file);
void wyn_ecosystem_config_free(WynEcosystemConfig* config);

#endif // WYN_ECOSYSTEM_H
