#include "types.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// T4.5.x: Third-Party Ecosystem Implementation

// Package Registry System
typedef struct {
    char* name;
    char* version;
    char* author;
    char* description;
    char* repository_url;
    size_t download_count;
    bool is_verified;
} WynThirdPartyPackage;

typedef struct {
    char* registry_url;
    WynThirdPartyPackage** packages;
    size_t package_count;
    size_t capacity;
} WynPackageRegistry;

// Community Tools
typedef struct {
    char* tool_name;
    char* executable_path;
    char* version;
    bool is_installed;
} WynCommunityTool;

typedef struct {
    WynCommunityTool** tools;
    size_t tool_count;
    size_t capacity;
} WynToolRegistry;

// Package Registry Management
WynPackageRegistry* wyn_registry_new(const char* url) {
    WynPackageRegistry* registry = malloc(sizeof(WynPackageRegistry));
    registry->registry_url = strdup(url);
    registry->packages = malloc(sizeof(WynThirdPartyPackage*) * 100);
    registry->package_count = 0;
    registry->capacity = 100;
    return registry;
}

WynThirdPartyPackage* wyn_package_new(const char* name, const char* version, const char* author) {
    WynThirdPartyPackage* package = malloc(sizeof(WynThirdPartyPackage));
    package->name = strdup(name);
    package->version = strdup(version);
    package->author = strdup(author);
    package->description = strdup("Third-party Wyn package");
    package->repository_url = strdup("https://github.com/example/repo");
    package->download_count = 0;
    package->is_verified = false;
    return package;
}

bool wyn_registry_add_package(WynPackageRegistry* registry, WynThirdPartyPackage* package) {
    if (!registry || !package) return false;
    
    if (registry->package_count >= registry->capacity) {
        registry->capacity *= 2;
        registry->packages = realloc(registry->packages, 
                                   sizeof(WynThirdPartyPackage*) * registry->capacity);
    }
    
    registry->packages[registry->package_count++] = package;
    return true;
}

WynThirdPartyPackage* wyn_registry_find_package(WynPackageRegistry* registry, const char* name) {
    if (!registry || !name) return NULL;
    
    for (size_t i = 0; i < registry->package_count; i++) {
        if (strcmp(registry->packages[i]->name, name) == 0) {
            return registry->packages[i];
        }
    }
    return NULL;
}

bool wyn_package_install(WynThirdPartyPackage* package) {
    if (!package) return false;
    
    printf("Installing package: %s v%s by %s\n", 
           package->name, package->version, package->author);
    
    // Simulate installation process
    package->download_count++;
    return true;
}

void wyn_package_free(WynThirdPartyPackage* package) {
    if (package) {
        free(package->name);
        free(package->version);
        free(package->author);
        free(package->description);
        free(package->repository_url);
        free(package);
    }
}

void wyn_registry_free(WynPackageRegistry* registry) {
    if (registry) {
        for (size_t i = 0; i < registry->package_count; i++) {
            wyn_package_free(registry->packages[i]);
        }
        free(registry->packages);
        free(registry->registry_url);
        free(registry);
    }
}

// Community Tools Management
WynToolRegistry* wyn_tool_registry_new(void) {
    WynToolRegistry* registry = malloc(sizeof(WynToolRegistry));
    registry->tools = malloc(sizeof(WynCommunityTool*) * 50);
    registry->tool_count = 0;
    registry->capacity = 50;
    return registry;
}

WynCommunityTool* wyn_community_tool_new(const char* name, const char* path, const char* version) {
    WynCommunityTool* tool = malloc(sizeof(WynCommunityTool));
    tool->tool_name = strdup(name);
    tool->executable_path = strdup(path);
    tool->version = strdup(version);
    tool->is_installed = false;
    return tool;
}

bool wyn_tool_registry_add(WynToolRegistry* registry, WynCommunityTool* tool) {
    if (!registry || !tool) return false;
    
    if (registry->tool_count >= registry->capacity) {
        registry->capacity *= 2;
        registry->tools = realloc(registry->tools, 
                                sizeof(WynCommunityTool*) * registry->capacity);
    }
    
    registry->tools[registry->tool_count++] = tool;
    return true;
}

WynCommunityTool* wyn_tool_registry_find(WynToolRegistry* registry, const char* name) {
    if (!registry || !name) return NULL;
    
    for (size_t i = 0; i < registry->tool_count; i++) {
        if (strcmp(registry->tools[i]->tool_name, name) == 0) {
            return registry->tools[i];
        }
    }
    return NULL;
}

bool wyn_community_tool_install(WynCommunityTool* tool) {
    if (!tool) return false;
    
    printf("Installing community tool: %s v%s\n", tool->tool_name, tool->version);
    tool->is_installed = true;
    return true;
}

void wyn_community_tool_free(WynCommunityTool* tool) {
    if (tool) {
        free(tool->tool_name);
        free(tool->executable_path);
        free(tool->version);
        free(tool);
    }
}

void wyn_tool_registry_free(WynToolRegistry* registry) {
    if (registry) {
        for (size_t i = 0; i < registry->tool_count; i++) {
            wyn_community_tool_free(registry->tools[i]);
        }
        free(registry->tools);
        free(registry);
    }
}

// Community Contribution Framework
typedef struct {
    char* contributor_name;
    char* email;
    size_t contributions;
    bool is_maintainer;
} WynContributor;

WynContributor* wyn_contributor_new(const char* name, const char* email) {
    WynContributor* contributor = malloc(sizeof(WynContributor));
    contributor->contributor_name = strdup(name);
    contributor->email = strdup(email);
    contributor->contributions = 0;
    contributor->is_maintainer = false;
    return contributor;
}

void wyn_contributor_add_contribution(WynContributor* contributor) {
    if (contributor) {
        contributor->contributions++;
        if (contributor->contributions >= 10) {
            contributor->is_maintainer = true;
        }
    }
}

void wyn_contributor_free(WynContributor* contributor) {
    if (contributor) {
        free(contributor->contributor_name);
        free(contributor->email);
        free(contributor);
    }
}

// Third-Party Ecosystem Statistics
static struct {
    size_t packages_registered;
    size_t packages_installed;
    size_t tools_registered;
    size_t contributors_active;
} ecosystem_stats = {0};

void wyn_ecosystem_print_stats(void) {
    printf("Third-Party Ecosystem Statistics:\n");
    printf("  Packages registered: %zu\n", ecosystem_stats.packages_registered);
    printf("  Packages installed: %zu\n", ecosystem_stats.packages_installed);
    printf("  Community tools: %zu\n", ecosystem_stats.tools_registered);
    printf("  Active contributors: %zu\n", ecosystem_stats.contributors_active);
}

void wyn_ecosystem_reset_stats(void) {
    ecosystem_stats.packages_registered = 0;
    ecosystem_stats.packages_installed = 0;
    ecosystem_stats.tools_registered = 0;
    ecosystem_stats.contributors_active = 0;
}
