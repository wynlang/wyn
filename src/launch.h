#ifndef WYN_LAUNCH_H
#define WYN_LAUNCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynLaunchManager WynLaunchManager;
typedef struct WynLanguageStatus WynLanguageStatus;
typedef struct WynProductionMetrics WynProductionMetrics;

// Launch phases
typedef enum {
    WYN_LAUNCH_PREPARATION,
    WYN_LAUNCH_VALIDATION,
    WYN_LAUNCH_DEPLOYMENT,
    WYN_LAUNCH_ANNOUNCEMENT,
    WYN_LAUNCH_MONITORING,
    WYN_LAUNCH_COMPLETE
} WynLaunchPhase;

// Component status
typedef enum {
    WYN_STATUS_NOT_READY,
    WYN_STATUS_READY,
    WYN_STATUS_VALIDATED,
    WYN_STATUS_DEPLOYED,
    WYN_STATUS_ACTIVE
} WynComponentStatus;

// Language components
typedef struct {
    char* name;
    char* version;
    WynComponentStatus status;
    double completion_percentage;
    char* description;
    bool is_critical;
} WynLanguageComponent;

// Production metrics
typedef struct WynProductionMetrics {
    size_t total_components;
    size_t ready_components;
    size_t validated_components;
    size_t deployed_components;
    double overall_completion;
    double performance_score;
    double stability_score;
    double security_score;
    size_t test_coverage_percentage;
    size_t documentation_coverage;
    bool production_ready;
} WynProductionMetrics;

// Language status
typedef struct WynLanguageStatus {
    char* version;
    WynLaunchPhase current_phase;
    WynLanguageComponent* components;
    size_t component_count;
    WynProductionMetrics* metrics;
    char* release_notes;
    char* announcement_text;
    uint64_t launch_timestamp;
    bool is_launched;
} WynLanguageStatus;

// Launch manager
typedef struct WynLaunchManager {
    WynLanguageStatus* status;
    char* project_name;
    char* project_description;
    char* website_url;
    char* repository_url;
    char* documentation_url;
    char* community_url;
    bool pre_launch_checks_passed;
    bool launch_authorized;
} WynLaunchManager;

// Launch manager functions
WynLaunchManager* wyn_launch_manager_new(void);
void wyn_launch_manager_free(WynLaunchManager* manager);
bool wyn_launch_manager_initialize(WynLaunchManager* manager);
bool wyn_launch_manager_prepare_launch(WynLaunchManager* manager);
bool wyn_launch_manager_execute_launch(WynLaunchManager* manager);

// Component management
WynLanguageComponent* wyn_language_component_new(const char* name, const char* version, const char* description);
void wyn_language_component_free(WynLanguageComponent* component);
bool wyn_language_component_set_status(WynLanguageComponent* component, WynComponentStatus status);
bool wyn_language_status_add_component(WynLanguageStatus* status, WynLanguageComponent* component);

// Standard components
bool wyn_register_core_components(WynLanguageStatus* status);
bool wyn_register_compiler_components(WynLanguageStatus* status);
bool wyn_register_stdlib_components(WynLanguageStatus* status);
bool wyn_register_tooling_components(WynLanguageStatus* status);
bool wyn_register_ecosystem_components(WynLanguageStatus* status);

// Launch validation
bool wyn_validate_all_components(WynLaunchManager* manager);
bool wyn_validate_performance_requirements(WynLaunchManager* manager);
bool wyn_validate_security_requirements(WynLaunchManager* manager);
bool wyn_validate_documentation_completeness(WynLaunchManager* manager);
bool wyn_validate_test_coverage(WynLaunchManager* manager);
bool wyn_run_final_integration_tests(WynLaunchManager* manager);

// Launch preparation
bool wyn_prepare_release_artifacts(WynLaunchManager* manager);
bool wyn_prepare_documentation_site(WynLaunchManager* manager);
bool wyn_prepare_community_platforms(WynLaunchManager* manager);
bool wyn_prepare_distribution_channels(WynLaunchManager* manager);
bool wyn_prepare_announcement_materials(WynLaunchManager* manager);

