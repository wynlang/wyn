#ifndef WYN_RELEASE_H
#define WYN_RELEASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynReleaseManager WynReleaseManager;
typedef struct WynReleaseArtifact WynReleaseArtifact;
typedef struct WynReleaseValidation WynReleaseValidation;
typedef struct WynDeploymentConfig WynDeploymentConfig;

// Release types
typedef enum {
    WYN_RELEASE_ALPHA,
    WYN_RELEASE_BETA,
    WYN_RELEASE_RC,
    WYN_RELEASE_STABLE,
    WYN_RELEASE_LTS
} WynReleaseType;

// Artifact types
typedef enum {
    WYN_ARTIFACT_COMPILER,
    WYN_ARTIFACT_STDLIB,
    WYN_ARTIFACT_DOCUMENTATION,
    WYN_ARTIFACT_TOOLS,
    WYN_ARTIFACT_EXAMPLES,
    WYN_ARTIFACT_TESTS
} WynArtifactType;

// Validation status
typedef enum {
    WYN_VALIDATION_PENDING,
    WYN_VALIDATION_RUNNING,
    WYN_VALIDATION_PASSED,
    WYN_VALIDATION_FAILED
} WynValidationStatus;

// Platform targets
typedef enum {
    WYN_PLATFORM_LINUX_X64,
    WYN_PLATFORM_LINUX_ARM64,
    WYN_PLATFORM_MACOS_X64,
    WYN_PLATFORM_MACOS_ARM64,
    WYN_PLATFORM_WINDOWS_X64,
    WYN_PLATFORM_WINDOWS_ARM64,
    WYN_PLATFORM_WASM32
} WynPlatform;

// Release artifact
typedef struct WynReleaseArtifact {
    char* name;
    WynArtifactType type;
    WynPlatform platform;
    char* file_path;
    char* checksum;
    char* signature;
    size_t file_size;
    uint64_t build_timestamp;
    bool is_signed;
    bool is_verified;
} WynReleaseArtifact;

// Release validation
typedef struct WynReleaseValidation {
    char* test_name;
    WynValidationStatus status;
    char* description;
    char* error_message;
    double execution_time;
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    bool is_critical;
} WynReleaseValidation;

// Deployment configuration
typedef struct WynDeploymentConfig {
    char* registry_url;
    char* cdn_url;
    char* documentation_url;
    char* github_release_url;
    char* signing_key_path;
    char* deployment_script;
    bool auto_deploy;
    bool create_github_release;
    bool update_documentation;
    bool notify_community;
} WynDeploymentConfig;

// Release manager
typedef struct WynReleaseManager {
    char* version;
    WynReleaseType release_type;
    WynReleaseArtifact* artifacts;
    size_t artifact_count;
    WynReleaseValidation* validations;
    size_t validation_count;
    WynDeploymentConfig* deployment_config;
    char* release_notes;
    char* changelog;
    uint64_t release_timestamp;
    bool is_ready_for_release;
    bool is_released;
} WynReleaseManager;

// Release manager functions
WynReleaseManager* wyn_release_manager_new(const char* version, WynReleaseType type);
void wyn_release_manager_free(WynReleaseManager* manager);
bool wyn_release_manager_initialize(WynReleaseManager* manager);
bool wyn_release_manager_validate_release(WynReleaseManager* manager);
bool wyn_release_manager_create_release(WynReleaseManager* manager);

// Artifact management
WynReleaseArtifact* wyn_release_artifact_new(const char* name, WynArtifactType type, WynPlatform platform);
void wyn_release_artifact_free(WynReleaseArtifact* artifact);
bool wyn_release_artifact_build(WynReleaseArtifact* artifact, const char* source_path);
bool wyn_release_artifact_sign(WynReleaseArtifact* artifact, const char* private_key);
bool wyn_release_artifact_verify(WynReleaseArtifact* artifact, const char* public_key);
bool wyn_release_artifact_calculate_checksum(WynReleaseArtifact* artifact);

// Artifact collection
bool wyn_release_manager_add_artifact(WynReleaseManager* manager, WynReleaseArtifact* artifact);
WynReleaseArtifact* wyn_release_manager_find_artifact(WynReleaseManager* manager, const char* name);
bool wyn_release_manager_build_all_artifacts(WynReleaseManager* manager);
bool wyn_release_manager_sign_all_artifacts(WynReleaseManager* manager, const char* private_key);

// Release validation
WynReleaseValidation* wyn_release_validation_new(const char* test_name, const char* description, bool is_critical);
void wyn_release_validation_free(WynReleaseValidation* validation);
bool wyn_release_validation_run(WynReleaseValidation* validation);
bool wyn_release_manager_add_validation(WynReleaseManager* manager, WynReleaseValidation* validation);
bool wyn_release_manager_run_all_validations(WynReleaseManager* manager);

