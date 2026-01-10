#ifndef WYN_ECOSYSTEM_THIRDPARTY_H
#define WYN_ECOSYSTEM_THIRDPARTY_H

#include <stdbool.h>
#include <stddef.h>

// T4.5.x: Third-Party Ecosystem - Package Registry, Community Tools

// Package Registry
typedef struct WynThirdPartyPackage WynThirdPartyPackage;
typedef struct WynPackageRegistry WynPackageRegistry;

WynPackageRegistry* wyn_registry_new(const char* url);
WynThirdPartyPackage* wyn_package_new(const char* name, const char* version, const char* author);
bool wyn_registry_add_package(WynPackageRegistry* registry, WynThirdPartyPackage* package);
WynThirdPartyPackage* wyn_registry_find_package(WynPackageRegistry* registry, const char* name);
bool wyn_package_install(WynThirdPartyPackage* package);
void wyn_package_free(WynThirdPartyPackage* package);
void wyn_registry_free(WynPackageRegistry* registry);

// Community Tools
typedef struct WynCommunityTool WynCommunityTool;
typedef struct WynToolRegistry WynToolRegistry;

WynToolRegistry* wyn_tool_registry_new(void);
WynCommunityTool* wyn_community_tool_new(const char* name, const char* path, const char* version);
bool wyn_tool_registry_add(WynToolRegistry* registry, WynCommunityTool* tool);
WynCommunityTool* wyn_tool_registry_find(WynToolRegistry* registry, const char* name);
bool wyn_community_tool_install(WynCommunityTool* tool);
void wyn_community_tool_free(WynCommunityTool* tool);
void wyn_tool_registry_free(WynToolRegistry* registry);

// Community Contribution Framework
typedef struct WynContributor WynContributor;

WynContributor* wyn_contributor_new(const char* name, const char* email);
void wyn_contributor_add_contribution(WynContributor* contributor);
void wyn_contributor_free(WynContributor* contributor);

// Statistics
void wyn_ecosystem_print_stats(void);
void wyn_ecosystem_reset_stats(void);

#endif
