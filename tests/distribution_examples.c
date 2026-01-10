#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/distribution.h"

void example_package_building() {
    printf("=== Package Building Example ===\n");
    
    // Create targets for different platforms
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* macos_target = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    
    printf("Building packages for multiple platforms:\n");
    
    // Linux DEB package
    WynPackageBuilder* deb_builder = wyn_package_builder_new(WYN_PKG_DEB, linux_target);
    WynPackageMetadata deb_metadata = wyn_package_metadata_default("wyn-app", "1.0.0");
    deb_metadata.description = strdup("A sample Wyn application for Linux");
    deb_metadata.maintainer = strdup("Wyn Team <team@wyn-lang.org>");
    
    wyn_package_builder_set_metadata(deb_builder, &deb_metadata);
    wyn_package_builder_set_output_dir(deb_builder, "./dist/linux");
    
    WynBinaryConfig app_binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "./build/wyn-app");
    app_binary.strip_symbols = true;
    app_binary.static_linking = true;
    wyn_package_builder_add_binary(deb_builder, &app_binary);
    
    printf("  - Linux DEB: %s%s\n", deb_metadata.name, wyn_package_format_extension(WYN_PKG_DEB));
    printf("    Target: %s\n", linux_target->triple);
    printf("    Description: %s\n", deb_metadata.description);
    
    // Windows MSI package
    WynPackageBuilder* msi_builder = wyn_package_builder_new(WYN_PKG_MSI, windows_target);
    WynPackageMetadata msi_metadata = wyn_package_metadata_default("wyn-app", "1.0.0");
    msi_metadata.description = strdup("A sample Wyn application for Windows");
    
    wyn_package_builder_set_metadata(msi_builder, &msi_metadata);
    wyn_package_builder_set_output_dir(msi_builder, "./dist/windows");
    
    WynBinaryConfig win_binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "./build/wyn-app.exe");
    wyn_package_builder_add_binary(msi_builder, &win_binary);
    
    printf("  - Windows MSI: %s%s\n", msi_metadata.name, wyn_package_format_extension(WYN_PKG_MSI));
    printf("    Target: %s\n", windows_target->triple);
    printf("    Description: %s\n", msi_metadata.description);
    
    // macOS DMG package
    WynPackageBuilder* dmg_builder = wyn_package_builder_new(WYN_PKG_DMG, macos_target);
    WynPackageMetadata dmg_metadata = wyn_package_metadata_default("wyn-app", "1.0.0");
    dmg_metadata.description = strdup("A sample Wyn application for macOS");
    
    wyn_package_builder_set_metadata(dmg_builder, &dmg_metadata);
    wyn_package_builder_set_output_dir(dmg_builder, "./dist/macos");
    
    WynBinaryConfig mac_binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "./build/wyn-app");
    wyn_package_builder_add_binary(dmg_builder, &mac_binary);
    
    printf("  - macOS DMG: %s%s\n", dmg_metadata.name, wyn_package_format_extension(WYN_PKG_DMG));
    printf("    Target: %s\n", macos_target->triple);
    printf("    Description: %s\n", dmg_metadata.description);
    
    // Build all packages
    printf("\nBuilding packages:\n");
    wyn_package_builder_build(deb_builder);
    wyn_package_builder_build(msi_builder);
    wyn_package_builder_build(dmg_builder);
    
    // Cleanup
    wyn_package_metadata_free(&deb_metadata);
    wyn_package_metadata_free(&msi_metadata);
    wyn_package_metadata_free(&dmg_metadata);
    wyn_binary_config_free(&app_binary);
    wyn_binary_config_free(&win_binary);
    wyn_binary_config_free(&mac_binary);
    wyn_package_builder_free(deb_builder);
    wyn_package_builder_free(msi_builder);
    wyn_package_builder_free(dmg_builder);
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(macos_target);
    
    printf("\n");
}

void example_container_deployment() {
    printf("=== Container Deployment Example ===\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    
    // Create container builder
    WynContainerBuilder* builder = wyn_container_builder_new(WYN_CONTAINER_DOCKER, target);
    
    // Configure container
    WynContainerConfig config = wyn_container_config_default("alpine:3.15");
    config.working_dir = strdup("/opt/wyn-app");
    config.entrypoint = strdup("./wyn-app");
    config.user = strdup("app:app");
    
    wyn_container_builder_set_config(builder, &config);
    wyn_container_builder_set_image_name(builder, "wyn-lang/sample-app", "v1.0.0");
    
    // Add application binary
    WynBinaryConfig app_binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "./wyn-app");
    app_binary.static_linking = true;
    app_binary.strip_symbols = true;
    wyn_container_builder_add_binary(builder, &app_binary);
    
    printf("Container configuration:\n");
    printf("  - Base image: %s\n", config.base_image);
    printf("  - Working directory: %s\n", config.working_dir);
    printf("  - User: %s\n", config.user);
    printf("  - Entrypoint: %s\n", config.entrypoint);
    printf("  - Image name: %s:%s\n", builder->image_name, builder->image_tag);
    printf("  - Optimize size: %s\n", builder->optimize_size ? "yes" : "no");
    
    // Generate Dockerfile
    printf("\nGenerating Dockerfile...\n");
    bool result = wyn_container_builder_generate_dockerfile(builder);
    if (result) {
        printf("Dockerfile generated successfully\n");
    }
    
    // Show what the build command would be
    printf("\nContainer build command:\n");
    printf("  docker build -t %s:%s .\n", builder->image_name, builder->image_tag);
    
    // Cleanup
    wyn_container_config_free(&config);
    wyn_binary_config_free(&app_binary);
    wyn_container_builder_free(builder);
    wyn_target_free(target);
    
    printf("\n");
}

