#ifndef WYN_BUILD_H
#define WYN_BUILD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "package.h"

// Forward declarations
typedef struct WynBuildSystem WynBuildSystem;
typedef struct WynBuildTarget WynBuildTarget;
typedef struct WynBuildConfig WynBuildConfig;
typedef struct WynBuildCache WynBuildCache;

// Build target types
typedef enum {
    WYN_BUILD_EXECUTABLE,
    WYN_BUILD_LIBRARY,
    WYN_BUILD_STATIC_LIBRARY,
    WYN_BUILD_SHARED_LIBRARY,
    WYN_BUILD_TEST
} WynBuildTargetType;

// Build optimization levels
typedef enum {
    WYN_BUILD_DEBUG,
    WYN_BUILD_RELEASE,
    WYN_BUILD_RELEASE_WITH_DEBUG,
    WYN_BUILD_MIN_SIZE
} WynBuildOptLevel;

// Build platforms
typedef enum {
    WYN_BUILD_NATIVE,
    WYN_BUILD_WINDOWS_X64,
    WYN_BUILD_MACOS_X64,
    WYN_BUILD_MACOS_ARM64,
    WYN_BUILD_LINUX_X64,
    WYN_BUILD_LINUX_ARM64,
    WYN_BUILD_WASM32
} WynBuildPlatform;

// Build configuration
typedef struct WynBuildConfig {
    WynBuildOptLevel opt_level;
    WynBuildPlatform target_platform;
    bool debug_info;
    bool link_time_optimization;
    bool parallel_build;
    int max_parallel_jobs;
    char* output_dir;
    char* cache_dir;
    char** include_paths;
    size_t include_path_count;
    char** library_paths;
    size_t library_path_count;
    char** libraries;
    size_t library_count;
    char** compiler_flags;
    size_t compiler_flag_count;
    char** linker_flags;
    size_t linker_flag_count;
} WynBuildConfig;

// Build target
typedef struct WynBuildTarget {
    char* name;
    WynBuildTargetType type;
    char** source_files;
    size_t source_file_count;
    char** dependencies;
    size_t dependency_count;
    WynBuildConfig config;
    char* output_path;
    struct WynBuildTarget* next;
} WynBuildTarget;

// Build cache entry
typedef struct {
    char* file_path;
    uint64_t last_modified;
    char* checksum;
    char* object_path;
    bool needs_rebuild;
} WynBuildCacheEntry;

// Build cache
typedef struct WynBuildCache {
    WynBuildCacheEntry* entries;
    size_t entry_count;
    size_t entry_capacity;
    char* cache_file_path;
} WynBuildCache;

// Build system
typedef struct WynBuildSystem {
    WynPackageManager* package_manager;
    WynBuildTarget* targets;
    WynBuildCache* cache;
    WynBuildConfig default_config;
    char* project_root;
    char* build_dir;
    bool verbose;
    bool clean_build;
} WynBuildSystem;

// Build system lifecycle
WynBuildSystem* wyn_build_system_new(const char* project_root);
void wyn_build_system_free(WynBuildSystem* build_system);
bool wyn_build_system_load_config(WynBuildSystem* build_system, const char* config_path);
bool wyn_build_system_save_config(WynBuildSystem* build_system, const char* config_path);

// Target management
WynBuildTarget* wyn_build_target_new(const char* name, WynBuildTargetType type);
void wyn_build_target_free(WynBuildTarget* target);
bool wyn_build_system_add_target(WynBuildSystem* build_system, WynBuildTarget* target);
WynBuildTarget* wyn_build_system_find_target(WynBuildSystem* build_system, const char* name);
bool wyn_build_target_add_source(WynBuildTarget* target, const char* source_file);
bool wyn_build_target_add_dependency(WynBuildTarget* target, const char* dependency);

// Configuration management
WynBuildConfig wyn_build_config_default(void);
void wyn_build_config_free(WynBuildConfig* config);
bool wyn_build_config_add_include_path(WynBuildConfig* config, const char* path);
bool wyn_build_config_add_library_path(WynBuildConfig* config, const char* path);
bool wyn_build_config_add_library(WynBuildConfig* config, const char* library);
bool wyn_build_config_add_compiler_flag(WynBuildConfig* config, const char* flag);
bool wyn_build_config_add_linker_flag(WynBuildConfig* config, const char* flag);

// Build cache management
WynBuildCache* wyn_build_cache_new(const char* cache_dir);
void wyn_build_cache_free(WynBuildCache* cache);
bool wyn_build_cache_load(WynBuildCache* cache);
bool wyn_build_cache_save(WynBuildCache* cache);
bool wyn_build_cache_needs_rebuild(WynBuildCache* cache, const char* source_file);
bool wyn_build_cache_update_entry(WynBuildCache* cache, const char* source_file, 
                                  const char* object_file);

// Build execution
bool wyn_build_system_build_target(WynBuildSystem* build_system, const char* target_name);
bool wyn_build_system_build_all(WynBuildSystem* build_system);
bool wyn_build_system_clean(WynBuildSystem* build_system);
bool wyn_build_system_test(WynBuildSystem* build_system);

// Build file generation
bool wyn_build_generate_makefile(WynBuildSystem* build_system, const char* output_path);
bool wyn_build_generate_cmake(WynBuildSystem* build_system, const char* output_path);
bool wyn_build_generate_ninja(WynBuildSystem* build_system, const char* output_path);
bool wyn_build_generate_vscode_config(WynBuildSystem* build_system, const char* output_path);

// Dependency resolution
bool wyn_build_resolve_dependencies(WynBuildSystem* build_system);
char** wyn_build_get_build_order(WynBuildSystem* build_system, size_t* count);

// Parallel build support
typedef struct {
    WynBuildTarget* target;
    WynBuildSystem* build_system;
    bool success;
} WynBuildJob;

bool wyn_build_system_build_parallel(WynBuildSystem* build_system, int max_jobs);

// Utility functions
char* wyn_build_get_object_path(const char* source_file, const char* build_dir);
char* wyn_build_get_output_path(WynBuildTarget* target, const char* build_dir);
bool wyn_build_file_newer(const char* file1, const char* file2);
uint64_t wyn_build_file_checksum(const char* file_path);
bool wyn_build_create_directory(const char* path);
bool wyn_build_copy_file(const char* src, const char* dst);

// Platform-specific utilities
const char* wyn_build_get_platform_name(WynBuildPlatform platform);
const char* wyn_build_get_executable_extension(WynBuildPlatform platform);
const char* wyn_build_get_library_extension(WynBuildPlatform platform, bool shared);
char** wyn_build_get_platform_flags(WynBuildPlatform platform, size_t* count);

// Integration with package manager
bool wyn_build_integrate_packages(WynBuildSystem* build_system);
bool wyn_build_download_dependencies(WynBuildSystem* build_system);

#endif // WYN_BUILD_H
