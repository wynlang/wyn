#ifndef WYN_DISTRIBUTION_H
#define WYN_DISTRIBUTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "crossplatform.h"

// Forward declarations
typedef struct WynPackageBuilder WynPackageBuilder;
typedef struct WynContainerBuilder WynContainerBuilder;
typedef struct WynDistribution WynDistribution;

// Package formats
typedef enum {
    WYN_PKG_DEB,        // Debian package (.deb)
    WYN_PKG_RPM,        // Red Hat package (.rpm)
    WYN_PKG_MSI,        // Windows installer (.msi)
    WYN_PKG_DMG,        // macOS disk image (.dmg)
    WYN_PKG_TAR_GZ,     // Compressed tarball (.tar.gz)
    WYN_PKG_ZIP,        // ZIP archive (.zip)
    WYN_PKG_APPIMAGE,   // Linux AppImage
    WYN_PKG_SNAP,       // Ubuntu Snap package
    WYN_PKG_FLATPAK     // Flatpak package
} WynPackageFormat;

// Container types
typedef enum {
    WYN_CONTAINER_DOCKER,
    WYN_CONTAINER_PODMAN,
    WYN_CONTAINER_BUILDAH,
    WYN_CONTAINER_OCI
} WynContainerType;

// Binary types
typedef enum {
    WYN_BINARY_EXECUTABLE,
    WYN_BINARY_SHARED_LIBRARY,
    WYN_BINARY_STATIC_LIBRARY,
    WYN_BINARY_PLUGIN
} WynBinaryType;

// Package metadata
typedef struct {
    char* name;
    char* version;
    char* description;
    char* maintainer;
    char* homepage;
    char* license;
    char** dependencies;
    size_t dependency_count;
    char** conflicts;
    size_t conflict_count;
    char** provides;
    size_t provide_count;
    uint64_t installed_size;
} WynPackageMetadata;

// Binary configuration
typedef struct {
    WynBinaryType type;
    char* source_path;
    char* output_path;
    bool strip_symbols;
    bool compress;
    bool static_linking;
    char** link_libraries;
    size_t library_count;
    char** runtime_dependencies;
    size_t runtime_dependency_count;
} WynBinaryConfig;

// Package builder
typedef struct WynPackageBuilder {
    WynPackageFormat format;
    WynTarget* target;
    WynPackageMetadata metadata;
    WynBinaryConfig* binaries;
    size_t binary_count;
    char* output_directory;
    char* temp_directory;
    bool verbose;
} WynPackageBuilder;

// Container configuration
typedef struct {
    char* base_image;
    char* working_dir;
    char** environment_vars;
    size_t env_var_count;
    char** exposed_ports;
    size_t port_count;
    char** volumes;
    size_t volume_count;
    char* entrypoint;
    char** cmd_args;
    size_t cmd_arg_count;
    char* user;
    bool multi_stage;
} WynContainerConfig;

// Container builder
typedef struct WynContainerBuilder {
    WynContainerType type;
    WynTarget* target;
    WynContainerConfig config;
    WynBinaryConfig* binaries;
    size_t binary_count;
    char* dockerfile_path;
    char* image_name;
    char* image_tag;
    bool optimize_size;
    bool security_scan;
} WynContainerBuilder;

// Distribution configuration
typedef struct WynDistribution {
    char* project_name;
    char* version;
    WynTarget** targets;
    size_t target_count;
    WynPackageBuilder** packages;
    size_t package_count;
    WynContainerBuilder** containers;
    size_t container_count;
    char* output_directory;
    bool parallel_build;
} WynDistribution;

// Package builder functions
WynPackageBuilder* wyn_package_builder_new(WynPackageFormat format, WynTarget* target);
void wyn_package_builder_free(WynPackageBuilder* builder);
bool wyn_package_builder_set_metadata(WynPackageBuilder* builder, const WynPackageMetadata* metadata);
bool wyn_package_builder_add_binary(WynPackageBuilder* builder, const WynBinaryConfig* binary);
bool wyn_package_builder_set_output_dir(WynPackageBuilder* builder, const char* directory);
bool wyn_package_builder_build(WynPackageBuilder* builder);

