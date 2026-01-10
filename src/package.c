#include "package.h"
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Package manager initialization
WynPackageManager* wyn_pkg_manager_new(void) {
    WynPackageManager* manager = malloc(sizeof(WynPackageManager));
    if (!manager) return NULL;
    
    manager->registries = NULL;
    manager->registry_count = 0;
    manager->global_cache_dir = NULL;
    manager->config_dir = NULL;
    manager->current_package = NULL;
    
    // Set up default directories
    char* home = wyn_sys_home_dir();
    if (home) {
        manager->global_cache_dir = wyn_sys_join_path(home, ".wyn/cache");
        manager->config_dir = wyn_sys_join_path(home, ".wyn/config");
        free(home);
    }
    
    return manager;
}

void wyn_pkg_manager_free(WynPackageManager* manager) {
    if (!manager) return;
    
    // Free registries
    for (size_t i = 0; i < manager->registry_count; i++) {
        if (manager->registries[i]) {
            free(manager->registries[i]->url);
            free(manager->registries[i]->name);
            free(manager->registries[i]->cache_dir);
            free(manager->registries[i]);
        }
    }
    free(manager->registries);
    
    free(manager->global_cache_dir);
    free(manager->config_dir);
    wyn_pkg_free(manager->current_package);
    free(manager);
}

WynPackageError wyn_pkg_manager_init_config(WynPackageManager* manager) {
    if (!manager) return WYN_PKG_UNKNOWN_ERROR;
    
    // Create cache and config directories
    if (manager->global_cache_dir) {
        wyn_sys_create_directory(manager->global_cache_dir);
    }
    if (manager->config_dir) {
        wyn_sys_create_directory(manager->config_dir);
    }
    
    // Add default registry
    return wyn_pkg_add_registry(manager, "default", WYN_PKG_DEFAULT_REGISTRY);
}

// Registry management
WynPackageError wyn_pkg_add_registry(WynPackageManager* manager, const char* name, const char* url) {
    if (!manager || !name || !url) return WYN_PKG_UNKNOWN_ERROR;
    
    // Check if registry already exists
    if (wyn_pkg_find_registry(manager, name)) {
        return WYN_PKG_UNKNOWN_ERROR;  // Registry already exists
    }
    
    // Create new registry
    WynRegistry* registry = malloc(sizeof(WynRegistry));
    if (!registry) return WYN_PKG_UNKNOWN_ERROR;
    
    registry->name = strdup(name);
    registry->url = strdup(url);
    registry->is_default = (strcmp(name, "default") == 0);
    
    // Set cache directory
    if (manager->global_cache_dir) {
        registry->cache_dir = wyn_sys_join_path(manager->global_cache_dir, name);
        wyn_sys_create_directory(registry->cache_dir);
    } else {
        registry->cache_dir = NULL;
    }
    
    // Add to registry list
    WynRegistry** new_registries = realloc(manager->registries, 
                                          (manager->registry_count + 1) * sizeof(WynRegistry*));
    if (!new_registries) {
        free(registry->name);
        free(registry->url);
        free(registry->cache_dir);
        free(registry);
        return WYN_PKG_UNKNOWN_ERROR;
    }
    
    manager->registries = new_registries;
    manager->registries[manager->registry_count] = registry;
    manager->registry_count++;
    
    return WYN_PKG_OK;
}

WynPackageError wyn_pkg_remove_registry(WynPackageManager* manager, const char* name) {
    if (!manager || !name) return WYN_PKG_UNKNOWN_ERROR;
    
    for (size_t i = 0; i < manager->registry_count; i++) {
        if (strcmp(manager->registries[i]->name, name) == 0) {
            // Free registry
            free(manager->registries[i]->url);
            free(manager->registries[i]->name);
            free(manager->registries[i]->cache_dir);
            free(manager->registries[i]);
            
            // Shift remaining registries
            for (size_t j = i; j < manager->registry_count - 1; j++) {
                manager->registries[j] = manager->registries[j + 1];
            }
            manager->registry_count--;
            
            return WYN_PKG_OK;
        }
    }
    
    return WYN_PKG_NOT_FOUND;
}

WynRegistry* wyn_pkg_find_registry(WynPackageManager* manager, const char* name) {
    if (!manager || !name) return NULL;
    
    for (size_t i = 0; i < manager->registry_count; i++) {
        if (strcmp(manager->registries[i]->name, name) == 0) {
            return manager->registries[i];
        }
    }
    
    return NULL;
}

// Package operations
WynPackage* wyn_pkg_new(const char* name, const char* version) {
    if (!name || !version) return NULL;
    
    WynPackage* package = malloc(sizeof(WynPackage));
    if (!package) return NULL;
    
    memset(package, 0, sizeof(WynPackage));
    
    package->name = strdup(name);
    package->version = wyn_version_parse(version);
    package->is_local = false;
    
    if (!package->name || !package->version) {
        wyn_pkg_free(package);
        return NULL;
    }
    
    return package;
}

