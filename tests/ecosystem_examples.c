#include "../src/ecosystem.h"
#include <stdio.h>
#include <stdlib.h>

// Example: Setting up the Wyn ecosystem
void example_ecosystem_setup() {
    printf("=== Ecosystem Setup Example ===\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    
    printf("Initializing Wyn ecosystem version %s...\n", ecosystem->ecosystem_version);
    
    // Initialize the ecosystem with built-in components
    if (wyn_ecosystem_initialize(ecosystem)) {
        printf("Ecosystem initialized successfully!\n");
        
        printf("\nEcosystem components:\n");
        printf("  IDE Plugins: %zu\n", ecosystem->plugin_count);
        printf("  Packages: %zu\n", ecosystem->registry->package_count);
        printf("  Community Tools: %zu\n", ecosystem->tool_count);
        
        // Show IDE plugins
        printf("\nAvailable IDE plugins:\n");
        for (size_t i = 0; i < ecosystem->plugin_count; i++) {
            WynIDEPlugin* plugin = &ecosystem->ide_plugins[i];
            printf("  %s (%s) - %zu features\n", 
                   plugin->name, 
                   wyn_ide_type_name(plugin->ide_type),
                   plugin->feature_count);
        }
    }
    
    wyn_ecosystem_free(ecosystem);
}

// Example: IDE plugin development
void example_ide_plugin_development() {
    printf("\n=== IDE Plugin Development Example ===\n");
    
    // Create a custom IDE plugin
    WynIDEPlugin* plugin = wyn_ide_plugin_new(WYN_IDE_VSCODE, "Wyn Advanced", "2.0.0");
    
    printf("Creating IDE plugin: %s for %s\n", 
           plugin->name, wyn_ide_type_name(plugin->ide_type));
    
    // Add comprehensive features
    WynPluginFeature features[] = {
        WYN_FEATURE_SYNTAX_HIGHLIGHTING,
        WYN_FEATURE_CODE_COMPLETION,
        WYN_FEATURE_ERROR_CHECKING,
        WYN_FEATURE_GO_TO_DEFINITION,
        WYN_FEATURE_FIND_REFERENCES,
        WYN_FEATURE_REFACTORING,
        WYN_FEATURE_DEBUGGING,
        WYN_FEATURE_FORMATTING,
        WYN_FEATURE_SNIPPETS
    };
    
    printf("\nAdding features:\n");
    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        wyn_ide_plugin_add_feature(plugin, features[i]);
        printf("  + %s\n", wyn_plugin_feature_name(features[i]));
    }
    
    printf("\nPlugin configuration:\n");
    printf("  Name: %s\n", plugin->name);
    printf("  Version: %s\n", plugin->version);
    printf("  IDE: %s\n", wyn_ide_type_name(plugin->ide_type));
    printf("  Features: %zu\n", plugin->feature_count);
    printf("  Official: %s\n", plugin->is_official ? "Yes" : "No");
    
    wyn_ide_plugin_free(plugin);
    free(plugin);
}