// Container builder functions
WynContainerBuilder* wyn_container_builder_new(WynContainerType type, WynTarget* target);
void wyn_container_builder_free(WynContainerBuilder* builder);
bool wyn_container_builder_set_config(WynContainerBuilder* builder, const WynContainerConfig* config);
bool wyn_container_builder_add_binary(WynContainerBuilder* builder, const WynBinaryConfig* binary);
bool wyn_container_builder_set_image_name(WynContainerBuilder* builder, const char* name, const char* tag);
bool wyn_container_builder_generate_dockerfile(WynContainerBuilder* builder);
bool wyn_container_builder_build_image(WynContainerBuilder* builder);

// Distribution functions
WynDistribution* wyn_distribution_new(const char* project_name, const char* version);
void wyn_distribution_free(WynDistribution* distribution);
bool wyn_distribution_add_target(WynDistribution* distribution, WynTarget* target);
bool wyn_distribution_add_package(WynDistribution* distribution, WynPackageBuilder* package);
bool wyn_distribution_add_container(WynDistribution* distribution, WynContainerBuilder* container);
bool wyn_distribution_build_all(WynDistribution* distribution);

// Binary optimization functions
bool wyn_binary_strip_symbols(const char* binary_path);
bool wyn_binary_compress(const char* binary_path, const char* output_path);
bool wyn_binary_static_link(const char* binary_path, char** libraries, size_t library_count);
size_t wyn_binary_get_size(const char* binary_path);
char** wyn_binary_get_dependencies(const char* binary_path, size_t* count);

// Package format specific functions
bool wyn_build_deb_package(WynPackageBuilder* builder);
bool wyn_build_rpm_package(WynPackageBuilder* builder);
bool wyn_build_msi_package(WynPackageBuilder* builder);
bool wyn_build_dmg_package(WynPackageBuilder* builder);
bool wyn_build_tar_gz_package(WynPackageBuilder* builder);
bool wyn_build_zip_package(WynPackageBuilder* builder);
bool wyn_build_appimage_package(WynPackageBuilder* builder);

// Container optimization functions
bool wyn_container_optimize_size(WynContainerBuilder* builder);
bool wyn_container_security_scan(WynContainerBuilder* builder);
bool wyn_container_multi_stage_build(WynContainerBuilder* builder);

// Utility functions
const char* wyn_package_format_extension(WynPackageFormat format);
const char* wyn_package_format_name(WynPackageFormat format);
bool wyn_package_format_supported(WynPackageFormat format, WynTarget* target);
WynPackageFormat* wyn_get_supported_formats(WynTarget* target, size_t* count);

// Metadata helpers
WynPackageMetadata wyn_package_metadata_default(const char* name, const char* version);
void wyn_package_metadata_free(WynPackageMetadata* metadata);
bool wyn_package_metadata_add_dependency(WynPackageMetadata* metadata, const char* dependency);
bool wyn_package_metadata_add_conflict(WynPackageMetadata* metadata, const char* conflict);

// Binary configuration helpers
WynBinaryConfig wyn_binary_config_default(WynBinaryType type, const char* source_path);
void wyn_binary_config_free(WynBinaryConfig* config);
bool wyn_binary_config_add_library(WynBinaryConfig* config, const char* library);
bool wyn_binary_config_add_runtime_dependency(WynBinaryConfig* config, const char* dependency);

// Container configuration helpers
WynContainerConfig wyn_container_config_default(const char* base_image);
void wyn_container_config_free(WynContainerConfig* config);
bool wyn_container_config_add_env_var(WynContainerConfig* config, const char* name, const char* value);
bool wyn_container_config_add_port(WynContainerConfig* config, const char* port);
bool wyn_container_config_add_volume(WynContainerConfig* config, const char* volume);

// Template generation
bool wyn_generate_debian_control(WynPackageBuilder* builder, const char* output_path);
bool wyn_generate_rpm_spec(WynPackageBuilder* builder, const char* output_path);
bool wyn_generate_dockerfile(WynContainerBuilder* builder, const char* output_path);
bool wyn_generate_docker_compose(WynDistribution* distribution, const char* output_path);

// CI/CD integration
bool wyn_generate_github_actions(WynDistribution* distribution, const char* output_path);
bool wyn_generate_gitlab_ci(WynDistribution* distribution, const char* output_path);
bool wyn_generate_jenkins_pipeline(WynDistribution* distribution, const char* output_path);

#endif // WYN_DISTRIBUTION_H
