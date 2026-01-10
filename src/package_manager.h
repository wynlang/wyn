#ifndef WYN_PACKAGE_MANAGER_H
#define WYN_PACKAGE_MANAGER_H

#include <stddef.h>
#include <stdbool.h>

// Package metadata structure
typedef struct {
    char* name;
    char* version;
    char* description;
    char** dependencies;
    size_t dependency_count;
    char* author;
    char* license;
} PackageMetadata;

// Package registry entry
typedef struct {
    PackageMetadata metadata;
    char* download_url;
    char* checksum;
    bool is_local;
} PackageEntry;

// Package manager operations
int wyn_pkg_init(const char* project_dir);
int wyn_pkg_install(const char* package_name, const char* version);
int wyn_pkg_uninstall(const char* package_name);
int wyn_pkg_update(const char* package_name);
int wyn_pkg_list_installed(void);
int wyn_pkg_search(const char* query);
int wyn_pkg_publish(const char* package_dir);

// Dependency resolution
int wyn_pkg_resolve_dependencies(const char* package_name);
bool wyn_pkg_check_compatibility(const char* package_name, const char* version);

// Package validation
bool wyn_pkg_validate_metadata(const PackageMetadata* metadata);
bool wyn_pkg_verify_checksum(const char* file_path, const char* expected_checksum);

// Registry operations
int wyn_pkg_registry_add(const char* registry_url);
int wyn_pkg_registry_remove(const char* registry_url);
int wyn_pkg_registry_list(void);

// Cleanup
void wyn_pkg_cleanup(void);

#endif // WYN_PACKAGE_MANAGER_H