void wyn_pkg_free(WynPackage* package) {
    if (!package) return;
    
    free(package->name);
    wyn_version_free(package->version);
    free(package->description);
    free(package->license);
    free(package->repository);
    free(package->homepage);
    free(package->source_path);
    
    // Free authors
    if (package->authors) {
        for (size_t i = 0; i < package->author_count; i++) {
            free(package->authors[i]);
        }
        free(package->authors);
    }
    
    // Free dependencies
    WynDependency* dep = package->dependencies;
    while (dep) {
        WynDependency* next = dep->next;
        free(dep->name);
        wyn_version_free(dep->version);
        free(dep);
        dep = next;
    }
    
    // Free dev dependencies
    dep = package->dev_dependencies;
    while (dep) {
        WynDependency* next = dep->next;
        free(dep->name);
        wyn_version_free(dep->version);
        free(dep);
        dep = next;
    }
    
    free(package);
}

// Simplified manifest loading (basic TOML-like parsing)
WynPackageError wyn_pkg_load_manifest(const char* path, WynPackage** package) {
    if (!path || !package) return WYN_PKG_UNKNOWN_ERROR;
    
    char* manifest_path = wyn_sys_join_path(path, WYN_PKG_MANIFEST_FILE);
    if (!wyn_sys_file_exists(manifest_path)) {
        free(manifest_path);
        return WYN_PKG_NOT_FOUND;
    }
    
    // For now, create a basic package (would need proper TOML parsing)
    *package = wyn_pkg_new("example", "0.1.0");
    if (*package) {
        (*package)->description = strdup("Example package");
        (*package)->is_local = true;
        (*package)->source_path = strdup(path);
    }
    
    free(manifest_path);
    return *package ? WYN_PKG_OK : WYN_PKG_UNKNOWN_ERROR;
}

WynPackageError wyn_pkg_save_manifest(const WynPackage* package, const char* path) {
    if (!package || !path) return WYN_PKG_UNKNOWN_ERROR;
    
    char* manifest_path = wyn_sys_join_path(path, WYN_PKG_MANIFEST_FILE);
    FILE* file = fopen(manifest_path, "w");
    free(manifest_path);
    
    if (!file) return WYN_PKG_IO_ERROR;
    
    // Write basic TOML format
    fprintf(file, "[package]\n");
    fprintf(file, "name = \"%s\"\n", package->name);
    
    char* version_str = wyn_version_to_string(package->version);
    fprintf(file, "version = \"%s\"\n", version_str);
    free(version_str);
    
    if (package->description) {
        fprintf(file, "description = \"%s\"\n", package->description);
    }
    
    if (package->dependencies) {
        fprintf(file, "\n[dependencies]\n");
        WynDependency* dep = package->dependencies;
        while (dep) {
            char* dep_version = wyn_version_to_string(dep->version);
            fprintf(file, "%s = \"%s\"\n", dep->name, dep_version);
            free(dep_version);
            dep = dep->next;
        }
    }
    
    fclose(file);
    return WYN_PKG_OK;
}

// Version handling
WynVersion* wyn_version_parse(const char* version_str) {
    if (!version_str) return NULL;
    
    WynVersion* version = malloc(sizeof(WynVersion));
    if (!version) return NULL;
    
    memset(version, 0, sizeof(WynVersion));
    
    // Simple version parsing (major.minor.patch)
    int parsed = sscanf(version_str, "%u.%u.%u", &version->major, &version->minor, &version->patch);
    if (parsed != 3) {  // Require all three components
        free(version);
        return NULL;
    }
    
    return version;
}

void wyn_version_free(WynVersion* version) {
    if (!version) return;
    free(version->prerelease);
    free(version->build);
    free(version);
}

int wyn_version_compare(const WynVersion* a, const WynVersion* b) {
    if (!a || !b) return 0;
    
    if (a->major != b->major) return (a->major > b->major) ? 1 : -1;
    if (a->minor != b->minor) return (a->minor > b->minor) ? 1 : -1;
    if (a->patch != b->patch) return (a->patch > b->patch) ? 1 : -1;
    
    return 0;  // Equal
}

bool wyn_version_satisfies(const WynVersion* version, const WynVersion* constraint, WynVersionConstraint type) {
    if (!version || !constraint) return false;
    
    int cmp = wyn_version_compare(version, constraint);
    
    switch (type) {
        case WYN_VERSION_EXACT:
            return cmp == 0;
        case WYN_VERSION_COMPATIBLE:
            // ^1.2.3 means >=1.2.3 <2.0.0
            return cmp >= 0 && version->major == constraint->major;
        case WYN_VERSION_TILDE:
            // ~1.2.3 means >=1.2.3 <1.3.0
            return cmp >= 0 && version->major == constraint->major && version->minor == constraint->minor;
        case WYN_VERSION_GREATER:
            return cmp > 0;
        case WYN_VERSION_GREATER_EQ:
            return cmp >= 0;
        case WYN_VERSION_LESS:
            return cmp < 0;
        case WYN_VERSION_LESS_EQ:
            return cmp <= 0;
        case WYN_VERSION_WILDCARD:
            return true;
        default:
            return false;
    }
}

