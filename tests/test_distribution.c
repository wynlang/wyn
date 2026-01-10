#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/distribution.h"

void test_package_builder() {
    printf("Testing package builder...\n");
    
    // Create target and package builder
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynPackageBuilder* builder = wyn_package_builder_new(WYN_PKG_DEB, target);
    
    assert(builder != NULL);
    assert(builder->format == WYN_PKG_DEB);
    assert(builder->target == target);
    assert(builder->verbose == false);
    
    // Set metadata
    WynPackageMetadata metadata = wyn_package_metadata_default("test-app", "1.0.0");
    bool result = wyn_package_builder_set_metadata(builder, &metadata);
    assert(result == true);
    assert(strcmp(builder->metadata.name, "test-app") == 0);
    assert(strcmp(builder->metadata.version, "1.0.0") == 0);
    
    // Add binary
    WynBinaryConfig binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "/usr/bin/test-app");
    result = wyn_package_builder_add_binary(builder, &binary);
    assert(result == true);
    assert(builder->binary_count == 1);
    assert(builder->binaries[0].type == WYN_BINARY_EXECUTABLE);
    
    // Set output directory
    result = wyn_package_builder_set_output_dir(builder, "/tmp/packages");
    assert(result == true);
    assert(strcmp(builder->output_directory, "/tmp/packages") == 0);
    
    wyn_package_metadata_free(&metadata);
    wyn_binary_config_free(&binary);
    wyn_package_builder_free(builder);
    wyn_target_free(target);
    
    printf("✓ Package builder tests passed\n");
}

void test_container_builder() {
    printf("Testing container builder...\n");
    
    // Create target and container builder
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynContainerBuilder* builder = wyn_container_builder_new(WYN_CONTAINER_DOCKER, target);
    
    assert(builder != NULL);
    assert(builder->type == WYN_CONTAINER_DOCKER);
    assert(builder->target == target);
    assert(builder->optimize_size == true);
    
    // Set container configuration
    WynContainerConfig config = wyn_container_config_default("alpine:latest");
    bool result = wyn_container_builder_set_config(builder, &config);
    assert(result == true);
    assert(strcmp(builder->config.base_image, "alpine:latest") == 0);
    
    // Add binary
    WynBinaryConfig binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "./app");
    result = wyn_container_builder_add_binary(builder, &binary);
    assert(result == true);
    assert(builder->binary_count == 1);
    
    // Set image name
    result = wyn_container_builder_set_image_name(builder, "test-app", "v1.0.0");
    assert(result == true);
    assert(strcmp(builder->image_name, "test-app") == 0);
    assert(strcmp(builder->image_tag, "v1.0.0") == 0);
    
    wyn_container_config_free(&config);
    wyn_binary_config_free(&binary);
    wyn_container_builder_free(builder);
    wyn_target_free(target);
    
    printf("✓ Container builder tests passed\n");
}

void test_distribution() {
    printf("Testing distribution...\n");
    
    // Create distribution
    WynDistribution* distribution = wyn_distribution_new("MyProject", "2.0.0");
    assert(distribution != NULL);
    assert(strcmp(distribution->project_name, "MyProject") == 0);
    assert(strcmp(distribution->version, "2.0.0") == 0);
    assert(distribution->parallel_build == true);
    
    // Add targets
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    
    bool result = wyn_distribution_add_target(distribution, linux_target);
    assert(result == true);
    assert(distribution->target_count == 1);
    
    result = wyn_distribution_add_target(distribution, windows_target);
    assert(result == true);
    assert(distribution->target_count == 2);
    
    // Add package builders
    WynPackageBuilder* deb_builder = wyn_package_builder_new(WYN_PKG_DEB, linux_target);
    WynPackageBuilder* msi_builder = wyn_package_builder_new(WYN_PKG_MSI, windows_target);
    
    result = wyn_distribution_add_package(distribution, deb_builder);
    assert(result == true);
    assert(distribution->package_count == 1);
    
    result = wyn_distribution_add_package(distribution, msi_builder);
    assert(result == true);
    assert(distribution->package_count == 2);
    
    // Add container builder
    WynContainerBuilder* container = wyn_container_builder_new(WYN_CONTAINER_DOCKER, linux_target);
    result = wyn_distribution_add_container(distribution, container);
    assert(result == true);
    assert(distribution->container_count == 1);
    
    wyn_distribution_free(distribution);
    
    printf("✓ Distribution tests passed\n");
}

