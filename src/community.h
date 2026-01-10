#ifndef WYN_COMMUNITY_H
#define WYN_COMMUNITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynCommunityManager WynCommunityManager;
typedef struct WynContributor WynContributor;
typedef struct WynProject WynProject;
typedef struct WynCommunityMetrics WynCommunityMetrics;

// Community roles
typedef enum {
    WYN_ROLE_USER,
    WYN_ROLE_CONTRIBUTOR,
    WYN_ROLE_MAINTAINER,
    WYN_ROLE_CORE_TEAM,
    WYN_ROLE_ADMIN
} WynCommunityRole;

// Project types
typedef enum {
    WYN_PROJECT_LIBRARY,
    WYN_PROJECT_APPLICATION,
    WYN_PROJECT_TOOL,
    WYN_PROJECT_EXAMPLE,
    WYN_PROJECT_TUTORIAL,
    WYN_PROJECT_TEMPLATE
} WynProjectType;

// Contribution types
typedef enum {
    WYN_CONTRIB_CODE,
    WYN_CONTRIB_DOCUMENTATION,
    WYN_CONTRIB_TESTING,
    WYN_CONTRIB_DESIGN,
    WYN_CONTRIB_TRANSLATION,
    WYN_CONTRIB_COMMUNITY
} WynContributionType;

// Community platforms
typedef enum {
    WYN_PLATFORM_GITHUB,
    WYN_PLATFORM_DISCORD,
    WYN_PLATFORM_REDDIT,
    WYN_PLATFORM_TWITTER,
    WYN_PLATFORM_FORUM,
    WYN_PLATFORM_BLOG
} WynCommunityPlatform;

// Contributor information
typedef struct WynContributor {
    char* username;
    char* email;
    char* real_name;
    WynCommunityRole role;
    WynContributionType* contribution_types;
    size_t contribution_count;
    uint64_t join_date;
    uint64_t last_activity;
    size_t total_contributions;
    bool is_active;
    bool is_verified;
} WynContributor;

// Community project
typedef struct WynProject {
    char* name;
    char* description;
    char* repository_url;
    char* documentation_url;
    WynProjectType type;
    WynContributor* maintainers;
    size_t maintainer_count;
    char* license;
    char* version;
    uint64_t created_date;
    uint64_t last_updated;
    size_t star_count;
    size_t fork_count;
    size_t download_count;
    bool is_official;
    bool is_featured;
} WynProject;

// Community metrics
typedef struct WynCommunityMetrics {
    size_t total_users;
    size_t active_contributors;
    size_t total_projects;
    size_t official_projects;
    size_t community_projects;
    size_t total_downloads;
    size_t github_stars;
    size_t discord_members;
    size_t reddit_subscribers;
    size_t forum_posts;
    double growth_rate;
    double engagement_score;
} WynCommunityMetrics;

// Community events
typedef struct {
    char* name;
    char* description;
    char* location;
    uint64_t start_date;
    uint64_t end_date;
    char* organizer;
    size_t attendee_count;
    bool is_virtual;
    bool is_official;
} WynCommunityEvent;

// Community manager
typedef struct WynCommunityManager {
    WynContributor* contributors;
    size_t contributor_count;
    WynProject* projects;
    size_t project_count;
    WynCommunityEvent* events;
    size_t event_count;
    WynCommunityMetrics* metrics;
    char* code_of_conduct;
    char* contribution_guidelines;
    char* governance_model;
} WynCommunityManager;

// Community manager functions
WynCommunityManager* wyn_community_manager_new(void);
void wyn_community_manager_free(WynCommunityManager* manager);
bool wyn_community_manager_initialize(WynCommunityManager* manager);
bool wyn_community_manager_update_metrics(WynCommunityManager* manager);

// Contributor management
WynContributor* wyn_contributor_new(const char* username, const char* email);
void wyn_contributor_free(WynContributor* contributor);
bool wyn_contributor_add_contribution(WynContributor* contributor, WynContributionType type);
bool wyn_contributor_promote_role(WynContributor* contributor, WynCommunityRole new_role);
bool wyn_community_manager_add_contributor(WynCommunityManager* manager, WynContributor* contributor);
WynContributor* wyn_community_manager_find_contributor(WynCommunityManager* manager, const char* username);

// Project management
WynProject* wyn_project_new(const char* name, const char* description, WynProjectType type);
void wyn_project_free(WynProject* project);
bool wyn_project_add_maintainer(WynProject* project, WynContributor* maintainer);
bool wyn_project_set_official(WynProject* project, bool is_official);
bool wyn_project_set_featured(WynProject* project, bool is_featured);
bool wyn_community_manager_add_project(WynCommunityManager* manager, WynProject* project);
WynProject* wyn_community_manager_find_project(WynCommunityManager* manager, const char* name);