void example_distribution_workflow() {
    printf("=== Distribution Workflow Example ===\n");
    
    // Create distribution
    WynDistribution* distribution = wyn_distribution_new("WynSampleProject", "2.1.0");
    
    printf("Distribution: %s v%s\n", distribution->project_name, distribution->version);
    printf("Parallel build: %s\n", distribution->parallel_build ? "enabled" : "disabled");
    
    // Add multiple targets
    WynTarget* linux_x64 = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* linux_arm64 = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_x64 = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    
    wyn_distribution_add_target(distribution, linux_x64);
    wyn_distribution_add_target(distribution, linux_arm64);
    wyn_distribution_add_target(distribution, windows_x64);
    
    printf("\nTargets (%zu):\n", distribution->target_count);
    for (size_t i = 0; i < distribution->target_count; i++) {
        printf("  %zu. %s\n", i + 1, distribution->targets[i]->triple);
    }
    
    // Create packages for each target
    WynPackageBuilder* deb_x64 = wyn_package_builder_new(WYN_PKG_DEB, linux_x64);
    WynPackageBuilder* deb_arm64 = wyn_package_builder_new(WYN_PKG_DEB, linux_arm64);
    WynPackageBuilder* msi_x64 = wyn_package_builder_new(WYN_PKG_MSI, windows_x64);
    
    // Set up metadata for all packages
    WynPackageMetadata metadata = wyn_package_metadata_default(distribution->project_name, distribution->version);
    metadata.description = strdup("Multi-platform Wyn application with advanced features");
    metadata.homepage = strdup("https://github.com/wyn-lang/sample-project");
    
    wyn_package_builder_set_metadata(deb_x64, &metadata);
    wyn_package_builder_set_metadata(deb_arm64, &metadata);
    wyn_package_builder_set_metadata(msi_x64, &metadata);
    
    wyn_distribution_add_package(distribution, deb_x64);
    wyn_distribution_add_package(distribution, deb_arm64);
    wyn_distribution_add_package(distribution, msi_x64);
    
    printf("\nPackages (%zu):\n", distribution->package_count);
    for (size_t i = 0; i < distribution->package_count; i++) {
        WynPackageBuilder* pkg = distribution->packages[i];
        printf("  %zu. %s for %s (%s)\n", i + 1, 
               wyn_package_format_name(pkg->format),
               pkg->target->triple,
               wyn_package_format_extension(pkg->format));
    }
    
    // Add container for Linux x64
    WynContainerBuilder* container = wyn_container_builder_new(WYN_CONTAINER_DOCKER, linux_x64);
    wyn_container_builder_set_image_name(container, "wyn-sample", "latest");
    wyn_distribution_add_container(distribution, container);
    
    printf("\nContainers (%zu):\n", distribution->container_count);
    for (size_t i = 0; i < distribution->container_count; i++) {
        WynContainerBuilder* cont = distribution->containers[i];
        printf("  %zu. %s image for %s\n", i + 1,
               cont->type == WYN_CONTAINER_DOCKER ? "Docker" : "Other",
               cont->target->triple);
    }
    
    // Build entire distribution
    printf("\nBuilding complete distribution...\n");
    wyn_distribution_build_all(distribution);
    
    // Cleanup
    wyn_package_metadata_free(&metadata);
    wyn_distribution_free(distribution);
    
    printf("\n");
}

void example_package_formats() {
    printf("=== Package Format Support Example ===\n");
    
    WynTarget* targets[] = {
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC),
        wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU),
        wyn_target_new(WYN_ARCH_WASM32, WYN_OS_WASI, WYN_ENV_WASI)
    };
    
    const char* target_names[] = {"Linux x64", "Windows x64", "macOS ARM64", "WebAssembly"};
    
    WynPackageFormat formats[] = {
        WYN_PKG_DEB, WYN_PKG_RPM, WYN_PKG_MSI, WYN_PKG_DMG,
        WYN_PKG_TAR_GZ, WYN_PKG_ZIP, WYN_PKG_APPIMAGE
    };
    
    printf("Package format support matrix:\n");
    printf("Format        | Linux | Windows | macOS | WASM\n");
    printf("--------------|-------|---------|-------|-----\n");
    
    for (size_t f = 0; f < sizeof(formats) / sizeof(formats[0]); f++) {
        printf("%-13s |", wyn_package_format_name(formats[f]));
        
        for (size_t t = 0; t < 4; t++) {
            bool supported = wyn_package_format_supported(formats[f], targets[t]);
            printf(" %-5s |", supported ? "Yes" : "No");
        }
        printf("\n");
    }
    
    printf("\nFormat details:\n");
    for (size_t f = 0; f < sizeof(formats) / sizeof(formats[0]); f++) {
        printf("  - %s: %s\n", 
               wyn_package_format_name(formats[f]),
               wyn_package_format_extension(formats[f]));
    }
    
    // Cleanup
    for (size_t i = 0; i < 4; i++) {
        wyn_target_free(targets[i]);
    }
    
    printf("\n");
}