// Standard validations
bool wyn_validate_compiler_functionality(WynReleaseManager* manager);
bool wyn_validate_standard_library(WynReleaseManager* manager);
bool wyn_validate_cross_platform_compatibility(WynReleaseManager* manager);
bool wyn_validate_performance_benchmarks(WynReleaseManager* manager);
bool wyn_validate_memory_safety(WynReleaseManager* manager);
bool wyn_validate_security_audit(WynReleaseManager* manager);
bool wyn_validate_documentation_completeness(WynReleaseManager* manager);
bool wyn_validate_example_programs(WynReleaseManager* manager);

// Deployment configuration
WynDeploymentConfig* wyn_deployment_config_new(void);
void wyn_deployment_config_free(WynDeploymentConfig* config);
bool wyn_deployment_config_load(WynDeploymentConfig* config, const char* config_file);
bool wyn_deployment_config_save(WynDeploymentConfig* config, const char* config_file);

// Release deployment
bool wyn_release_manager_deploy(WynReleaseManager* manager);
bool wyn_deploy_to_registry(WynReleaseManager* manager);
bool wyn_deploy_to_cdn(WynReleaseManager* manager);
bool wyn_create_github_release(WynReleaseManager* manager);
bool wyn_update_documentation_site(WynReleaseManager* manager);
bool wyn_notify_community(WynReleaseManager* manager);

// Release notes and changelog
bool wyn_release_manager_generate_release_notes(WynReleaseManager* manager);
bool wyn_release_manager_update_changelog(WynReleaseManager* manager);
bool wyn_release_manager_set_release_notes(WynReleaseManager* manager, const char* notes);

// Version management
typedef struct {
    int major;
    int minor;
    int patch;
    char* pre_release;
    char* build_metadata;
} WynVersion;

WynVersion* wyn_version_parse(const char* version_string);
void wyn_version_free(WynVersion* version);
char* wyn_version_to_string(const WynVersion* version);
bool wyn_version_is_compatible(const WynVersion* required, const WynVersion* available);
bool wyn_version_is_newer(const WynVersion* v1, const WynVersion* v2);

// Release statistics and metrics
typedef struct {
    size_t total_artifacts;
    size_t signed_artifacts;
    size_t verified_artifacts;
    size_t passed_validations;
    size_t failed_validations;
    size_t critical_failures;
    double total_build_time;
    double total_validation_time;
    size_t total_file_size;
} WynReleaseMetrics;

WynReleaseMetrics* wyn_release_manager_get_metrics(WynReleaseManager* manager);
bool wyn_release_metrics_generate_report(WynReleaseMetrics* metrics, const char* output_file);

// Quality gates
typedef struct {
    double min_test_coverage;
    size_t max_critical_failures;
    double max_build_time_minutes;
    bool require_all_platforms;
    bool require_signed_artifacts;
    bool require_security_audit;
} WynReleaseQualityGate;

bool wyn_release_manager_check_quality_gates(WynReleaseManager* manager, const WynReleaseQualityGate* gates);
WynReleaseQualityGate wyn_get_default_quality_gates(WynReleaseType type);

// Rollback and recovery
typedef struct {
    char* previous_version;
    char* rollback_script;
    char* backup_location;
    bool auto_rollback_on_failure;
} WynRollbackConfig;

WynRollbackConfig* wyn_rollback_config_new(void);
void wyn_rollback_config_free(WynRollbackConfig* config);
bool wyn_release_manager_create_rollback_point(WynReleaseManager* manager, WynRollbackConfig* config);
bool wyn_release_manager_rollback(WynReleaseManager* manager, WynRollbackConfig* config);

// Utility functions
const char* wyn_release_type_name(WynReleaseType type);
const char* wyn_artifact_type_name(WynArtifactType type);
const char* wyn_platform_name(WynPlatform platform);
const char* wyn_validation_status_name(WynValidationStatus status);
bool wyn_is_release_ready(WynReleaseManager* manager);

// CI/CD integration
typedef struct {
    char* build_id;
    char* commit_hash;
    char* branch_name;
    char* build_url;
    char* test_results_url;
    bool is_ci_build;
} WynCIInfo;

WynCIInfo* wyn_ci_info_new(void);
void wyn_ci_info_free(WynCIInfo* info);
bool wyn_release_manager_set_ci_info(WynReleaseManager* manager, WynCIInfo* ci_info);
bool wyn_generate_ci_release_report(WynReleaseManager* manager, const char* output_file);

// Release automation
bool wyn_automate_release_process(WynReleaseManager* manager);
bool wyn_schedule_release(WynReleaseManager* manager, uint64_t scheduled_timestamp);
bool wyn_cancel_scheduled_release(WynReleaseManager* manager);

#endif // WYN_RELEASE_H
