#include "../src/ecosystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_ecosystem_creation() {
    printf("Testing ecosystem creation...\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    assert(ecosystem != NULL);
    assert(ecosystem->ecosystem_version != NULL);
    assert(strcmp(ecosystem->ecosystem_version, "1.0.0") == 0);
    assert(ecosystem->registry != NULL);
    assert(ecosystem->plugin_count == 0);
    assert(ecosystem->tool_count == 0);
    
    wyn_ecosystem_free(ecosystem);
    printf("✓ Ecosystem creation test passed\n");
}

void test_ide_plugin_management() {
    printf("Testing IDE plugin management...\n");
    
    WynIDEPlugin* plugin = wyn_ide_plugin_new(WYN_IDE_VSCODE, "Wyn VSCode", "1.0.0");
    assert(plugin != NULL);
    assert(plugin->ide_type == WYN_IDE_VSCODE);
    assert(strcmp(plugin->name, "Wyn VSCode") == 0);
    assert(strcmp(plugin->version, "1.0.0") == 0);
    assert(plugin->is_official == true);
    assert(plugin->feature_count == 0);
    
    // Add features
    assert(wyn_ide_plugin_add_feature(plugin, WYN_FEATURE_SYNTAX_HIGHLIGHTING) == true);
    assert(wyn_ide_plugin_add_feature(plugin, WYN_FEATURE_CODE_COMPLETION) == true);
    assert(plugin->feature_count == 2);
    assert(plugin->features[0] == WYN_FEATURE_SYNTAX_HIGHLIGHTING);
    assert(plugin->features[1] == WYN_FEATURE_CODE_COMPLETION);
    
    wyn_ide_plugin_free(plugin);
    free(plugin);
    printf("✓ IDE plugin management test passed\n");
}

void test_package_registry() {
    printf("Testing package registry...\n");
    
    WynPackageRegistry* registry = wyn_package_registry_new("https://packages.wyn-lang.org");
    assert(registry != NULL);
    assert(registry->registry_url != NULL);
    assert(strcmp(registry->registry_url, "https://packages.wyn-lang.org") == 0);
    assert(registry->package_count == 0);
    assert(registry->package_capacity == 100);
    
    // Add a package
    WynPackageEntry package = {
        .name = "test-package",
        .version = "1.0.0",
        .description = "A test package",
        .author = "Test Author",
        .category = WYN_PKG_UTILITY,
        .download_count = 100,
        .rating = 4.5,
        .repository_url = "https://github.com/test/package",
        .documentation_url = "https://docs.test.com"
    };
    
    assert(wyn_package_registry_add_package(registry, &package) == true);
    assert(registry->package_count == 1);
    
    // Search for package
    size_t result_count;
    WynPackageEntry* results = wyn_package_registry_search(registry, "test", &result_count);
    assert(results != NULL);
    assert(result_count == 1);
    assert(strcmp(results[0].name, "test-package") == 0);
    
    free(results);
    wyn_package_registry_free(registry);
    printf("✓ Package registry test passed\n");
}

void test_community_tools() {
    printf("Testing community tools...\n");
    
    WynCommunityTool* tool = wyn_community_tool_new(WYN_TOOL_FORMATTER, "wyn-fmt", "Code formatter for Wyn");
    assert(tool != NULL);
    assert(tool->type == WYN_TOOL_FORMATTER);
    assert(strcmp(tool->name, "wyn-fmt") == 0);
    assert(strcmp(tool->description, "Code formatter for Wyn") == 0);
    assert(tool->is_official == false);
    
    wyn_community_tool_free(tool);
    free(tool);
    printf("✓ Community tools test passed\n");
}