// Example: Package registry management
void example_package_registry() {
    printf("\n=== Package Registry Example ===\n");
    
    WynPackageRegistry* registry = wyn_package_registry_new("https://packages.wyn-lang.org");
    
    printf("Package registry: %s\n", registry->registry_url);
    
    // Add some example packages
    WynPackageEntry packages[] = {
        {
            .name = "web-framework",
            .version = "3.1.0",
            .description = "Modern web framework for Wyn with async support",
            .author = "WebDev Team",
            .category = WYN_PKG_WEB,
            .download_count = 15000,
            .rating = 4.8,
            .repository_url = "https://github.com/wyn-web/framework",
            .documentation_url = "https://docs.wyn-web.org"
        },
        {
            .name = "database-orm",
            .version = "2.5.2",
            .description = "Object-relational mapping library with migration support",
            .author = "Database Team",
            .category = WYN_PKG_DATABASE,
            .download_count = 8500,
            .rating = 4.6,
            .repository_url = "https://github.com/wyn-db/orm",
            .documentation_url = "https://docs.wyn-db.org"
        },
        {
            .name = "graphics-engine",
            .version = "1.0.0",
            .description = "2D/3D graphics engine with Vulkan backend",
            .author = "Graphics Team",
            .category = WYN_PKG_GRAPHICS,
            .download_count = 3200,
            .rating = 4.9,
            .repository_url = "https://github.com/wyn-graphics/engine",
            .documentation_url = "https://docs.wyn-graphics.org"
        }
    };
    
    printf("\nAdding packages to registry:\n");
    for (size_t i = 0; i < 3; i++) {
        wyn_package_registry_add_package(registry, &packages[i]);
        printf("  + %s v%s (%s) - %zu downloads, %.1f★\n",
               packages[i].name,
               packages[i].version,
               wyn_package_category_name(packages[i].category),
               packages[i].download_count,
               packages[i].rating);
    }
    
    printf("\nRegistry statistics:\n");
    printf("  Total packages: %zu\n", registry->package_count);
    
    // Search for packages
    printf("\nSearching for 'web' packages:\n");
    size_t result_count;
    WynPackageEntry* results = wyn_package_registry_search(registry, "web", &result_count);
    if (results) {
        for (size_t i = 0; i < result_count; i++) {
            printf("  Found: %s - %s\n", results[i].name, results[i].description);
        }
        free(results);
    }
    
    wyn_package_registry_free(registry);
}

// Example: Community tools ecosystem
void example_community_tools() {
    printf("\n=== Community Tools Example ===\n");
    
    // Create various community tools
    WynCommunityTool* tools[] = {
        wyn_community_tool_new(WYN_TOOL_FORMATTER, "wyn-fmt", "Official Wyn code formatter"),
        wyn_community_tool_new(WYN_TOOL_LINTER, "wyn-lint", "Static analysis and linting tool"),
        wyn_community_tool_new(WYN_TOOL_TEST_RUNNER, "wyn-test", "Advanced testing framework"),
        wyn_community_tool_new(WYN_TOOL_DOCUMENTATION_GENERATOR, "wyn-doc", "API documentation generator"),
        wyn_community_tool_new(WYN_TOOL_PROFILER, "wyn-prof", "Performance profiling tool")
    };
    
    printf("Community tools ecosystem:\n");
    
    for (size_t i = 0; i < 5; i++) {
        WynCommunityTool* tool = tools[i];
        printf("  %s (%s)\n", tool->name, wyn_tool_type_name(tool->type));
        printf("    Description: %s\n", tool->description);
        printf("    Version: %s\n", tool->version);
        printf("    Official: %s\n", tool->is_official ? "Yes" : "No");
        printf("\n");
        
        wyn_community_tool_free(tool);
        free(tool);
    }
}

// Example: Standard library expansion
void example_stdlib_expansion() {
    printf("=== Standard Library Expansion Example ===\n");
    
    // Create web module
    WynStdLibModule* web_module = wyn_create_web_module();
    printf("Web module: %s\n", web_module->module_name);
    printf("  Description: %s\n", web_module->description);
    printf("  Functions (%zu):\n", web_module->function_count);
    for (size_t i = 0; i < web_module->function_count; i++) {
        printf("    - %s\n", web_module->functions[i]);
    }
    printf("  Documentation: %s\n", web_module->documentation_url);
    
    // Cleanup web module
    for (size_t i = 0; i < web_module->function_count; i++) {
        free(web_module->functions[i]);
    }
    free(web_module->functions);
    free(web_module->module_name);
    free(web_module->description);
    free(web_module->documentation_url);
    free(web_module);
    
    printf("\n");
    
    // Create crypto module
    WynStdLibModule* crypto_module = wyn_create_crypto_module();
    printf("Crypto module: %s\n", crypto_module->module_name);
    printf("  Description: %s\n", crypto_module->description);
    printf("  Functions (%zu):\n", crypto_module->function_count);
    for (size_t i = 0; i < crypto_module->function_count; i++) {
        printf("    - %s\n", crypto_module->functions[i]);
    }
    printf("  Documentation: %s\n", crypto_module->documentation_url);
    
    // Cleanup crypto module
    for (size_t i = 0; i < crypto_module->function_count; i++) {
        free(crypto_module->functions[i]);
    }
    free(crypto_module->functions);
    free(crypto_module->module_name);
    free(crypto_module->description);
    free(crypto_module->documentation_url);
    free(crypto_module);
}

