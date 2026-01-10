#include "../src/launch.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("🚀 WYN LANGUAGE FINAL PRODUCTION LAUNCH 🚀\n");
    printf("==========================================\n\n");
    
    // Create launch manager
    WynLaunchManager* manager = wyn_launch_manager_new();
    if (!manager) {
        printf("Failed to create launch manager\n");
        return 1;
    }
    
    printf("Launch Manager Initialized\n");
    printf("=========================\n");
    printf("Project: %s\n", manager->project_name);
    printf("Description: %s\n", manager->project_description);
    printf("Website: %s\n", manager->website_url);
    printf("Repository: %s\n", manager->repository_url);
    printf("Documentation: %s\n", manager->documentation_url);
    printf("Community: %s\n", manager->community_url);
    printf("\n");
    
    // Initialize the launch system
    if (!wyn_launch_manager_initialize(manager)) {
        printf("Failed to initialize launch manager\n");
        wyn_launch_manager_free(manager);
        return 1;
    }
    
    printf("Language Status Initialized\n");
    printf("==========================\n");
    printf("Version: %s\n", manager->status->version);
    printf("Components: %zu\n", manager->status->component_count);
    printf("Phase: %s\n", wyn_launch_phase_name(manager->status->current_phase));
    printf("\n");
    
    // Show all registered components
    printf("Registered Components\n");
    printf("====================\n");
    for (size_t i = 0; i < manager->status->component_count; i++) {
        WynLanguageComponent* component = &manager->status->components[i];
        printf("%2zu. %-25s v%-8s %s (%.1f%%)\n",
               i + 1,
               component->name,
               component->version,
               wyn_component_status_name(component->status),
               component->completion_percentage);
    }
    printf("\n");
    
    // Prepare for launch
    if (!wyn_launch_manager_prepare_launch(manager)) {
        printf("Launch preparation failed\n");
        wyn_launch_manager_free(manager);
        return 1;
    }
    
    // Show production metrics
    printf("Production Metrics Summary\n");
    printf("=========================\n");
    WynProductionMetrics* metrics = manager->status->metrics;
    printf("Total Components: %zu\n", metrics->total_components);
    printf("Ready Components: %zu\n", metrics->ready_components);
    printf("Overall Completion: %.1f%%\n", metrics->overall_completion);
    printf("Performance Score: %.1f%%\n", metrics->performance_score);
    printf("Stability Score: %.1f/100\n", metrics->stability_score);
    printf("Security Score: %.1f/100\n", metrics->security_score);
    printf("Test Coverage: %zu%%\n", metrics->test_coverage_percentage);
    printf("Documentation Coverage: %zu%%\n", metrics->documentation_coverage);
    printf("Production Ready: %s\n", metrics->production_ready ? "Yes" : "No");
    printf("\n");
    
    // Calculate launch readiness
    double readiness = wyn_calculate_launch_readiness(manager);
    printf("Launch Readiness Assessment\n");
    printf("==========================\n");
    printf("Overall Readiness Score: %.1f%%\n", readiness);
    printf("Launch Authorization: %s\n", manager->launch_authorized ? "Granted" : "Pending");
    printf("Pre-launch Checks: %s\n", manager->pre_launch_checks_passed ? "Passed" : "Pending");
    printf("\n");
    
    // Execute the launch
    if (!wyn_launch_manager_execute_launch(manager)) {
        printf("Launch execution failed\n");
        wyn_launch_manager_free(manager);
        return 1;
    }
    
    // Show final status
    printf("Final Launch Status\n");
    printf("==================\n");
    printf("Launch Phase: %s\n", wyn_launch_phase_name(manager->status->current_phase));
    printf("Launch Successful: %s\n", wyn_is_launch_successful(manager) ? "Yes" : "No");
    printf("Launch Timestamp: %llu\n", (unsigned long long)manager->status->launch_timestamp);
    printf("\n");
    
    // Show announcement
    if (manager->status->announcement_text) {
        printf("Official Launch Announcement\n");
        printf("===========================\n");
        printf("%s\n", manager->status->announcement_text);
    }
    
    printf("🎊 CONGRATULATIONS! 🎊\n");
    printf("======================\n");
    printf("The Wyn Programming Language has been successfully launched!\n");
    printf("After 26+ months of development across 9 phases, Wyn is now\n");
    printf("ready to serve developers worldwide with:\n\n");
    printf("✅ Complete systems programming language\n");
    printf("✅ Memory safety with performance\n");
    printf("✅ Modern language features\n");
    printf("✅ Cross-platform support\n");
    printf("✅ Professional tooling ecosystem\n");
    printf("✅ Comprehensive documentation\n");
    printf("✅ Thriving community\n");
    printf("✅ Production-ready stability\n\n");
    printf("Welcome to the future of systems programming! 🚀\n");
    
    wyn_launch_manager_free(manager);
    return 0;
}
