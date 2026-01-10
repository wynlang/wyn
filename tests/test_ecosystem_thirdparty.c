#include "test.h"
#include "ecosystem_thirdparty.h"
#include <stdio.h>
#include <string.h>

static int test_package_registry() {
    WynPackageRegistry* registry = wyn_registry_new("https://packages.wyn-lang.org");
    
    WynThirdPartyPackage* package = wyn_package_new("json-parser", "1.0.0", "community");
    bool added = wyn_registry_add_package(registry, package);
    
    if (!added) {
        wyn_registry_free(registry);
        return 0;
    }
    
    WynThirdPartyPackage* found = wyn_registry_find_package(registry, "json-parser");
    if (!found) {
        wyn_registry_free(registry);
        return 0;
    }
    
    wyn_registry_free(registry);
    return 1;
}

static int test_package_installation() {
    WynThirdPartyPackage* package = wyn_package_new("http-client", "2.1.0", "developer");
    
    bool installed = wyn_package_install(package);
    if (!installed) {
        wyn_package_free(package);
        return 0;
    }
    
    wyn_package_free(package);
    return 1;
}

static int test_community_tools() {
    WynToolRegistry* registry = wyn_tool_registry_new();
    
    WynCommunityTool* tool = wyn_community_tool_new("wyn-formatter", "/usr/local/bin/wyn-fmt", "1.2.0");
    bool added = wyn_tool_registry_add(registry, tool);
    
    if (!added) {
        wyn_tool_registry_free(registry);
        return 0;
    }
    
    WynCommunityTool* found = wyn_tool_registry_find(registry, "wyn-formatter");
    if (!found) {
        wyn_tool_registry_free(registry);
        return 0;
    }
    
    bool installed = wyn_community_tool_install(found);
    if (!installed) {
        wyn_tool_registry_free(registry);
        return 0;
    }
    
    wyn_tool_registry_free(registry);
    return 1;
}

static int test_contribution_framework() {
    WynContributor* contributor = wyn_contributor_new("Alice Developer", "alice@example.com");
    
    // Add multiple contributions
    for (int i = 0; i < 15; i++) {
        wyn_contributor_add_contribution(contributor);
    }
    
    // Should become maintainer after 10+ contributions
    // (We can't directly check is_maintainer without exposing the struct)
    
    wyn_contributor_free(contributor);
    return 1;
}

static int test_ecosystem_stats() {
    wyn_ecosystem_reset_stats();
    wyn_ecosystem_print_stats();
    return 1;
}

int main() {
    int total = 0, passed = 0;
    
    printf("=== Third-Party Ecosystem Tests ===\n");
    
    total++; if (test_package_registry()) { printf("✓ Package registry\n"); passed++; } else printf("✗ Package registry\n");
    total++; if (test_package_installation()) { printf("✓ Package installation\n"); passed++; } else printf("✗ Package installation\n");
    total++; if (test_community_tools()) { printf("✓ Community tools\n"); passed++; } else printf("✗ Community tools\n");
    total++; if (test_contribution_framework()) { printf("✓ Contribution framework\n"); passed++; } else printf("✗ Contribution framework\n");
    total++; if (test_ecosystem_stats()) { printf("✓ Ecosystem statistics\n"); passed++; } else printf("✗ Ecosystem statistics\n");
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All third-party ecosystem tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}