// Example: Ecosystem metrics and health
void example_ecosystem_metrics() {
    printf("\n=== Ecosystem Metrics Example ===\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    wyn_ecosystem_initialize(ecosystem);
    
    // Collect ecosystem metrics
    WynEcosystemMetrics* metrics = wyn_collect_ecosystem_metrics(ecosystem);
    
    printf("Wyn Ecosystem Health Report\n");
    printf("===========================\n");
    printf("Total Packages: %zu\n", metrics->total_packages);
    printf("Active Developers: %zu\n", metrics->active_developers);
    printf("Monthly Downloads: %zu\n", metrics->monthly_downloads);
    printf("GitHub Stars: %zu\n", metrics->github_stars);
    printf("Community Members: %zu\n", metrics->community_members);
    printf("Health Score: %.1f/100\n", metrics->ecosystem_health_score);
    
    // Interpret health score
    if (metrics->ecosystem_health_score >= 80.0) {
        printf("Status: Excellent ecosystem health! 🎉\n");
    } else if (metrics->ecosystem_health_score >= 60.0) {
        printf("Status: Good ecosystem health ✅\n");
    } else if (metrics->ecosystem_health_score >= 40.0) {
        printf("Status: Moderate ecosystem health ⚠️\n");
    } else {
        printf("Status: Needs improvement 🔧\n");
    }
    
    printf("\nGrowth indicators:\n");
    printf("  Package diversity: %zu categories covered\n", 
           (metrics->total_packages > 0) ? 3 : 0); // Simplified
    printf("  Developer engagement: %s\n", 
           (metrics->active_developers > 100) ? "High" : "Growing");
    printf("  Community activity: %s\n", 
           (metrics->monthly_downloads > 20000) ? "Very Active" : "Active");
    
    free(metrics);
    wyn_ecosystem_free(ecosystem);
}

// Example: IDE integration showcase
void example_ide_integration() {
    printf("\n=== IDE Integration Showcase Example ===\n");
    
    WynEcosystem* ecosystem = wyn_ecosystem_new();
    wyn_ecosystem_initialize(ecosystem);
    
    printf("Wyn Language IDE Support\n");
    printf("========================\n");
    
    for (size_t i = 0; i < ecosystem->plugin_count; i++) {
        WynIDEPlugin* plugin = &ecosystem->ide_plugins[i];
        
        printf("\n%s Plugin\n", wyn_ide_type_name(plugin->ide_type));
        printf("Name: %s v%s\n", plugin->name, plugin->version);
        printf("Official: %s\n", plugin->is_official ? "✓" : "✗");
        
        if (plugin->installation_url) {
            printf("Install: %s\n", plugin->installation_url);
        }
        
        printf("Features:\n");
        for (size_t j = 0; j < plugin->feature_count; j++) {
            printf("  ✓ %s\n", wyn_plugin_feature_name(plugin->features[j]));
        }
        
        if (plugin->documentation_url) {
            printf("Docs: %s\n", plugin->documentation_url);
        }
    }
    
    printf("\nDeveloper Experience:\n");
    printf("• Rich syntax highlighting with semantic tokens\n");
    printf("• Intelligent code completion with type information\n");
    printf("• Real-time error checking and diagnostics\n");
    printf("• Advanced refactoring capabilities\n");
    printf("• Integrated debugging with breakpoints\n");
    printf("• Code formatting and style enforcement\n");
    printf("• Snippet templates for common patterns\n");
    
    wyn_ecosystem_free(ecosystem);
}

int main() {
    printf("Wyn Ecosystem Maturity and IDE Integration Examples\n");
    printf("===================================================\n\n");
    
    example_ecosystem_setup();
    example_ide_plugin_development();
    example_package_registry();
    example_community_tools();
    example_stdlib_expansion();
    example_ecosystem_metrics();
    example_ide_integration();
    
    printf("\n✓ All ecosystem maturity and IDE integration examples completed!\n");
    return 0;
}