// Community guidelines and governance
bool wyn_community_manager_set_code_of_conduct(WynCommunityManager* manager, const char* code_of_conduct);
bool wyn_community_manager_set_contribution_guidelines(WynCommunityManager* manager, const char* guidelines);
bool wyn_community_manager_set_governance_model(WynCommunityManager* manager, const char* governance);

// Community engagement
typedef struct {
    char* title;
    char* content;
    char* author;
    uint64_t publish_date;
    size_t view_count;
    size_t like_count;
    size_t comment_count;
} WynBlogPost;

typedef struct {
    char* question;
    char* answer;
    char* author;
    uint64_t created_date;
    size_t upvote_count;
    bool is_verified;
} WynForumPost;

WynBlogPost* wyn_blog_post_new(const char* title, const char* content, const char* author);
void wyn_blog_post_free(WynBlogPost* post);
WynForumPost* wyn_forum_post_new(const char* question, const char* answer, const char* author);
void wyn_forum_post_free(WynForumPost* post);

// Community events
WynCommunityEvent* wyn_community_event_new(const char* name, const char* description);
void wyn_community_event_free(WynCommunityEvent* event);
bool wyn_community_event_set_virtual(WynCommunityEvent* event, bool is_virtual);
bool wyn_community_event_set_official(WynCommunityEvent* event, bool is_official);
bool wyn_community_manager_add_event(WynCommunityManager* manager, WynCommunityEvent* event);

// Community outreach
typedef struct {
    WynCommunityPlatform platform;
    char* account_name;
    size_t follower_count;
    size_t post_count;
    double engagement_rate;
    bool is_verified;
} WynSocialMediaAccount;

WynSocialMediaAccount* wyn_social_media_account_new(WynCommunityPlatform platform, const char* account_name);
void wyn_social_media_account_free(WynSocialMediaAccount* account);
bool wyn_social_media_post_update(WynSocialMediaAccount* account, const char* content);

// Community analytics
typedef struct {
    uint64_t timestamp;
    size_t new_users;
    size_t active_users;
    size_t new_projects;
    size_t downloads;
    double engagement_score;
} WynCommunitySnapshot;

WynCommunitySnapshot* wyn_community_snapshot_new(void);
void wyn_community_snapshot_free(WynCommunitySnapshot* snapshot);
bool wyn_community_manager_take_snapshot(WynCommunityManager* manager, WynCommunitySnapshot* snapshot);
bool wyn_community_generate_report(WynCommunityManager* manager, const char* output_file);

// Community growth initiatives
typedef struct {
    char* name;
    char* description;
    uint64_t start_date;
    uint64_t end_date;
    char* target_metric;
    double target_value;
    double current_value;
    bool is_active;
} WynGrowthInitiative;

WynGrowthInitiative* wyn_growth_initiative_new(const char* name, const char* description);
void wyn_growth_initiative_free(WynGrowthInitiative* initiative);
bool wyn_growth_initiative_set_target(WynGrowthInitiative* initiative, const char* metric, double target);
bool wyn_growth_initiative_update_progress(WynGrowthInitiative* initiative, double current_value);

// Community support
typedef struct {
    char* category;
    char* question;
    char* answer;
    char* tags;
    size_t view_count;
    bool is_official;
} WynFAQEntry;

typedef struct {
    char* title;
    char* content;
    char* category;
    uint64_t created_date;
    uint64_t updated_date;
    char* author;
} WynTutorial;

WynFAQEntry* wyn_faq_entry_new(const char* category, const char* question, const char* answer);
void wyn_faq_entry_free(WynFAQEntry* entry);
WynTutorial* wyn_tutorial_new(const char* title, const char* content, const char* category);
void wyn_tutorial_free(WynTutorial* tutorial);

// Community moderation
typedef struct {
    char* username;
    char* reason;
    uint64_t start_date;
    uint64_t end_date;
    char* moderator;
    bool is_permanent;
} WynModerationAction;

WynModerationAction* wyn_moderation_action_new(const char* username, const char* reason);
void wyn_moderation_action_free(WynModerationAction* action);
bool wyn_moderation_action_set_temporary(WynModerationAction* action, uint64_t duration_days);
bool wyn_moderation_action_set_permanent(WynModerationAction* action);

// Utility functions
const char* wyn_community_role_name(WynCommunityRole role);
const char* wyn_project_type_name(WynProjectType type);
const char* wyn_contribution_type_name(WynContributionType type);
const char* wyn_community_platform_name(WynCommunityPlatform platform);
bool wyn_is_contributor_active(WynContributor* contributor);
double wyn_calculate_engagement_score(WynCommunityMetrics* metrics);

// Community bootstrapping
bool wyn_bootstrap_community_infrastructure(WynCommunityManager* manager);
bool wyn_create_official_projects(WynCommunityManager* manager);
bool wyn_setup_community_platforms(WynCommunityManager* manager);
bool wyn_launch_community_initiatives(WynCommunityManager* manager);

#endif // WYN_COMMUNITY_H
