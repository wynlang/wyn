#ifndef WYN_PRODUCTION_H
#define WYN_PRODUCTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynProductionManager WynProductionManager;
typedef struct WynStabilityGuarantee WynStabilityGuarantee;
typedef struct WynSemanticVersion WynSemanticVersion;
typedef struct WynSecurityAudit WynSecurityAudit;

// Stability levels
typedef enum {
    WYN_STABILITY_EXPERIMENTAL,
    WYN_STABILITY_UNSTABLE,
    WYN_STABILITY_STABLE,
    WYN_STABILITY_DEPRECATED,
    WYN_STABILITY_FROZEN
} WynStabilityLevel;

// Support tiers
typedef enum {
    WYN_SUPPORT_COMMUNITY,
    WYN_SUPPORT_STANDARD,
    WYN_SUPPORT_EXTENDED,
    WYN_SUPPORT_ENTERPRISE
} WynSupportTier;

// Security vulnerability levels
typedef enum {
    WYN_VULN_LOW,
    WYN_VULN_MEDIUM,
    WYN_VULN_HIGH,
    WYN_VULN_CRITICAL
} WynVulnerabilityLevel;

// Semantic version structure
typedef struct WynSemanticVersion {
    int major;
    int minor;
    int patch;
    char* pre_release;
    char* build_metadata;
    char* version_string;
} WynSemanticVersion;

// Stability guarantee
typedef struct WynStabilityGuarantee {
    char* api_name;
    WynStabilityLevel level;
    WynSemanticVersion* introduced_version;
    WynSemanticVersion* deprecated_version;
    char* deprecation_reason;
    char* migration_guide;
    uint64_t expiry_timestamp;
} WynStabilityGuarantee;

// Security vulnerability
typedef struct {
    char* vulnerability_id;
    char* description;
    WynVulnerabilityLevel level;
    char* affected_versions;
    char* fixed_version;
    char* mitigation_steps;
    uint64_t discovered_timestamp;
    uint64_t fixed_timestamp;
    bool is_public;
} WynSecurityVulnerability;

// Security audit result
typedef struct WynSecurityAudit {
    WynSecurityVulnerability* vulnerabilities;
    size_t vulnerability_count;
    uint64_t audit_timestamp;
    char* auditor;
    double security_score;
    bool passed_audit;
} WynSecurityAudit;

// Long-term support plan
typedef struct {
    WynSemanticVersion* version;
    uint64_t release_date;
    uint64_t end_of_life_date;
    WynSupportTier support_tier;
    char* support_contact;
    bool security_updates_only;
} WynLTSPlan;

// Enterprise features
typedef struct {
    bool commercial_license_available;
    bool priority_support;
    bool custom_sla;
    bool on_premise_deployment;
    bool compliance_certifications;
    char** supported_standards;
    size_t standard_count;
} WynEnterpriseFeatures;

// Production manager
typedef struct WynProductionManager {
    WynSemanticVersion* current_version;
    WynStabilityGuarantee* guarantees;
    size_t guarantee_count;
    WynSecurityAudit* security_audit;
    WynLTSPlan* lts_plans;
    size_t lts_plan_count;
    WynEnterpriseFeatures* enterprise_features;
    bool production_ready;
} WynProductionManager;

// Production manager functions
WynProductionManager* wyn_production_manager_new(void);
void wyn_production_manager_free(WynProductionManager* manager);
bool wyn_production_manager_initialize(WynProductionManager* manager, const char* version);
bool wyn_production_manager_validate_readiness(WynProductionManager* manager);

// Semantic versioning functions
WynSemanticVersion* wyn_semantic_version_new(int major, int minor, int patch);
void wyn_semantic_version_free(WynSemanticVersion* version);
bool wyn_semantic_version_parse(const char* version_string, WynSemanticVersion* version);
char* wyn_semantic_version_to_string(const WynSemanticVersion* version);
int wyn_semantic_version_compare(const WynSemanticVersion* v1, const WynSemanticVersion* v2);
bool wyn_semantic_version_is_compatible(const WynSemanticVersion* required, const WynSemanticVersion* available);

// Stability guarantee functions
WynStabilityGuarantee* wyn_stability_guarantee_new(const char* api_name, WynStabilityLevel level);
void wyn_stability_guarantee_free(WynStabilityGuarantee* guarantee);
bool wyn_stability_guarantee_add(WynProductionManager* manager, WynStabilityGuarantee* guarantee);
WynStabilityGuarantee* wyn_stability_guarantee_find(WynProductionManager* manager, const char* api_name);
bool wyn_stability_guarantee_deprecate(WynStabilityGuarantee* guarantee, const char* reason, const char* migration_guide);