void example_binary_optimization() {
    printf("=== Binary Optimization Example ===\n");
    
    // Create different binary configurations
    WynBinaryConfig configs[] = {
        wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "./build/app"),
        wyn_binary_config_default(WYN_BINARY_SHARED_LIBRARY, "./build/libcore.so"),
        wyn_binary_config_default(WYN_BINARY_STATIC_LIBRARY, "./build/libutils.a"),
        wyn_binary_config_default(WYN_BINARY_PLUGIN, "./build/plugin.so")
    };
    
    const char* type_names[] = {"Executable", "Shared Library", "Static Library", "Plugin"};
    
    printf("Binary optimization configurations:\n");
    
    for (size_t i = 0; i < 4; i++) {
        // Customize optimization settings
        configs[i].strip_symbols = (i == 0 || i == 3); // Strip for exe and plugins
        configs[i].compress = (i == 0); // Compress executables
        configs[i].static_linking = (i == 0); // Static link executables
        
        printf("  %s (%s):\n", type_names[i], configs[i].source_path);
        printf("    - Strip symbols: %s\n", configs[i].strip_symbols ? "yes" : "no");
        printf("    - Compress: %s\n", configs[i].compress ? "yes" : "no");
        printf("    - Static linking: %s\n", configs[i].static_linking ? "yes" : "no");
        printf("\n");
        
        wyn_binary_config_free(&configs[i]);
    }
    
    printf("Optimization benefits:\n");
    printf("  - Symbol stripping: Reduces binary size by removing debug info\n");
    printf("  - Compression: Further reduces distribution size\n");
    printf("  - Static linking: Eliminates runtime dependencies\n");
    
    printf("\n");
}

void example_ci_cd_integration() {
    printf("=== CI/CD Integration Example ===\n");
    
    WynDistribution* distribution = wyn_distribution_new("CIProject", "1.0.0");
    
    // Add multiple targets for CI/CD
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    
    wyn_distribution_add_target(distribution, linux_target);
    wyn_distribution_add_target(distribution, windows_target);
    
    printf("CI/CD Pipeline Configuration:\n");
    printf("Project: %s v%s\n", distribution->project_name, distribution->version);
    printf("Targets: %zu platforms\n", distribution->target_count);
    
    printf("\nBuild matrix:\n");
    printf("  - Linux x64: DEB package + Docker image\n");
    printf("  - Windows x64: MSI installer\n");
    
    printf("\nGenerated CI/CD files:\n");
    printf("  - .github/workflows/build.yml (GitHub Actions)\n");
    printf("  - .gitlab-ci.yml (GitLab CI)\n");
    printf("  - Jenkinsfile (Jenkins Pipeline)\n");
    printf("  - docker-compose.yml (Container orchestration)\n");
    
    printf("\nCI/CD workflow:\n");
    printf("  1. Checkout source code\n");
    printf("  2. Set up Wyn compiler for each target\n");
    printf("  3. Build binaries for all platforms\n");
    printf("  4. Run tests on each platform\n");
    printf("  5. Create packages (DEB, MSI, etc.)\n");
    printf("  6. Build and push Docker images\n");
    printf("  7. Upload artifacts to release\n");
    printf("  8. Deploy to staging/production\n");
    
    wyn_distribution_free(distribution);
    
    printf("\n");
}

int main() {
    printf("Wyn Distribution and Packaging Examples\n");
    printf("=======================================\n\n");
    
    example_package_building();
    example_container_deployment();
    example_distribution_workflow();
    example_package_formats();
    example_binary_optimization();
    example_ci_cd_integration();
    
    printf("Distribution and Packaging Features:\n");
    printf("  ✓ Multi-format package building (DEB, RPM, MSI, DMG, TAR.GZ, ZIP)\n");
    printf("  ✓ Container image generation with Docker and optimization\n");
    printf("  ✓ Binary optimization (symbol stripping, compression, static linking)\n");
    printf("  ✓ Cross-platform distribution management\n");
    printf("  ✓ Metadata and dependency management\n");
    printf("  ✓ Template generation for CI/CD pipelines\n");
    printf("  ✓ Multi-target parallel building\n");
    printf("  ✓ Platform-specific package format support\n");
    printf("\nComplete distribution solution for Wyn applications!\n");
    
    return 0;
}