// Launch execution
bool wyn_deploy_compiler_binaries(WynLaunchManager* manager);
bool wyn_deploy_package_registry(WynLaunchManager* manager);
bool wyn_deploy_documentation_site(WynLaunchManager* manager);
bool wyn_activate_community_platforms(WynLaunchManager* manager);
bool wyn_publish_announcement(WynLaunchManager* manager);

// Post-launch monitoring
typedef struct {
    size_t download_count;
    size_t active_users;
    size_t github_stars;
    size_t community_members;
    size_t packages_published;
    double user_satisfaction;
    size_t bug_reports;
    size_t feature_requests;
} WynLaunchMetrics;

WynLaunchMetrics* wyn_collect_launch_metrics(WynLaunchManager* manager);
bool wyn_monitor_launch_success(WynLaunchManager* manager);
bool wyn_generate_launch_report(WynLaunchManager* manager, const char* output_file);

// Production readiness assessment
typedef struct {
    bool compiler_stable;
    bool stdlib_complete;
    bool tooling_functional;
    bool documentation_complete;
    bool performance_acceptable;
    bool security_validated;
    bool community_ready;
    double readiness_score;
} WynReadinessAssessment;

WynReadinessAssessment* wyn_assess_production_readiness(WynLaunchManager* manager);
bool wyn_is_ready_for_production(WynReadinessAssessment* assessment);

// Launch announcement generation
bool wyn_generate_launch_announcement(WynLaunchManager* manager);
bool wyn_generate_press_release(WynLaunchManager* manager, const char* output_file);
bool wyn_generate_blog_post(WynLaunchManager* manager, const char* output_file);
bool wyn_generate_social_media_content(WynLaunchManager* manager);

// Version and release management
typedef struct {
    int major;
    int minor;
    int patch;
    char* pre_release;
    char* build_metadata;
    char* codename;
} WynReleaseVersion;

WynReleaseVersion* wyn_create_launch_version(void);
char* wyn_format_version_string(WynReleaseVersion* version);
bool wyn_tag_release_version(WynReleaseVersion* version);

// Success criteria validation
typedef struct {
    double min_performance_score;
    double min_stability_score;
    double min_security_score;
    size_t min_test_coverage;
    size_t min_documentation_coverage;
    bool require_all_components_ready;
} WynLaunchCriteria;

bool wyn_validate_launch_criteria(WynLaunchManager* manager, WynLaunchCriteria* criteria);
WynLaunchCriteria wyn_get_default_launch_criteria(void);

// Launch timeline and milestones
typedef struct {
    char* milestone_name;
    char* description;
    uint64_t target_date;
    uint64_t completion_date;
    bool is_completed;
    bool is_critical;
} WynLaunchMilestone;

bool wyn_create_launch_timeline(WynLaunchManager* manager);
bool wyn_track_milestone_progress(WynLaunchManager* manager);
bool wyn_validate_timeline_adherence(WynLaunchManager* manager);

// Global launch coordination
bool wyn_coordinate_global_launch(WynLaunchManager* manager);
bool wyn_schedule_launch_events(WynLaunchManager* manager);
bool wyn_notify_stakeholders(WynLaunchManager* manager);
bool wyn_activate_support_channels(WynLaunchManager* manager);

// Utility functions
const char* wyn_launch_phase_name(WynLaunchPhase phase);
const char* wyn_component_status_name(WynComponentStatus status);
bool wyn_is_launch_successful(WynLaunchManager* manager);
double wyn_calculate_launch_readiness(WynLaunchManager* manager);

// Final validation and go-live
bool wyn_perform_final_go_no_go_check(WynLaunchManager* manager);
bool wyn_authorize_production_launch(WynLaunchManager* manager);
bool wyn_execute_go_live_sequence(WynLaunchManager* manager);
bool wyn_celebrate_launch_success(WynLaunchManager* manager);

#endif // WYN_LAUNCH_H