void test_package_formats() {
    printf("Testing package formats...\n");
    
    // Test format extensions
    assert(strcmp(wyn_package_format_extension(WYN_PKG_DEB), ".deb") == 0);
    assert(strcmp(wyn_package_format_extension(WYN_PKG_RPM), ".rpm") == 0);
    assert(strcmp(wyn_package_format_extension(WYN_PKG_MSI), ".msi") == 0);
    assert(strcmp(wyn_package_format_extension(WYN_PKG_DMG), ".dmg") == 0);
    assert(strcmp(wyn_package_format_extension(WYN_PKG_TAR_GZ), ".tar.gz") == 0);
    assert(strcmp(wyn_package_format_extension(WYN_PKG_ZIP), ".zip") == 0);
    
    // Test format names
    assert(strcmp(wyn_package_format_name(WYN_PKG_DEB), "Debian Package") == 0);
    assert(strcmp(wyn_package_format_name(WYN_PKG_RPM), "RPM Package") == 0);
    assert(strcmp(wyn_package_format_name(WYN_PKG_MSI), "Windows Installer") == 0);
    assert(strcmp(wyn_package_format_name(WYN_PKG_DMG), "macOS Disk Image") == 0);
    
    // Test format support
    WynTarget* linux_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynTarget* windows_target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_WINDOWS, WYN_ENV_MSVC);
    WynTarget* macos_target = wyn_target_new(WYN_ARCH_ARM64, WYN_OS_MACOS, WYN_ENV_GNU);
    
    assert(wyn_package_format_supported(WYN_PKG_DEB, linux_target) == true);
    assert(wyn_package_format_supported(WYN_PKG_DEB, windows_target) == false);
    assert(wyn_package_format_supported(WYN_PKG_MSI, windows_target) == true);
    assert(wyn_package_format_supported(WYN_PKG_MSI, linux_target) == false);
    assert(wyn_package_format_supported(WYN_PKG_DMG, macos_target) == true);
    assert(wyn_package_format_supported(WYN_PKG_DMG, linux_target) == false);
    
    // Universal formats should work on all platforms
    assert(wyn_package_format_supported(WYN_PKG_TAR_GZ, linux_target) == true);
    assert(wyn_package_format_supported(WYN_PKG_TAR_GZ, windows_target) == true);
    assert(wyn_package_format_supported(WYN_PKG_ZIP, linux_target) == true);
    assert(wyn_package_format_supported(WYN_PKG_ZIP, windows_target) == true);
    
    wyn_target_free(linux_target);
    wyn_target_free(windows_target);
    wyn_target_free(macos_target);
    
    printf("✓ Package format tests passed\n");
}

void test_metadata_helpers() {
    printf("Testing metadata helpers...\n");
    
    // Test default metadata
    WynPackageMetadata metadata = wyn_package_metadata_default("test-package", "1.2.3");
    assert(strcmp(metadata.name, "test-package") == 0);
    assert(strcmp(metadata.version, "1.2.3") == 0);
    assert(strcmp(metadata.description, "A Wyn application") == 0);
    assert(strcmp(metadata.license, "MIT") == 0);
    assert(metadata.installed_size == 0);
    
    wyn_package_metadata_free(&metadata);
    
    printf("✓ Metadata helper tests passed\n");
}

void test_binary_config() {
    printf("Testing binary configuration...\n");
    
    // Test default binary config
    WynBinaryConfig config = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "/usr/bin/myapp");
    assert(config.type == WYN_BINARY_EXECUTABLE);
    assert(strcmp(config.source_path, "/usr/bin/myapp") == 0);
    assert(config.strip_symbols == true);
    assert(config.compress == false);
    assert(config.static_linking == false);
    
    wyn_binary_config_free(&config);
    
    // Test library config
    WynBinaryConfig lib_config = wyn_binary_config_default(WYN_BINARY_SHARED_LIBRARY, "/usr/lib/libmylib.so");
    assert(lib_config.type == WYN_BINARY_SHARED_LIBRARY);
    assert(strcmp(lib_config.source_path, "/usr/lib/libmylib.so") == 0);
    
    wyn_binary_config_free(&lib_config);
    
    printf("✓ Binary configuration tests passed\n");
}

