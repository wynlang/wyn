#ifndef WYN_PACKAGE_H
#define WYN_PACKAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynPackage WynPackage;
typedef struct WynDependency WynDependency;
typedef struct WynRegistry WynRegistry;
typedef struct WynPackageManager WynPackageManager;

// Package manager error codes
typedef enum {
    WYN_PKG_OK,
    WYN_PKG_NOT_FOUND,
    WYN_PKG_INVALID_VERSION,
    WYN_PKG_DEPENDENCY_ERROR,
    WYN_PKG_NETWORK_ERROR,
    WYN_PKG_IO_ERROR,
    WYN_PKG_PARSE_ERROR,
    WYN_PKG_PERMISSION_ERROR,
    WYN_PKG_UNKNOWN_ERROR
} WynPackageError;

// Version constraint types
typedef enum {
    WYN_VERSION_EXACT,      // =1.0.0
    WYN_VERSION_COMPATIBLE, // ^1.0.0
    WYN_VERSION_TILDE,      // ~1.0.0
    WYN_VERSION_GREATER,    // >1.0.0
    WYN_VERSION_GREATER_EQ, // >=1.0.0
    WYN_VERSION_LESS,       // <1.0.0
    WYN_VERSION_LESS_EQ,    // <=1.0.0
    WYN_VERSION_WILDCARD    // *
} WynVersionConstraint;

// Semantic version structure
typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    char* prerelease;
    char* build;
} WynVersion;

// Dependency structure
typedef struct WynDependency {
    char* name;
    WynVersion* version;
    WynVersionConstraint constraint;
    bool optional;
    bool dev_only;
    struct WynDependency* next;
} WynDependency;

// Package structure
typedef struct WynPackage {
    char* name;
    WynVersion* version;
    char* description;
    char** authors;
    size_t author_count;
    char* license;
    char* repository;
    char* homepage;
    WynDependency* dependencies;
    WynDependency* dev_dependencies;
    char* source_path;
    bool is_local;
} WynPackage;

// Registry structure
typedef struct WynRegistry {
    char* url;
    char* name;
    char* cache_dir;
    bool is_default;
} WynRegistry;

// Package manager structure
typedef struct WynPackageManager {
    WynRegistry** registries;
    size_t registry_count;
    char* global_cache_dir;
    char* config_dir;
    WynPackage* current_package;
} WynPackageManager;

// Package manager initialization
WynPackageManager* wyn_pkg_manager_new(void);
void wyn_pkg_manager_free(WynPackageManager* manager);
WynPackageError wyn_pkg_manager_init_config(WynPackageManager* manager);

// Registry management
WynPackageError wyn_pkg_add_registry(WynPackageManager* manager, const char* name, const char* url);
WynPackageError wyn_pkg_remove_registry(WynPackageManager* manager, const char* name);
WynRegistry* wyn_pkg_find_registry(WynPackageManager* manager, const char* name);

// Package operations
WynPackage* wyn_pkg_new(const char* name, const char* version);
void wyn_pkg_free(WynPackage* package);
WynPackageError wyn_pkg_load_manifest(const char* path, WynPackage** package);
WynPackageError wyn_pkg_save_manifest(const WynPackage* package, const char* path);

// Dependency management
WynPackageError wyn_pkg_add_dependency(WynPackage* package, const char* name, const char* version_spec);
WynPackageError wyn_pkg_remove_dependency(WynPackage* package, const char* name);
WynDependency* wyn_pkg_find_dependency(const WynPackage* package, const char* name);

// Version handling
WynVersion* wyn_version_parse(const char* version_str);
void wyn_version_free(WynVersion* version);
int wyn_version_compare(const WynVersion* a, const WynVersion* b);
bool wyn_version_satisfies(const WynVersion* version, const WynVersion* constraint, WynVersionConstraint type);
char* wyn_version_to_string(const WynVersion* version);

// Package installation
WynPackageError wyn_pkg_install(WynPackageManager* manager, const char* name, const char* version_spec);
WynPackageError wyn_pkg_uninstall(WynPackageManager* manager, const char* name);
WynPackageError wyn_pkg_update(WynPackageManager* manager, const char* name);
WynPackageError wyn_pkg_update_all(WynPackageManager* manager);

// Package search and info
WynPackage** wyn_pkg_search(WynPackageManager* manager, const char* query, size_t* count);
WynPackage* wyn_pkg_get_info(WynPackageManager* manager, const char* name);
WynPackage** wyn_pkg_list_installed(WynPackageManager* manager, size_t* count);

// Dependency resolution
typedef struct WynDependencyGraph WynDependencyGraph;
WynDependencyGraph* wyn_pkg_resolve_dependencies(WynPackageManager* manager, const WynPackage* package);
void wyn_pkg_free_dependency_graph(WynDependencyGraph* graph);
WynPackage** wyn_pkg_get_install_order(const WynDependencyGraph* graph, size_t* count);

// Cache management
WynPackageError wyn_pkg_cache_package(WynPackageManager* manager, const WynPackage* package);
WynPackage* wyn_pkg_get_cached(WynPackageManager* manager, const char* name, const char* version);
WynPackageError wyn_pkg_clear_cache(WynPackageManager* manager);

// Project initialization
WynPackageError wyn_pkg_init_project(const char* path, const char* name);
WynPackageError wyn_pkg_load_project(const char* path, WynPackage** package);

// Utility functions
const char* wyn_pkg_error_string(WynPackageError error);
char* wyn_pkg_get_cache_dir(void);
char* wyn_pkg_get_config_dir(void);
bool wyn_pkg_is_valid_name(const char* name);
bool wyn_pkg_is_valid_version(const char* version);

// Constants
#define WYN_PKG_MANIFEST_FILE "wyn.toml"
#define WYN_PKG_LOCK_FILE "wyn.lock"
#define WYN_PKG_DEFAULT_REGISTRY "https://packages.wyn-lang.org"
#define WYN_PKG_CACHE_DIR ".wyn/cache"
#define WYN_PKG_CONFIG_DIR ".wyn/config"

#endif // WYN_PACKAGE_H
