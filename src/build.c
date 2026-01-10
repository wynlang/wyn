#include "build.h"
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Build system lifecycle
WynBuildSystem* wyn_build_system_new(const char* project_root) {
    if (!project_root) return NULL;
    
    WynBuildSystem* build_system = malloc(sizeof(WynBuildSystem));
    if (!build_system) return NULL;
    
    memset(build_system, 0, sizeof(WynBuildSystem));
    
    build_system->project_root = strdup(project_root);
    
    // Create build directory path
    size_t build_dir_len = strlen(project_root) + 10; // "/build" + null
    build_system->build_dir = malloc(build_dir_len);
    snprintf(build_system->build_dir, build_dir_len, "%s/build", project_root);
    
    // Initialize default configuration
    build_system->default_config = wyn_build_config_default();
    build_system->default_config.output_dir = strdup(build_system->build_dir);
    
    // Create cache directory path
    size_t cache_dir_len = strlen(project_root) + 20; // "/.wyn/cache" + null
    char* cache_dir = malloc(cache_dir_len);
    snprintf(cache_dir, cache_dir_len, "%s/.wyn/cache", project_root);
    build_system->default_config.cache_dir = cache_dir;
    
    // Initialize build cache
    build_system->cache = wyn_build_cache_new(cache_dir);
    
    // Create package manager
    build_system->package_manager = wyn_pkg_manager_new();
    
    return build_system;
}

void wyn_build_system_free(WynBuildSystem* build_system) {
    if (!build_system) return;
    
    free(build_system->project_root);
    free(build_system->build_dir);
    
    // Free targets
    WynBuildTarget* target = build_system->targets;
    while (target) {
        WynBuildTarget* next = target->next;
        wyn_build_target_free(target);
        target = next;
    }
    
    wyn_build_config_free(&build_system->default_config);
    wyn_build_cache_free(build_system->cache);
    wyn_pkg_manager_free(build_system->package_manager);
    
    free(build_system);
}

// Target management
WynBuildTarget* wyn_build_target_new(const char* name, WynBuildTargetType type) {
    if (!name) return NULL;
    
    WynBuildTarget* target = malloc(sizeof(WynBuildTarget));
    if (!target) return NULL;
    
    memset(target, 0, sizeof(WynBuildTarget));
    
    target->name = strdup(name);
    target->type = type;
    target->config = wyn_build_config_default();
    
    return target;
}

void wyn_build_target_free(WynBuildTarget* target) {
    if (!target) return;
    
    free(target->name);
    free(target->output_path);
    
    // Free source files
    for (size_t i = 0; i < target->source_file_count; i++) {
        free(target->source_files[i]);
    }
    free(target->source_files);
    
    // Free dependencies
    for (size_t i = 0; i < target->dependency_count; i++) {
        free(target->dependencies[i]);
    }
    free(target->dependencies);
    
    wyn_build_config_free(&target->config);
    free(target);
}

bool wyn_build_system_add_target(WynBuildSystem* build_system, WynBuildTarget* target) {
    if (!build_system || !target) return false;
    
    target->next = build_system->targets;
    build_system->targets = target;
    
    return true;
}

WynBuildTarget* wyn_build_system_find_target(WynBuildSystem* build_system, const char* name) {
    if (!build_system || !name) return NULL;
    
    WynBuildTarget* target = build_system->targets;
    while (target) {
        if (strcmp(target->name, name) == 0) {
            return target;
        }
        target = target->next;
    }
    
    return NULL;
}

bool wyn_build_target_add_source(WynBuildTarget* target, const char* source_file) {
    if (!target || !source_file) return false;
    
    // Resize array
    target->source_files = realloc(target->source_files, 
                                  (target->source_file_count + 1) * sizeof(char*));
    if (!target->source_files) return false;
    
    target->source_files[target->source_file_count] = strdup(source_file);
    target->source_file_count++;
    
    return true;
}

bool wyn_build_target_add_dependency(WynBuildTarget* target, const char* dependency) {
    if (!target || !dependency) return false;
    
    // Resize array
    target->dependencies = realloc(target->dependencies, 
                                  (target->dependency_count + 1) * sizeof(char*));
    if (!target->dependencies) return false;
    
    target->dependencies[target->dependency_count] = strdup(dependency);
    target->dependency_count++;
    
    return true;
}

// Configuration management
WynBuildConfig wyn_build_config_default(void) {
    WynBuildConfig config;
    memset(&config, 0, sizeof(WynBuildConfig));
    
    config.opt_level = WYN_BUILD_DEBUG;
    config.target_platform = WYN_BUILD_NATIVE;
    config.debug_info = true;
    config.link_time_optimization = false;
    config.parallel_build = true;
    config.max_parallel_jobs = 4;
    
    return config;
}

