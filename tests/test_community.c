#include "../src/community.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_community_manager_creation() {
    printf("Testing community manager creation...\n");
    
    WynCommunityManager* manager = wyn_community_manager_new();
    assert(manager != NULL);
    assert(manager->contributor_count == 0);
    assert(manager->project_count == 0);
    assert(manager->event_count == 0);
    assert(manager->metrics == NULL);
    
    assert(wyn_community_manager_initialize(manager));
    assert(manager->metrics != NULL);
    assert(manager->code_of_conduct != NULL);
    assert(manager->contribution_guidelines != NULL);
    assert(manager->governance_model != NULL);
    
    wyn_community_manager_free(manager);
    printf("✓ Community manager creation tests passed\n");
}

void test_contributor_management() {
    printf("Testing contributor management...\n");
    
    WynCommunityManager* manager = wyn_community_manager_new();
    wyn_community_manager_initialize(manager);
    
    // Create contributors
    WynContributor* alice = wyn_contributor_new("alice", "alice@example.com");
    WynContributor* bob = wyn_contributor_new("bob", "bob@example.com");
    
    assert(alice != NULL);
    assert(bob != NULL);
    assert(strcmp(alice->username, "alice") == 0);
    assert(alice->role == WYN_ROLE_USER);
    assert(alice->is_active == true);
    
    // Add contributions
    assert(wyn_contributor_add_contribution(alice, WYN_CONTRIB_CODE));
    assert(wyn_contributor_add_contribution(alice, WYN_CONTRIB_DOCUMENTATION));
    assert(alice->contribution_count == 2);
    assert(alice->total_contributions == 2);
    
    // Promote role
    assert(wyn_contributor_promote_role(alice, WYN_ROLE_MAINTAINER));
    assert(alice->role == WYN_ROLE_MAINTAINER);
    
    // Add to manager
    assert(wyn_community_manager_add_contributor(manager, alice));
    assert(wyn_community_manager_add_contributor(manager, bob));
    assert(manager->contributor_count == 2);
    
    // Find contributors
    WynContributor* found = wyn_community_manager_find_contributor(manager, "alice");
    assert(found != NULL);
    assert(strcmp(found->username, "alice") == 0);
    
    wyn_contributor_free(alice);
    wyn_contributor_free(bob);
    wyn_community_manager_free(manager);
    
    printf("✓ Contributor management tests passed\n");
}

void test_project_management() {
    printf("Testing project management...\n");
    
    WynCommunityManager* manager = wyn_community_manager_new();
    wyn_community_manager_initialize(manager);
    
    // Create projects
    WynProject* web_lib = wyn_project_new("wyn-web", "Web framework for Wyn", WYN_PROJECT_LIBRARY);
    WynProject* cli_tool = wyn_project_new("wyn-cli", "CLI tools", WYN_PROJECT_TOOL);
    
    assert(web_lib != NULL);
    assert(cli_tool != NULL);
    assert(strcmp(web_lib->name, "wyn-web") == 0);
    assert(web_lib->type == WYN_PROJECT_LIBRARY);
    assert(web_lib->is_official == false);
    
    // Set project properties
    assert(wyn_project_set_official(web_lib, true));
    assert(wyn_project_set_featured(web_lib, true));
    assert(web_lib->is_official == true);
    assert(web_lib->is_featured == true);
    
    // Add to manager
    assert(wyn_community_manager_add_project(manager, web_lib));
    assert(wyn_community_manager_add_project(manager, cli_tool));
    assert(manager->project_count == 2);
    
    // Find projects
    WynProject* found = wyn_community_manager_find_project(manager, "wyn-web");
    assert(found != NULL);
    assert(strcmp(found->name, "wyn-web") == 0);
    
    wyn_project_free(web_lib);
    wyn_project_free(cli_tool);
    wyn_community_manager_free(manager);
    
    printf("✓ Project management tests passed\n");
}