void test_ecosystem_initialization() {
    printf("Testing ecosystem initialization...\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    assert(ecosystem != NULL);
    
    // Initialize ecosystem
    assert(wyn_ecosystem_initialize(ecosystem) == true);
    
    // Check that plugins were added
    assert(ecosystem->plugin_count > 0);
    
    // Check that packages were added to registry
    assert(ecosystem->registry->package_count > 0);
    
    // Verify some plugins exist
    bool found_vscode = false, found_intellij = false, found_vim = false;
    for (size_t i = 0; i < ecosystem->plugin_count; i++) {
        WynIDEPlugin* plugin = &ecosystem->ide_plugins[i];
        if (plugin->ide_type == WYN_IDE_VSCODE) found_vscode = true;
        if (plugin->ide_type == WYN_IDE_INTELLIJ) found_intellij = true;
        if (plugin->ide_type == WYN_IDE_VIM) found_vim = true;
    }
    
    assert(found_vscode == true);
    assert(found_intellij == true);
    assert(found_vim == true);
    
    wyn_ecosystem_free(ecosystem);
    printf("✓ Ecosystem initialization test passed\n");
}

void test_standard_library_expansion() {
    printf("Testing standard library expansion...\n");
    
    // Test web module creation
    WynStdLibModule* web_module = wyn_create_web_module();
    assert(web_module != NULL);
    assert(strcmp(web_module->module_name, "web") == 0);
    assert(web_module->function_count == 5);
    assert(strcmp(web_module->functions[0], "http_server_new") == 0);
    
    // Cleanup
    for (size_t i = 0; i < web_module->function_count; i++) {
        free(web_module->functions[i]);
    }
    free(web_module->functions);
    free(web_module->module_name);
    free(web_module->description);
    free(web_module->documentation_url);
    free(web_module);
    
    // Test crypto module creation
    WynStdLibModule* crypto_module = wyn_create_crypto_module();
    assert(crypto_module != NULL);
    assert(strcmp(crypto_module->module_name, "crypto") == 0);
    assert(crypto_module->function_count == 4);
    assert(strcmp(crypto_module->functions[0], "hash_sha256") == 0);
    
    // Cleanup
    for (size_t i = 0; i < crypto_module->function_count; i++) {
        free(crypto_module->functions[i]);
    }
    free(crypto_module->functions);
    free(crypto_module->module_name);
    free(crypto_module->description);
    free(crypto_module->documentation_url);
    free(crypto_module);
    
    printf("✓ Standard library expansion test passed\n");
}

void test_ecosystem_metrics() {
    printf("Testing ecosystem metrics...\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    wyn_ecosystem_initialize(ecosystem);
    
    WynEcosystemMetrics* metrics = wyn_collect_ecosystem_metrics(ecosystem);
    assert(metrics != NULL);
    assert(metrics->total_packages > 0);
    assert(metrics->active_developers > 0);
    assert(metrics->monthly_downloads > 0);
    assert(metrics->ecosystem_health_score >= 0.0);
    assert(metrics->ecosystem_health_score <= 100.0);
    
    printf("  Total packages: %zu\n", metrics->total_packages);
    printf("  Active developers: %zu\n", metrics->active_developers);
    printf("  Monthly downloads: %zu\n", metrics->monthly_downloads);
    printf("  Health score: %.1f\n", metrics->ecosystem_health_score);
    
    free(metrics);
    wyn_ecosystem_free(ecosystem);
    printf("✓ Ecosystem metrics test passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test IDE type names
    assert(strcmp(wyn_ide_type_name(WYN_IDE_VSCODE), "Visual Studio Code") == 0);
    assert(strcmp(wyn_ide_type_name(WYN_IDE_INTELLIJ), "IntelliJ IDEA") == 0);
    assert(strcmp(wyn_ide_type_name(WYN_IDE_VIM), "Vim") == 0);
    
    // Test plugin feature names
    assert(strcmp(wyn_plugin_feature_name(WYN_FEATURE_SYNTAX_HIGHLIGHTING), "Syntax Highlighting") == 0);
    assert(strcmp(wyn_plugin_feature_name(WYN_FEATURE_CODE_COMPLETION), "Code Completion") == 0);
    assert(strcmp(wyn_plugin_feature_name(WYN_FEATURE_ERROR_CHECKING), "Error Checking") == 0);
    
    // Test package category names
    assert(strcmp(wyn_package_category_name(WYN_PKG_WEB), "Web") == 0);
    assert(strcmp(wyn_package_category_name(WYN_PKG_CRYPTO), "Cryptography") == 0);
    assert(strcmp(wyn_package_category_name(WYN_PKG_UTILITY), "Utility") == 0);
    
    // Test tool type names
    assert(strcmp(wyn_tool_type_name(WYN_TOOL_FORMATTER), "Formatter") == 0);
    assert(strcmp(wyn_tool_type_name(WYN_TOOL_LINTER), "Linter") == 0);
    assert(strcmp(wyn_tool_type_name(WYN_TOOL_DEBUGGER), "Debugger") == 0);
    
    printf("✓ Utility functions test passed\n");
}

void test_package_bootstrap() {
    printf("Testing package bootstrap...\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    assert(ecosystem != NULL);
    
    // Bootstrap package ecosystem
    assert(wyn_bootstrap_package_ecosystem(ecosystem) == true);
    assert(ecosystem->registry->package_count >= 3);
    
    // Check for core packages
    bool found_json = false, found_http = false, found_crypto = false;
    for (size_t i = 0; i < ecosystem->registry->package_count; i++) {
        WynPackageEntry* pkg = &ecosystem->registry->packages[i];
        if (strcmp(pkg->name, "json") == 0) found_json = true;
        if (strcmp(pkg->name, "http") == 0) found_http = true;
        if (strcmp(pkg->name, "crypto") == 0) found_crypto = true;
    }
    
    assert(found_json == true);
    assert(found_http == true);
    assert(found_crypto == true);
    
    wyn_ecosystem_free(ecosystem);
    printf("✓ Package bootstrap test passed\n");
}

void test_plugin_features() {
    printf("Testing plugin features...\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    wyn_ecosystem_initialize(ecosystem);
    
    // Check VSCode plugin features
    WynIDEPlugin* vscode_plugin = NULL;
    for (size_t i = 0; i < ecosystem->plugin_count; i++) {
        if (ecosystem->ide_plugins[i].ide_type == WYN_IDE_VSCODE) {
            vscode_plugin = &ecosystem->ide_plugins[i];
            break;
        }
    }
    
    assert(vscode_plugin != NULL);
    assert(vscode_plugin->feature_count > 0);
    
    // Check that it has syntax highlighting
    bool has_syntax_highlighting = false;
    for (size_t i = 0; i < vscode_plugin->feature_count; i++) {
        if (vscode_plugin->features[i] == WYN_FEATURE_SYNTAX_HIGHLIGHTING) {
            has_syntax_highlighting = true;
            break;
        }
    }
    assert(has_syntax_highlighting == true);
    
    printf("  VSCode plugin has %zu features\n", vscode_plugin->feature_count);
    
    wyn_ecosystem_free(ecosystem);
    printf("✓ Plugin features test passed\n");
}

int main() {
    printf("Running Ecosystem Maturity and IDE Integration Tests\n");
    printf("====================================================\n");
    
    test_ecosystem_creation();
    test_ide_plugin_management();
    test_package_registry();
    test_community_tools();
    test_ecosystem_initialization();
    test_standard_library_expansion();
    test_ecosystem_metrics();
    test_utility_functions();
    test_package_bootstrap();
    test_plugin_features();
    
    printf("\n✓ All ecosystem maturity and IDE integration tests passed!\n");
    return 0;
}