void wyn_build_config_free(WynBuildConfig* config) {
    if (!config) return;
    
    free(config->output_dir);
    free(config->cache_dir);
    
    // Free arrays
    for (size_t i = 0; i < config->include_path_count; i++) {
        free(config->include_paths[i]);
    }
    free(config->include_paths);
    
    for (size_t i = 0; i < config->library_path_count; i++) {
        free(config->library_paths[i]);
    }
    free(config->library_paths);
    
    for (size_t i = 0; i < config->library_count; i++) {
        free(config->libraries[i]);
    }
    free(config->libraries);
    
    for (size_t i = 0; i < config->compiler_flag_count; i++) {
        free(config->compiler_flags[i]);
    }
    free(config->compiler_flags);
    
    for (size_t i = 0; i < config->linker_flag_count; i++) {
        free(config->linker_flags[i]);
    }
    free(config->linker_flags);
}

bool wyn_build_config_add_include_path(WynBuildConfig* config, const char* path) {
    if (!config || !path) return false;
    
    config->include_paths = realloc(config->include_paths, 
                                   (config->include_path_count + 1) * sizeof(char*));
    if (!config->include_paths) return false;
    
    config->include_paths[config->include_path_count] = strdup(path);
    config->include_path_count++;
    
    return true;
}

bool wyn_build_config_add_library(WynBuildConfig* config, const char* library) {
    if (!config || !library) return false;
    
    config->libraries = realloc(config->libraries, 
                               (config->library_count + 1) * sizeof(char*));
    if (!config->libraries) return false;
    
    config->libraries[config->library_count] = strdup(library);
    config->library_count++;
    
    return true;
}

// Build cache management
WynBuildCache* wyn_build_cache_new(const char* cache_dir) {
    if (!cache_dir) return NULL;
    
    WynBuildCache* cache = malloc(sizeof(WynBuildCache));
    if (!cache) return NULL;
    
    memset(cache, 0, sizeof(WynBuildCache));
    
    // Create cache file path
    size_t cache_file_len = strlen(cache_dir) + 20; // "/build.cache" + null
    cache->cache_file_path = malloc(cache_file_len);
    snprintf(cache->cache_file_path, cache_file_len, "%s/build.cache", cache_dir);
    
    cache->entry_capacity = 100;
    cache->entries = malloc(cache->entry_capacity * sizeof(WynBuildCacheEntry));
    
    return cache;
}

void wyn_build_cache_free(WynBuildCache* cache) {
    if (!cache) return;
    
    for (size_t i = 0; i < cache->entry_count; i++) {
        free(cache->entries[i].file_path);
        free(cache->entries[i].checksum);
        free(cache->entries[i].object_path);
    }
    
    free(cache->entries);
    free(cache->cache_file_path);
    free(cache);
}

bool wyn_build_cache_needs_rebuild(WynBuildCache* cache, const char* source_file) {
    if (!cache || !source_file) return true;
    
    // Find cache entry
    for (size_t i = 0; i < cache->entry_count; i++) {
        if (strcmp(cache->entries[i].file_path, source_file) == 0) {
            // Check if file was modified
            struct stat st;
            if (stat(source_file, &st) != 0) return true;
            
            return (uint64_t)st.st_mtime > cache->entries[i].last_modified;
        }
    }
    
    // Not in cache, needs build
    return true;
}

// Build execution
bool wyn_build_system_build_target(WynBuildSystem* build_system, const char* target_name) {
    if (!build_system || !target_name) return false;
    
    WynBuildTarget* target = wyn_build_system_find_target(build_system, target_name);
    if (!target) return false;
    
    if (build_system->verbose) {
        printf("Building target: %s\n", target->name);
    }
    
    // Create build directory
    wyn_build_create_directory(build_system->build_dir);
    
    // Compile source files
    for (size_t i = 0; i < target->source_file_count; i++) {
        const char* source_file = target->source_files[i];
        
        // Check if rebuild needed
        if (!build_system->clean_build && 
            !wyn_build_cache_needs_rebuild(build_system->cache, source_file)) {
            if (build_system->verbose) {
                printf("Skipping %s (up to date)\n", source_file);
            }
            continue;
        }
        
        // Get object file path
        char* object_file = wyn_build_get_object_path(source_file, build_system->build_dir);
        if (!object_file) return false;
        
        // Build compile command
        char compile_cmd[2048];
        const char* opt_flag = (target->config.opt_level == WYN_BUILD_DEBUG) ? "-g" : "-O2";
        
        snprintf(compile_cmd, sizeof(compile_cmd), 
                "gcc -c %s %s -o %s", opt_flag, source_file, object_file);
        
        if (build_system->verbose) {
            printf("Compiling: %s\n", compile_cmd);
        }
        
        int result = system(compile_cmd);
        if (result != 0) {
            free(object_file);
            return false;
        }
        
        // Update cache
        wyn_build_cache_update_entry(build_system->cache, source_file, object_file);
        free(object_file);
    }
    
    // Link target
    char* output_path = wyn_build_get_output_path(target, build_system->build_dir);
    if (!output_path) return false;
    
    char link_cmd[4096] = "gcc -o ";
    strcat(link_cmd, output_path);
    
    // Add object files
    for (size_t i = 0; i < target->source_file_count; i++) {
        char* object_file = wyn_build_get_object_path(target->source_files[i], 
                                                     build_system->build_dir);
        if (object_file) {
            strcat(link_cmd, " ");
            strcat(link_cmd, object_file);
            free(object_file);
        }
    }
    
    if (build_system->verbose) {
        printf("Linking: %s\n", link_cmd);
    }
    
    int result = system(link_cmd);
    free(output_path);
    
    return result == 0;
}

