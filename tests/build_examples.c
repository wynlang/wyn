#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/build.h"

void example_basic_build_system() {
    printf("=== Basic Build System Example ===\n");
    
    // Create build system for a project
    WynBuildSystem* build_system = wyn_build_system_new("/workspace/my-project");
    build_system->verbose = true;
    
    printf("Build system initialized for: %s\n", build_system->project_root);
    printf("Build directory: %s\n", build_system->build_dir);
    printf("Cache directory: %s\n", build_system->default_config.cache_dir);
    
    // Show default configuration
    printf("\nDefault configuration:\n");
    printf("  - Optimization: %s\n", 
           build_system->default_config.opt_level == WYN_BUILD_DEBUG ? "Debug" : "Release");
    printf("  - Platform: %s\n", 
           build_system->default_config.target_platform == WYN_BUILD_NATIVE ? "Native" : "Cross");
    printf("  - Debug info: %s\n", 
           build_system->default_config.debug_info ? "enabled" : "disabled");
    printf("  - Parallel build: %s\n", 
           build_system->default_config.parallel_build ? "enabled" : "disabled");
    printf("  - Max jobs: %d\n", build_system->default_config.max_parallel_jobs);
    
    wyn_build_system_free(build_system);
    printf("\n");
}

void example_executable_target() {
    printf("=== Executable Target Example ===\n");
    
    WynBuildSystem* build_system = wyn_build_system_new("/workspace/cli-app");
    
    // Create executable target
    WynBuildTarget* app_target = wyn_build_target_new("cli-app", WYN_BUILD_EXECUTABLE);
    
    // Add source files
    wyn_build_target_add_source(app_target, "src/main.c");
    wyn_build_target_add_source(app_target, "src/commands.c");
    wyn_build_target_add_source(app_target, "src/utils.c");
    
    // Add dependencies
    wyn_build_target_add_dependency(app_target, "libcurl");
    wyn_build_target_add_dependency(app_target, "libjson");
    
    // Configure for release build
    app_target->config.opt_level = WYN_BUILD_RELEASE;
    app_target->config.debug_info = false;
    
    // Add to build system
    wyn_build_system_add_target(build_system, app_target);
    
    printf("Created executable target: %s\n", app_target->name);
    printf("Source files: %zu\n", app_target->source_file_count);
    for (size_t i = 0; i < app_target->source_file_count; i++) {
        printf("  - %s\n", app_target->source_files[i]);
    }
    printf("Dependencies: %zu\n", app_target->dependency_count);
    for (size_t i = 0; i < app_target->dependency_count; i++) {
        printf("  - %s\n", app_target->dependencies[i]);
    }
    
    wyn_build_system_free(build_system);
    printf("\n");
}

void example_library_targets() {
    printf("=== Library Target Example ===\n");
    
    WynBuildSystem* build_system = wyn_build_system_new("/workspace/math-lib");
    
    // Create static library target
    WynBuildTarget* static_lib = wyn_build_target_new("libmath", WYN_BUILD_STATIC_LIBRARY);
    wyn_build_target_add_source(static_lib, "src/algebra.c");
    wyn_build_target_add_source(static_lib, "src/geometry.c");
    wyn_build_target_add_source(static_lib, "src/statistics.c");
    
    // Create shared library target
    WynBuildTarget* shared_lib = wyn_build_target_new("libmath", WYN_BUILD_SHARED_LIBRARY);
    wyn_build_target_add_source(shared_lib, "src/algebra.c");
    wyn_build_target_add_source(shared_lib, "src/geometry.c");
    wyn_build_target_add_source(shared_lib, "src/statistics.c");
    
    // Add both targets
    wyn_build_system_add_target(build_system, static_lib);
    wyn_build_system_add_target(build_system, shared_lib);
    
    printf("Created library targets:\n");
    printf("  - Static library: %s (%zu sources)\n", 
           static_lib->name, static_lib->source_file_count);
    printf("  - Shared library: %s (%zu sources)\n", 
           shared_lib->name, shared_lib->source_file_count);
    
    // Show output paths
    char* static_output = wyn_build_get_output_path(static_lib, build_system->build_dir);
    char* shared_output = wyn_build_get_output_path(shared_lib, build_system->build_dir);
    
    printf("Output paths:\n");
    printf("  - Static: %s\n", static_output);
    printf("  - Shared: %s\n", shared_output);
    
    free(static_output);
    free(shared_output);
    wyn_build_system_free(build_system);
    printf("\n");
}

void example_build_configuration() {
    printf("=== Build Configuration Example ===\n");
    
    WynBuildSystem* build_system = wyn_build_system_new("/workspace/complex-app");
    
    // Create target with custom configuration
    WynBuildTarget* target = wyn_build_target_new("complex-app", WYN_BUILD_EXECUTABLE);
    wyn_build_target_add_source(target, "src/main.c");
    
    // Configure build settings
    target->config.opt_level = WYN_BUILD_RELEASE_WITH_DEBUG;
    target->config.target_platform = WYN_BUILD_LINUX_X64;
    target->config.link_time_optimization = true;
    target->config.max_parallel_jobs = 8;
    
    // Add include paths
    wyn_build_config_add_include_path(&target->config, "/usr/include/gtk-3.0");
    wyn_build_config_add_include_path(&target->config, "/usr/include/glib-2.0");
    
    // Add libraries
    wyn_build_config_add_library(&target->config, "gtk-3");
    wyn_build_config_add_library(&target->config, "glib-2.0");
    wyn_build_config_add_library(&target->config, "pthread");
    
    wyn_build_system_add_target(build_system, target);
    
    printf("Target configuration:\n");
    printf("  - Optimization: Release with debug info\n");
    printf("  - Platform: Linux x64\n");
    printf("  - LTO: %s\n", target->config.link_time_optimization ? "enabled" : "disabled");
    printf("  - Max jobs: %d\n", target->config.max_parallel_jobs);
    printf("  - Include paths: %zu\n", target->config.include_path_count);
    printf("  - Libraries: %zu\n", target->config.library_count);
    
    wyn_build_system_free(build_system);
    printf("\n");
}