void test_community_metrics() {
    printf("Testing community metrics...\n");
    
    WynCommunityManager* manager = wyn_community_manager_new();
    wyn_community_manager_initialize(manager);
    
    // Add some contributors and projects
    WynContributor* contributor1 = wyn_contributor_new("user1", "user1@example.com");
    WynContributor* contributor2 = wyn_contributor_new("user2", "user2@example.com");
    contributor1->is_active = true;
    contributor2->is_active = false;
    
    wyn_community_manager_add_contributor(manager, contributor1);
    wyn_community_manager_add_contributor(manager, contributor2);
    
    WynProject* official_project = wyn_project_new("official", "Official project", WYN_PROJECT_LIBRARY);
    WynProject* community_project = wyn_project_new("community", "Community project", WYN_PROJECT_APPLICATION);
    wyn_project_set_official(official_project, true);
    
    wyn_community_manager_add_project(manager, official_project);
    wyn_community_manager_add_project(manager, community_project);
    
    // Update metrics
    assert(wyn_community_manager_update_metrics(manager));
    
    WynCommunityMetrics* metrics = manager->metrics;
    assert(metrics->total_projects == 2);
    assert(metrics->official_projects == 1);
    assert(metrics->community_projects == 1);
    assert(metrics->active_contributors == 1);
    assert(metrics->engagement_score > 0.0);
    
    wyn_contributor_free(contributor1);
    wyn_contributor_free(contributor2);
    wyn_project_free(official_project);
    wyn_project_free(community_project);
    wyn_community_manager_free(manager);
    
    printf("✓ Community metrics tests passed\n");
}

void test_community_bootstrapping() {
    printf("Testing community bootstrapping...\n");
    
    WynCommunityManager* manager = wyn_community_manager_new();
    wyn_community_manager_initialize(manager);
    
    // Bootstrap infrastructure
    assert(wyn_bootstrap_community_infrastructure(manager));
    assert(manager->contributor_count >= 2); // Core team members added
    
    // Create official projects
    assert(wyn_create_official_projects(manager));
    assert(manager->project_count >= 5); // Official projects added
    
    // Set up platforms
    assert(wyn_setup_community_platforms(manager));
    
    // Launch initiatives
    assert(wyn_launch_community_initiatives(manager));
    
    // Update metrics after bootstrapping
    wyn_community_manager_update_metrics(manager);
    
    printf("Final community stats:\n");
    printf("- Contributors: %zu\n", manager->contributor_count);
    printf("- Projects: %zu\n", manager->project_count);
    printf("- Official projects: %zu\n", manager->metrics->official_projects);
    printf("- Community projects: %zu\n", manager->metrics->community_projects);
    printf("- Engagement score: %.1f\n", manager->metrics->engagement_score);
    
    wyn_community_manager_free(manager);
    
    printf("✓ Community bootstrapping tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test role names
    assert(strcmp(wyn_community_role_name(WYN_ROLE_USER), "User") == 0);
    assert(strcmp(wyn_community_role_name(WYN_ROLE_MAINTAINER), "Maintainer") == 0);
    assert(strcmp(wyn_community_role_name(WYN_ROLE_CORE_TEAM), "Core Team") == 0);
    
    // Test project type names
    assert(strcmp(wyn_project_type_name(WYN_PROJECT_LIBRARY), "Library") == 0);
    assert(strcmp(wyn_project_type_name(WYN_PROJECT_APPLICATION), "Application") == 0);
    assert(strcmp(wyn_project_type_name(WYN_PROJECT_TOOL), "Tool") == 0);
    
    // Test contribution type names
    assert(strcmp(wyn_contribution_type_name(WYN_CONTRIB_CODE), "Code") == 0);
    assert(strcmp(wyn_contribution_type_name(WYN_CONTRIB_DOCUMENTATION), "Documentation") == 0);
    
    // Test platform names
    assert(strcmp(wyn_community_platform_name(WYN_PLATFORM_GITHUB), "GitHub") == 0);
    assert(strcmp(wyn_community_platform_name(WYN_PLATFORM_DISCORD), "Discord") == 0);
    
    // Test engagement score calculation
    WynCommunityMetrics metrics = {0};
    metrics.total_users = 100;
    metrics.active_contributors = 20;
    metrics.total_projects = 10;
    metrics.community_projects = 7;
    metrics.forum_posts = 150;
    metrics.discord_members = 600;
    
    double score = wyn_calculate_engagement_score(&metrics);
    assert(score > 0.0 && score <= 100.0);
    
    printf("✓ Utility function tests passed\n");
}

int main() {
    printf("Running Community Ecosystem Development Tests...\n\n");
    
    test_community_manager_creation();
    test_contributor_management();
    test_project_management();
    test_community_metrics();
    test_community_bootstrapping();
    test_utility_functions();
    
    printf("\n🎉 All community ecosystem tests passed!\n");
    printf("Community system provides:\n");
    printf("- Comprehensive contributor management with roles and contributions\n");
    printf("- Project management with official and community projects\n");
    printf("- Community metrics and engagement scoring\n");
    printf("- Automated community infrastructure bootstrapping\n");
    printf("- Multi-platform community presence (GitHub, Discord, Reddit, Forum)\n");
    printf("- Growth initiatives and recognition programs\n");
    printf("- Governance model and community guidelines\n");
    printf("\n🌟 Wyn Language community is ready to thrive!\n");
    
    return 0;
}