bool wyn_build_system_build_all(WynBuildSystem* build_system) {
    if (!build_system) return false;
    
    WynBuildTarget* target = build_system->targets;
    while (target) {
        if (!wyn_build_system_build_target(build_system, target->name)) {
            return false;
        }
        target = target->next;
    }
    
    return true;
}

bool wyn_build_system_clean(WynBuildSystem* build_system) {
    if (!build_system) return false;
    
    // Remove build directory
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", build_system->build_dir);
    
    if (build_system->verbose) {
        printf("Cleaning: %s\n", rm_cmd);
    }
    
    return system(rm_cmd) == 0;
}

// Build file generation
bool wyn_build_generate_makefile(WynBuildSystem* build_system, const char* output_path) {
    if (!build_system || !output_path) return false;
    
    FILE* makefile = fopen(output_path, "w");
    if (!makefile) return false;
    
    fprintf(makefile, "# Generated Makefile for Wyn project\n\n");
    fprintf(makefile, "CC = gcc\n");
    fprintf(makefile, "CFLAGS = -Wall -Wextra -std=c11\n");
    fprintf(makefile, "BUILD_DIR = %s\n\n", build_system->build_dir);
    
    // Generate targets
    WynBuildTarget* target = build_system->targets;
    while (target) {
        char* output_path = wyn_build_get_output_path(target, build_system->build_dir);
        if (output_path) {
            fprintf(makefile, "%s:", target->name);
            
            // Add object files as dependencies
            for (size_t i = 0; i < target->source_file_count; i++) {
                char* object_file = wyn_build_get_object_path(target->source_files[i], 
                                                             build_system->build_dir);
                if (object_file) {
                    fprintf(makefile, " %s", object_file);
                    free(object_file);
                }
            }
            
            fprintf(makefile, "\n\t$(CC) -o %s", output_path);
            
            // Add object files to link command
            for (size_t i = 0; i < target->source_file_count; i++) {
                char* object_file = wyn_build_get_object_path(target->source_files[i], 
                                                             build_system->build_dir);
                if (object_file) {
                    fprintf(makefile, " %s", object_file);
                    free(object_file);
                }
            }
            
            fprintf(makefile, "\n\n");
            free(output_path);
        }
        
        target = target->next;
    }
    
    fprintf(makefile, "clean:\n\trm -rf %s\n\n", build_system->build_dir);
    fprintf(makefile, ".PHONY: clean\n");
    
    fclose(makefile);
    return true;
}

// Utility functions
char* wyn_build_get_object_path(const char* source_file, const char* build_dir) {
    if (!source_file || !build_dir) return NULL;
    
    // Extract filename without extension
    const char* filename = strrchr(source_file, '/');
    if (!filename) filename = source_file;
    else filename++; // Skip the '/'
    
    // Find extension
    const char* ext = strrchr(filename, '.');
    size_t name_len = ext ? (size_t)(ext - filename) : strlen(filename);
    
    // Create object path
    size_t path_len = strlen(build_dir) + name_len + 10; // ".o" + "/" + null
    char* object_path = malloc(path_len);
    if (!object_path) return NULL;
    
    snprintf(object_path, path_len, "%s/%.*s.o", build_dir, (int)name_len, filename);
    
    return object_path;
}

char* wyn_build_get_output_path(WynBuildTarget* target, const char* build_dir) {
    if (!target || !build_dir) return NULL;
    
    const char* extension = "";
    if (target->type == WYN_BUILD_EXECUTABLE) {
        extension = "";
    } else if (target->type == WYN_BUILD_SHARED_LIBRARY) {
        extension = ".so";
    } else if (target->type == WYN_BUILD_STATIC_LIBRARY) {
        extension = ".a";
    }
    
    size_t path_len = strlen(build_dir) + strlen(target->name) + strlen(extension) + 10;
    char* output_path = malloc(path_len);
    if (!output_path) return NULL;
    
    snprintf(output_path, path_len, "%s/%s%s", build_dir, target->name, extension);
    
    return output_path;
}