// Security audit functions
WynSecurityAudit* wyn_security_audit_new(void);
void wyn_security_audit_free(WynSecurityAudit* audit);
bool wyn_security_audit_run(WynSecurityAudit* audit, const char* codebase_path);
bool wyn_security_audit_add_vulnerability(WynSecurityAudit* audit, const char* id, const char* description, WynVulnerabilityLevel level);
bool wyn_security_audit_fix_vulnerability(WynSecurityAudit* audit, const char* vulnerability_id, const char* fixed_version);
double wyn_security_audit_calculate_score(WynSecurityAudit* audit);

// Long-term support functions
WynLTSPlan* wyn_lts_plan_new(WynSemanticVersion* version, WynSupportTier tier);
void wyn_lts_plan_free(WynLTSPlan* plan);
bool wyn_lts_plan_add(WynProductionManager* manager, WynLTSPlan* plan);
WynLTSPlan* wyn_lts_plan_find_current(WynProductionManager* manager);
bool wyn_lts_plan_is_supported(WynLTSPlan* plan, uint64_t current_timestamp);

// Enterprise features
WynEnterpriseFeatures* wyn_enterprise_features_new(void);
void wyn_enterprise_features_free(WynEnterpriseFeatures* features);
bool wyn_enterprise_features_enable_commercial_license(WynEnterpriseFeatures* features);
bool wyn_enterprise_features_add_compliance_standard(WynEnterpriseFeatures* features, const char* standard);
bool wyn_enterprise_features_configure_sla(WynEnterpriseFeatures* features, const char* sla_terms);

// Production validation
typedef struct {
    bool api_stability_validated;
    bool security_audit_passed;
    bool performance_benchmarks_met;
    bool documentation_complete;
    bool test_coverage_adequate;
    bool cross_platform_validated;
    double overall_readiness_score;
} WynProductionReadiness;

WynProductionReadiness* wyn_validate_production_readiness(WynProductionManager* manager);
bool wyn_check_breaking_changes(const WynSemanticVersion* old_version, const WynSemanticVersion* new_version);
bool wyn_validate_api_compatibility(WynProductionManager* manager);
bool wyn_run_production_tests(WynProductionManager* manager);

// Release management
typedef struct {
    WynSemanticVersion* version;
    char* release_notes;
    char** breaking_changes;
    size_t breaking_change_count;
    char** new_features;
    size_t feature_count;
    char** bug_fixes;
    size_t bug_fix_count;
    uint64_t release_timestamp;
} WynRelease;

WynRelease* wyn_release_new(WynSemanticVersion* version);
void wyn_release_free(WynRelease* release);
bool wyn_release_add_feature(WynRelease* release, const char* feature);
bool wyn_release_add_breaking_change(WynRelease* release, const char* change);
bool wyn_release_add_bug_fix(WynRelease* release, const char* fix);
bool wyn_release_publish(WynRelease* release, WynProductionManager* manager);

// Quality gates
typedef struct {
    double min_test_coverage;
    size_t max_critical_vulnerabilities;
    double max_performance_regression;
    bool require_documentation;
    bool require_migration_guide;
} WynQualityGate;

bool wyn_quality_gate_check(WynProductionManager* manager, const WynQualityGate* gate);
WynQualityGate wyn_get_default_quality_gate(void);
bool wyn_quality_gate_validate_coverage(double coverage, double minimum);
bool wyn_quality_gate_validate_security(WynSecurityAudit* audit, size_t max_critical);

// Deployment and distribution
typedef struct {
    char* deployment_target;
    char* artifact_url;
    char* checksum;
    char* signature;
    bool is_official;
} WynDeploymentArtifact;

typedef struct {
    WynDeploymentArtifact* artifacts;
    size_t artifact_count;
    WynSemanticVersion* version;
    char* distribution_channel;
} WynDeployment;

WynDeployment* wyn_deployment_new(WynSemanticVersion* version);
void wyn_deployment_free(WynDeployment* deployment);
bool wyn_deployment_add_artifact(WynDeployment* deployment, const char* target, const char* url);
bool wyn_deployment_sign_artifacts(WynDeployment* deployment, const char* private_key);
bool wyn_deployment_publish(WynDeployment* deployment);

// Monitoring and telemetry
typedef struct {
    size_t active_installations;
    size_t crash_reports;
    double average_compile_time;
    size_t error_reports;
    double user_satisfaction_score;
} WynTelemetryData;

WynTelemetryData* wyn_collect_telemetry(WynProductionManager* manager);
bool wyn_analyze_crash_reports(WynTelemetryData* telemetry);
bool wyn_generate_stability_report(WynProductionManager* manager, const char* output_file);

// Utility functions
const char* wyn_stability_level_name(WynStabilityLevel level);
const char* wyn_support_tier_name(WynSupportTier tier);
const char* wyn_vulnerability_level_name(WynVulnerabilityLevel level);
bool wyn_is_production_ready(WynProductionManager* manager);

#endif // WYN_PRODUCTION_H