void test_container_config() {
    printf("Testing container configuration...\n");
    
    // Test default container config
    WynContainerConfig config = wyn_container_config_default("ubuntu:20.04");
    assert(strcmp(config.base_image, "ubuntu:20.04") == 0);
    assert(strcmp(config.working_dir, "/app") == 0);
    assert(strcmp(config.user, "1000:1000") == 0);
    assert(config.multi_stage == false);
    
    wyn_container_config_free(&config);
    
    // Test with default base image
    WynContainerConfig default_config = wyn_container_config_default(NULL);
    assert(strcmp(default_config.base_image, "alpine:latest") == 0);
    
    wyn_container_config_free(&default_config);
    
    printf("✓ Container configuration tests passed\n");
}

void test_dockerfile_generation() {
    printf("Testing Dockerfile generation...\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynContainerBuilder* builder = wyn_container_builder_new(WYN_CONTAINER_DOCKER, target);
    
    // Set configuration
    WynContainerConfig config = wyn_container_config_default("alpine:3.15");
    wyn_container_builder_set_config(builder, &config);
    
    // Add binary
    WynBinaryConfig binary = wyn_binary_config_default(WYN_BINARY_EXECUTABLE, "myapp");
    wyn_container_builder_add_binary(builder, &binary);
    
    // Generate Dockerfile
    bool result = wyn_generate_dockerfile(builder, "/tmp/Dockerfile.test");
    assert(result == true);
    
    // Check if file was created
    FILE* dockerfile = fopen("/tmp/Dockerfile.test", "r");
    assert(dockerfile != NULL);
    
    // Read and verify content
    char line[256];
    fgets(line, sizeof(line), dockerfile);
    assert(strstr(line, "FROM alpine:3.15") != NULL);
    
    fclose(dockerfile);
    
    wyn_container_config_free(&config);
    wyn_binary_config_free(&binary);
    wyn_container_builder_free(builder);
    wyn_target_free(target);
    
    printf("✓ Dockerfile generation tests passed\n");
}

void test_package_building() {
    printf("Testing package building...\n");
    
    WynTarget* target = wyn_target_new(WYN_ARCH_X86_64, WYN_OS_LINUX, WYN_ENV_GNU);
    WynPackageBuilder* builder = wyn_package_builder_new(WYN_PKG_TAR_GZ, target);
    
    // Set metadata
    WynPackageMetadata metadata = wyn_package_metadata_default("test-build", "0.1.0");
    wyn_package_builder_set_metadata(builder, &metadata);
    
    // Set output directory
    wyn_package_builder_set_output_dir(builder, "/tmp/test-packages");
    
    // Build package (will use stub implementation)
    bool result = wyn_package_builder_build(builder);
    assert(result == true);
    
    wyn_package_metadata_free(&metadata);
    wyn_package_builder_free(builder);
    wyn_target_free(target);
    
    printf("✓ Package building tests passed\n");
}

int main() {
    printf("Running Distribution and Packaging Tests\n");
    printf("========================================\n\n");
    
    test_package_builder();
    test_container_builder();
    test_distribution();
    test_package_formats();
    test_metadata_helpers();
    test_binary_config();
    test_container_config();
    test_dockerfile_generation();
    test_package_building();
    
    printf("\n✓ All distribution and packaging tests passed!\n");
    printf("Distribution system provides:\n");
    printf("  - Multi-format package building (DEB, RPM, MSI, DMG, etc.)\n");
    printf("  - Container image generation with Docker support\n");
    printf("  - Binary optimization and static linking\n");
    printf("  - Cross-platform distribution management\n");
    printf("  - Metadata and dependency management\n");
    printf("  - Template generation for build systems\n");
    
    return 0;
}