bool wyn_build_create_directory(const char* path) {
    if (!path) return false;
    
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", path);
    
    return system(mkdir_cmd) == 0;
}

bool wyn_build_cache_update_entry(WynBuildCache* cache, const char* source_file, 
                                  const char* object_file) {
    if (!cache || !source_file || !object_file) return false;
    
    // Get file modification time
    struct stat st;
    if (stat(source_file, &st) != 0) return false;
    
    // Find existing entry or create new one
    WynBuildCacheEntry* entry = NULL;
    for (size_t i = 0; i < cache->entry_count; i++) {
        if (strcmp(cache->entries[i].file_path, source_file) == 0) {
            entry = &cache->entries[i];
            break;
        }
    }
    
    if (!entry) {
        // Create new entry
        if (cache->entry_count >= cache->entry_capacity) {
            cache->entry_capacity *= 2;
            cache->entries = realloc(cache->entries, 
                                   cache->entry_capacity * sizeof(WynBuildCacheEntry));
            if (!cache->entries) return false;
        }
        
        entry = &cache->entries[cache->entry_count++];
        memset(entry, 0, sizeof(WynBuildCacheEntry));
        entry->file_path = strdup(source_file);
    }
    
    // Update entry
    entry->last_modified = (uint64_t)st.st_mtime;
    free(entry->object_path);
    entry->object_path = strdup(object_file);
    entry->needs_rebuild = false;
    
    return true;
}

// Stub implementations for unimplemented features
bool wyn_build_system_load_config(WynBuildSystem* build_system, const char* config_path) {
    (void)build_system; (void)config_path;
    return false; // Stub
}

bool wyn_build_system_save_config(WynBuildSystem* build_system, const char* config_path) {
    (void)build_system; (void)config_path;
    return false; // Stub
}

bool wyn_build_config_add_library_path(WynBuildConfig* config, const char* path) {
    (void)config; (void)path;
    return false; // Stub
}

bool wyn_build_config_add_compiler_flag(WynBuildConfig* config, const char* flag) {
    (void)config; (void)flag;
    return false; // Stub
}

bool wyn_build_config_add_linker_flag(WynBuildConfig* config, const char* flag) {
    (void)config; (void)flag;
    return false; // Stub
}

bool wyn_build_cache_load(WynBuildCache* cache) {
    (void)cache;
    return false; // Stub
}

bool wyn_build_cache_save(WynBuildCache* cache) {
    (void)cache;
    return false; // Stub
}

bool wyn_build_system_test(WynBuildSystem* build_system) {
    (void)build_system;
    return false; // Stub
}

bool wyn_build_generate_cmake(WynBuildSystem* build_system, const char* output_path) {
    (void)build_system; (void)output_path;
    return false; // Stub
}

bool wyn_build_generate_ninja(WynBuildSystem* build_system, const char* output_path) {
    (void)build_system; (void)output_path;
    return false; // Stub
}

bool wyn_build_generate_vscode_config(WynBuildSystem* build_system, const char* output_path) {
    (void)build_system; (void)output_path;
    return false; // Stub
}

bool wyn_build_resolve_dependencies(WynBuildSystem* build_system) {
    (void)build_system;
    return false; // Stub
}

char** wyn_build_get_build_order(WynBuildSystem* build_system, size_t* count) {
    (void)build_system;
    if (count) *count = 0;
    return NULL; // Stub
}

bool wyn_build_system_build_parallel(WynBuildSystem* build_system, int max_jobs) {
    (void)build_system; (void)max_jobs;
    return false; // Stub
}

bool wyn_build_file_newer(const char* file1, const char* file2) {
    (void)file1; (void)file2;
    return false; // Stub
}

uint64_t wyn_build_file_checksum(const char* file_path) {
    (void)file_path;
    return 0; // Stub
}

bool wyn_build_copy_file(const char* src, const char* dst) {
    (void)src; (void)dst;
    return false; // Stub
}

const char* wyn_build_get_platform_name(WynBuildPlatform platform) {
    (void)platform;
    return "native"; // Stub
}

const char* wyn_build_get_executable_extension(WynBuildPlatform platform) {
    (void)platform;
    return ""; // Stub
}

const char* wyn_build_get_library_extension(WynBuildPlatform platform, bool shared) {
    (void)platform; (void)shared;
    return ".so"; // Stub
}

char** wyn_build_get_platform_flags(WynBuildPlatform platform, size_t* count) {
    (void)platform;
    if (count) *count = 0;
    return NULL; // Stub
}

bool wyn_build_integrate_packages(WynBuildSystem* build_system) {
    (void)build_system;
    return false; // Stub
}

bool wyn_build_download_dependencies(WynBuildSystem* build_system) {
    (void)build_system;
    return false; // Stub
}