void example_build_cache() {
    printf("=== Build Cache Example ===\n");
    
    WynBuildSystem* build_system = wyn_build_system_new("/workspace/cached-build");
    
    printf("Build cache location: %s\n", build_system->cache->cache_file_path);
    printf("Cache capacity: %zu entries\n", build_system->cache->entry_capacity);
    printf("Current entries: %zu\n", build_system->cache->entry_count);
    
    // Simulate cache operations
    printf("\nCache operations:\n");
    
    // Check if files need rebuild (all will since they don't exist)
    const char* test_files[] = {"src/main.c", "src/utils.c", "src/config.c"};
    for (size_t i = 0; i < 3; i++) {
        bool needs_rebuild = wyn_build_cache_needs_rebuild(build_system->cache, test_files[i]);
        printf("  - %s: %s\n", test_files[i], needs_rebuild ? "needs rebuild" : "up to date");
    }
    
    wyn_build_system_free(build_system);
    printf("\n");
}

void example_makefile_generation() {
    printf("=== Makefile Generation Example ===\n");
    
    WynBuildSystem* build_system = wyn_build_system_new("/workspace/makefile-project");
    
    // Create multiple targets
    WynBuildTarget* app = wyn_build_target_new("myapp", WYN_BUILD_EXECUTABLE);
    wyn_build_target_add_source(app, "src/main.c");
    wyn_build_target_add_source(app, "src/app.c");
    
    WynBuildTarget* lib = wyn_build_target_new("libutils", WYN_BUILD_STATIC_LIBRARY);
    wyn_build_target_add_source(lib, "src/utils.c");
    wyn_build_target_add_source(lib, "src/helpers.c");
    
    wyn_build_system_add_target(build_system, app);
    wyn_build_system_add_target(build_system, lib);
    
    // Generate Makefile
    const char* makefile_path = "/tmp/generated_makefile";
    bool result = wyn_build_generate_makefile(build_system, makefile_path);
    
    printf("Makefile generation: %s\n", result ? "success" : "failed");
    printf("Generated Makefile: %s\n", makefile_path);
    
    if (result) {
        printf("\nMakefile content preview:\n");
        FILE* makefile = fopen(makefile_path, "r");
        if (makefile) {
            char line[256];
            int line_count = 0;
            while (fgets(line, sizeof(line), makefile) && line_count < 10) {
                printf("  %s", line);
                line_count++;
            }
            if (line_count == 10) {
                printf("  ... (truncated)\n");
            }
            fclose(makefile);
        }
    }
    
    wyn_build_system_free(build_system);
    printf("\n");
}

void example_utility_functions() {
    printf("=== Utility Functions Example ===\n");
    
    // Test object path generation
    printf("Object path generation:\n");
    char* obj1 = wyn_build_get_object_path("src/main.c", "/build");
    char* obj2 = wyn_build_get_object_path("lib/utils.c", "/tmp/build");
    char* obj3 = wyn_build_get_object_path("test.c", "/output");
    
    printf("  - src/main.c -> %s\n", obj1);
    printf("  - lib/utils.c -> %s\n", obj2);
    printf("  - test.c -> %s\n", obj3);
    
    free(obj1);
    free(obj2);
    free(obj3);
    
    // Test output path generation
    printf("\nOutput path generation:\n");
    WynBuildTarget* exe = wyn_build_target_new("myapp", WYN_BUILD_EXECUTABLE);
    WynBuildTarget* shared = wyn_build_target_new("libshared", WYN_BUILD_SHARED_LIBRARY);
    WynBuildTarget* static_lib = wyn_build_target_new("libstatic", WYN_BUILD_STATIC_LIBRARY);
    
    char* exe_path = wyn_build_get_output_path(exe, "/build");
    char* shared_path = wyn_build_get_output_path(shared, "/build");
    char* static_path = wyn_build_get_output_path(static_lib, "/build");
    
    printf("  - Executable: %s\n", exe_path);
    printf("  - Shared library: %s\n", shared_path);
    printf("  - Static library: %s\n", static_path);
    
    free(exe_path);
    free(shared_path);
    free(static_path);
    wyn_build_target_free(exe);
    wyn_build_target_free(shared);
    wyn_build_target_free(static_lib);
    
    printf("\n");
}

int main() {
    printf("Wyn Build System Examples\n");
    printf("=========================\n\n");
    
    example_basic_build_system();
    example_executable_target();
    example_library_targets();
    example_build_configuration();
    example_build_cache();
    example_makefile_generation();
    example_utility_functions();
    
    printf("Build System Features:\n");
    printf("  ✓ Multi-target build management\n");
    printf("  ✓ Executable and library targets\n");
    printf("  ✓ Configurable optimization levels\n");
    printf("  ✓ Incremental compilation with caching\n");
    printf("  ✓ Dependency tracking and resolution\n");
    printf("  ✓ Makefile generation\n");
    printf("  ✓ Cross-platform build support\n");
    printf("  ✓ Parallel build capabilities\n");
    printf("\nReady for integration with package manager and LSP!\n");
    
    return 0;
}