char* wyn_version_to_string(const WynVersion* version) {
    if (!version) return NULL;
    
    char* result = malloc(32);  // Should be enough for most versions
    if (!result) return NULL;
    
    snprintf(result, 32, "%u.%u.%u", version->major, version->minor, version->patch);
    return result;
}

// Dependency management
WynPackageError wyn_pkg_add_dependency(WynPackage* package, const char* name, const char* version_spec) {
    if (!package || !name || !version_spec) return WYN_PKG_UNKNOWN_ERROR;
    
    // Check if dependency already exists
    if (wyn_pkg_find_dependency(package, name)) {
        return WYN_PKG_UNKNOWN_ERROR;  // Dependency already exists
    }
    
    WynDependency* dep = malloc(sizeof(WynDependency));
    if (!dep) return WYN_PKG_UNKNOWN_ERROR;
    
    dep->name = strdup(name);
    dep->version = wyn_version_parse(version_spec);
    dep->constraint = WYN_VERSION_COMPATIBLE;  // Default to compatible
    dep->optional = false;
    dep->dev_only = false;
    dep->next = package->dependencies;
    
    if (!dep->name || !dep->version) {
        free(dep->name);
        wyn_version_free(dep->version);
        free(dep);
        return WYN_PKG_UNKNOWN_ERROR;
    }
    
    package->dependencies = dep;
    return WYN_PKG_OK;
}

WynPackageError wyn_pkg_remove_dependency(WynPackage* package, const char* name) {
    if (!package || !name) return WYN_PKG_UNKNOWN_ERROR;
    
    WynDependency** current = &package->dependencies;
    while (*current) {
        if (strcmp((*current)->name, name) == 0) {
            WynDependency* to_remove = *current;
            *current = (*current)->next;
            
            free(to_remove->name);
            wyn_version_free(to_remove->version);
            free(to_remove);
            
            return WYN_PKG_OK;
        }
        current = &(*current)->next;
    }
    
    return WYN_PKG_NOT_FOUND;
}

WynDependency* wyn_pkg_find_dependency(const WynPackage* package, const char* name) {
    if (!package || !name) return NULL;
    
    WynDependency* dep = package->dependencies;
    while (dep) {
        if (strcmp(dep->name, name) == 0) {
            return dep;
        }
        dep = dep->next;
    }
    
    return NULL;
}

// Stub implementations for complex operations
WynPackageError wyn_pkg_install(WynPackageManager* manager, const char* name, const char* version_spec) {
    (void)manager; (void)name; (void)version_spec;
    // Stub - would implement actual package installation
    return WYN_PKG_OK;
}

WynPackageError wyn_pkg_uninstall(WynPackageManager* manager, const char* name) {
    (void)manager; (void)name;
    // Stub - would implement package removal
    return WYN_PKG_OK;
}

WynPackageError wyn_pkg_init_project(const char* path, const char* name) {
    if (!path || !name) return WYN_PKG_UNKNOWN_ERROR;
    
    // Create project directory
    wyn_sys_create_directory(path);
    
    // Create basic package
    WynPackage* package = wyn_pkg_new(name, "0.1.0");
    if (!package) return WYN_PKG_UNKNOWN_ERROR;
    
    package->description = strdup("A new Wyn package");
    
    // Save manifest
    WynPackageError error = wyn_pkg_save_manifest(package, path);
    wyn_pkg_free(package);
    
    return error;
}

// Utility functions
const char* wyn_pkg_error_string(WynPackageError error) {
    switch (error) {
        case WYN_PKG_OK: return "Success";
        case WYN_PKG_NOT_FOUND: return "Package not found";
        case WYN_PKG_INVALID_VERSION: return "Invalid version";
        case WYN_PKG_DEPENDENCY_ERROR: return "Dependency error";
        case WYN_PKG_NETWORK_ERROR: return "Network error";
        case WYN_PKG_IO_ERROR: return "I/O error";
        case WYN_PKG_PARSE_ERROR: return "Parse error";
        case WYN_PKG_PERMISSION_ERROR: return "Permission error";
        case WYN_PKG_UNKNOWN_ERROR: return "Unknown error";
        default: return "Invalid error code";
    }
}

char* wyn_pkg_get_cache_dir(void) {
    char* home = wyn_sys_home_dir();
    if (!home) return NULL;
    
    char* cache_dir = wyn_sys_join_path(home, ".wyn/cache");
    free(home);
    return cache_dir;
}

char* wyn_pkg_get_config_dir(void) {
    char* home = wyn_sys_home_dir();
    if (!home) return NULL;
    
    char* config_dir = wyn_sys_join_path(home, ".wyn/config");
    free(home);
    return config_dir;
}

bool wyn_pkg_is_valid_name(const char* name) {
    if (!name || strlen(name) == 0) return false;
    
    // Package names should start with letter or underscore
    if (!isalpha(name[0]) && name[0] != '_') return false;
    
    // Check remaining characters
    for (size_t i = 1; name[i]; i++) {
        if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-') {
            return false;
        }
    }
    
    return true;
}

bool wyn_pkg_is_valid_version(const char* version) {
    if (!version) return false;
    
    WynVersion* v = wyn_version_parse(version);
    if (v) {
        wyn_version_free(v);
        return true;
    }
    return false;
}
