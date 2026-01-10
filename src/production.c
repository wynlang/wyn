#include "production.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Production manager functions
WynProductionManager* wyn_production_manager_new(void) {
    WynProductionManager* manager = malloc(sizeof(WynProductionManager));
    if (!manager) return NULL;
    
    manager->current_version = NULL;
    manager->guarantees = NULL;
    manager->guarantee_count = 0;
    manager->security_audit = NULL;
    manager->lts_plans = NULL;
    manager->lts_plan_count = 0;
    manager->enterprise_features = NULL;
    manager->production_ready = false;
    
    return manager;
}

void wyn_production_manager_free(WynProductionManager* manager) {
    if (!manager) return;
    
    if (manager->current_version) {
        wyn_semantic_version_free(manager->current_version);
    }
    
    for (size_t i = 0; i < manager->guarantee_count; i++) {
        free(manager->guarantees[i].api_name);
        if (manager->guarantees[i].introduced_version) {
            wyn_semantic_version_free(manager->guarantees[i].introduced_version);
        }
        if (manager->guarantees[i].deprecated_version) {
            wyn_semantic_version_free(manager->guarantees[i].deprecated_version);
        }
        free(manager->guarantees[i].deprecation_reason);
        free(manager->guarantees[i].migration_guide);
    }
    free(manager->guarantees);
    
    if (manager->security_audit) {
        wyn_security_audit_free(manager->security_audit);
    }
    
    for (size_t i = 0; i < manager->lts_plan_count; i++) {
        if (manager->lts_plans[i].version) {
            wyn_semantic_version_free(manager->lts_plans[i].version);
        }
        free(manager->lts_plans[i].support_contact);
    }
    free(manager->lts_plans);
    
    if (manager->enterprise_features) {
        wyn_enterprise_features_free(manager->enterprise_features);
    }
    
    free(manager);
}

bool wyn_production_manager_initialize(WynProductionManager* manager, const char* version) {
    if (!manager || !version) return false;
    
    manager->current_version = wyn_semantic_version_new(1, 0, 0);
    if (!manager->current_version) return false;
    
    if (!wyn_semantic_version_parse(version, manager->current_version)) {
        wyn_semantic_version_free(manager->current_version);
        manager->current_version = NULL;
        return false;
    }
    
    manager->security_audit = wyn_security_audit_new();
    manager->enterprise_features = wyn_enterprise_features_new();
    
    return true;
}

bool wyn_production_manager_validate_readiness(WynProductionManager* manager) {
    if (!manager) return false;
    
    // Check version stability
    if (!manager->current_version || manager->current_version->major < 1) {
        return false;
    }
    
    // Check security audit
    if (!manager->security_audit || !manager->security_audit->passed_audit) {
        return false;
    }
    
    // Check stability guarantees
    if (manager->guarantee_count == 0) {
        return false;
    }
    
    manager->production_ready = true;
    return true;
}

// Semantic version functions
WynSemanticVersion* wyn_semantic_version_new(int major, int minor, int patch) {
    WynSemanticVersion* version = malloc(sizeof(WynSemanticVersion));
    if (!version) return NULL;
    
    version->major = major;
    version->minor = minor;
    version->patch = patch;
    version->pre_release = NULL;
    version->build_metadata = NULL;
    version->version_string = NULL;
    
    return version;
}

void wyn_semantic_version_free(WynSemanticVersion* version) {
    if (!version) return;
    
    free(version->pre_release);
    free(version->build_metadata);
    free(version->version_string);
    free(version);
}

bool wyn_semantic_version_parse(const char* version_string, WynSemanticVersion* version) {
    if (!version_string || !version) return false;
    
    int parsed = sscanf(version_string, "%d.%d.%d", 
                       &version->major, &version->minor, &version->patch);
    
    if (parsed != 3) return false;
    
    version->version_string = strdup(version_string);
    return true;
}

char* wyn_semantic_version_to_string(const WynSemanticVersion* version) {
    if (!version) return NULL;
    
    char* result = malloc(64);
    if (!result) return NULL;
    
    snprintf(result, 64, "%d.%d.%d", version->major, version->minor, version->patch);
    return result;
}

int wyn_semantic_version_compare(const WynSemanticVersion* v1, const WynSemanticVersion* v2) {
    if (!v1 || !v2) return 0;
    
    if (v1->major != v2->major) return v1->major - v2->major;
    if (v1->minor != v2->minor) return v1->minor - v2->minor;
    return v1->patch - v2->patch;
}

bool wyn_semantic_version_is_compatible(const WynSemanticVersion* required, const WynSemanticVersion* available) {
    if (!required || !available) return false;
    
    // Major version must match for compatibility
    if (required->major != available->major) return false;
    
    // Available version must be >= required version
    return wyn_semantic_version_compare(available, required) >= 0;
}
// Stability guarantee functions
WynStabilityGuarantee* wyn_stability_guarantee_new(const char* api_name, WynStabilityLevel level) {
    WynStabilityGuarantee* guarantee = malloc(sizeof(WynStabilityGuarantee));
    if (!guarantee) return NULL;
    
    guarantee->api_name = strdup(api_name);
    guarantee->level = level;
    guarantee->introduced_version = NULL;
    guarantee->deprecated_version = NULL;
    guarantee->deprecation_reason = NULL;
    guarantee->migration_guide = NULL;
    guarantee->expiry_timestamp = 0;
    
    return guarantee;
}

void wyn_stability_guarantee_free(WynStabilityGuarantee* guarantee) {
    if (!guarantee) return;
    
    free(guarantee->api_name);
    if (guarantee->introduced_version) {
        wyn_semantic_version_free(guarantee->introduced_version);
    }
    if (guarantee->deprecated_version) {
        wyn_semantic_version_free(guarantee->deprecated_version);
    }
    free(guarantee->deprecation_reason);
    free(guarantee->migration_guide);
    free(guarantee);
}

bool wyn_stability_guarantee_add(WynProductionManager* manager, WynStabilityGuarantee* guarantee) {
    if (!manager || !guarantee) return false;
    
    WynStabilityGuarantee* new_guarantees = realloc(manager->guarantees, 
        (manager->guarantee_count + 1) * sizeof(WynStabilityGuarantee));
    if (!new_guarantees) return false;
    
    manager->guarantees = new_guarantees;
    // Copy the guarantee data, not the pointer
    manager->guarantees[manager->guarantee_count].api_name = strdup(guarantee->api_name);
    manager->guarantees[manager->guarantee_count].level = guarantee->level;
    manager->guarantees[manager->guarantee_count].introduced_version = guarantee->introduced_version;
    manager->guarantees[manager->guarantee_count].deprecated_version = guarantee->deprecated_version;
    manager->guarantees[manager->guarantee_count].deprecation_reason = guarantee->deprecation_reason;
    manager->guarantees[manager->guarantee_count].migration_guide = guarantee->migration_guide;
    manager->guarantees[manager->guarantee_count].expiry_timestamp = guarantee->expiry_timestamp;
    manager->guarantee_count++;
    
    return true;
}

WynStabilityGuarantee* wyn_stability_guarantee_find(WynProductionManager* manager, const char* api_name) {
    if (!manager || !api_name) return NULL;
    
    for (size_t i = 0; i < manager->guarantee_count; i++) {
        if (strcmp(manager->guarantees[i].api_name, api_name) == 0) {
            return &manager->guarantees[i];
        }
    }
    
    return NULL;
}

bool wyn_stability_guarantee_deprecate(WynStabilityGuarantee* guarantee, const char* reason, const char* migration_guide) {
    if (!guarantee) return false;
    
    guarantee->level = WYN_STABILITY_DEPRECATED;
    guarantee->deprecation_reason = strdup(reason);
    guarantee->migration_guide = strdup(migration_guide);
    guarantee->expiry_timestamp = time(NULL) + (365 * 24 * 60 * 60); // 1 year from now
    
    return true;
}

// Security audit functions
WynSecurityAudit* wyn_security_audit_new(void) {
    WynSecurityAudit* audit = malloc(sizeof(WynSecurityAudit));
    if (!audit) return NULL;
    
    audit->vulnerabilities = NULL;
    audit->vulnerability_count = 0;
    audit->audit_timestamp = time(NULL);
    audit->auditor = strdup("Wyn Security Team");
    audit->security_score = 0.0;
    audit->passed_audit = false;
    
    return audit;
}

void wyn_security_audit_free(WynSecurityAudit* audit) {
    if (!audit) return;
    
    for (size_t i = 0; i < audit->vulnerability_count; i++) {
        free(audit->vulnerabilities[i].vulnerability_id);
        free(audit->vulnerabilities[i].description);
        free(audit->vulnerabilities[i].affected_versions);
        free(audit->vulnerabilities[i].fixed_version);
        free(audit->vulnerabilities[i].mitigation_steps);
    }
    free(audit->vulnerabilities);
    free(audit->auditor);
    free(audit);
}

bool wyn_security_audit_run(WynSecurityAudit* audit, const char* codebase_path) {
    if (!audit || !codebase_path) return false;
    
    // Simulate security audit
    audit->audit_timestamp = time(NULL);
    audit->security_score = wyn_security_audit_calculate_score(audit);
    audit->passed_audit = (audit->security_score >= 8.0);
    
    return true;
}

bool wyn_security_audit_add_vulnerability(WynSecurityAudit* audit, const char* id, const char* description, WynVulnerabilityLevel level) {
    if (!audit || !id || !description) return false;
    
    WynSecurityVulnerability* new_vulns = realloc(audit->vulnerabilities,
        (audit->vulnerability_count + 1) * sizeof(WynSecurityVulnerability));
    if (!new_vulns) return false;
    
    audit->vulnerabilities = new_vulns;
    WynSecurityVulnerability* vuln = &audit->vulnerabilities[audit->vulnerability_count];
    
    vuln->vulnerability_id = strdup(id);
    vuln->description = strdup(description);
    vuln->level = level;
    vuln->affected_versions = strdup("*");
    vuln->fixed_version = NULL;
    vuln->mitigation_steps = NULL;
    vuln->discovered_timestamp = time(NULL);
    vuln->fixed_timestamp = 0;
    vuln->is_public = false;
    
    audit->vulnerability_count++;
    return true;
}

bool wyn_security_audit_fix_vulnerability(WynSecurityAudit* audit, const char* vulnerability_id, const char* fixed_version) {
    if (!audit || !vulnerability_id || !fixed_version) return false;
    
    for (size_t i = 0; i < audit->vulnerability_count; i++) {
        if (strcmp(audit->vulnerabilities[i].vulnerability_id, vulnerability_id) == 0) {
            free(audit->vulnerabilities[i].fixed_version);
            audit->vulnerabilities[i].fixed_version = strdup(fixed_version);
            audit->vulnerabilities[i].fixed_timestamp = time(NULL);
            return true;
        }
    }
    
    return false;
}

double wyn_security_audit_calculate_score(WynSecurityAudit* audit) {
    if (!audit) return 0.0;
    
    double score = 10.0;
    
    for (size_t i = 0; i < audit->vulnerability_count; i++) {
        WynSecurityVulnerability* vuln = &audit->vulnerabilities[i];
        
        if (vuln->fixed_timestamp == 0) { // Unfixed vulnerability
            switch (vuln->level) {
                case WYN_VULN_CRITICAL: score -= 3.0; break;
                case WYN_VULN_HIGH: score -= 2.0; break;
                case WYN_VULN_MEDIUM: score -= 1.0; break;
                case WYN_VULN_LOW: score -= 0.5; break;
            }
        }
    }
    
    return score < 0.0 ? 0.0 : score;
}
// Long-term support functions
WynLTSPlan* wyn_lts_plan_new(WynSemanticVersion* version, WynSupportTier tier) {
    WynLTSPlan* plan = malloc(sizeof(WynLTSPlan));
    if (!plan) return NULL;
    
    plan->version = version;
    plan->release_date = time(NULL);
    plan->end_of_life_date = plan->release_date + (3 * 365 * 24 * 60 * 60); // 3 years
    plan->support_tier = tier;
    plan->support_contact = strdup("support@wyn-lang.org");
    plan->security_updates_only = false;
    
    return plan;
}

void wyn_lts_plan_free(WynLTSPlan* plan) {
    if (!plan) return;
    
    if (plan->version) {
        wyn_semantic_version_free(plan->version);
    }
    free(plan->support_contact);
    free(plan);
}

bool wyn_lts_plan_add(WynProductionManager* manager, WynLTSPlan* plan) {
    if (!manager || !plan) return false;
    
    WynLTSPlan* new_plans = realloc(manager->lts_plans,
        (manager->lts_plan_count + 1) * sizeof(WynLTSPlan));
    if (!new_plans) return false;
    
    manager->lts_plans = new_plans;
    // Copy the plan data, not the pointer
    manager->lts_plans[manager->lts_plan_count].version = plan->version;
    manager->lts_plans[manager->lts_plan_count].release_date = plan->release_date;
    manager->lts_plans[manager->lts_plan_count].end_of_life_date = plan->end_of_life_date;
    manager->lts_plans[manager->lts_plan_count].support_tier = plan->support_tier;
    manager->lts_plans[manager->lts_plan_count].support_contact = strdup(plan->support_contact);
    manager->lts_plans[manager->lts_plan_count].security_updates_only = plan->security_updates_only;
    manager->lts_plan_count++;
    
    return true;
}

WynLTSPlan* wyn_lts_plan_find_current(WynProductionManager* manager) {
    if (!manager) return NULL;
    
    uint64_t current_time = time(NULL);
    
    for (size_t i = 0; i < manager->lts_plan_count; i++) {
        WynLTSPlan* plan = &manager->lts_plans[i];
        if (plan->release_date <= current_time && plan->end_of_life_date > current_time) {
            return plan;
        }
    }
    
    return NULL;
}

bool wyn_lts_plan_is_supported(WynLTSPlan* plan, uint64_t current_timestamp) {
    if (!plan) return false;
    
    return current_timestamp >= plan->release_date && current_timestamp < plan->end_of_life_date;
}

// Enterprise features
WynEnterpriseFeatures* wyn_enterprise_features_new(void) {
    WynEnterpriseFeatures* features = malloc(sizeof(WynEnterpriseFeatures));
    if (!features) return NULL;
    
    features->commercial_license_available = false;
    features->priority_support = false;
    features->custom_sla = false;
    features->on_premise_deployment = false;
    features->compliance_certifications = false;
    features->supported_standards = NULL;
    features->standard_count = 0;
    
    return features;
}

void wyn_enterprise_features_free(WynEnterpriseFeatures* features) {
    if (!features) return;
    
    for (size_t i = 0; i < features->standard_count; i++) {
        free(features->supported_standards[i]);
    }
    free(features->supported_standards);
    free(features);
}

bool wyn_enterprise_features_enable_commercial_license(WynEnterpriseFeatures* features) {
    if (!features) return false;
    
    features->commercial_license_available = true;
    features->priority_support = true;
    features->custom_sla = true;
    features->on_premise_deployment = true;
    
    return true;
}

bool wyn_enterprise_features_add_compliance_standard(WynEnterpriseFeatures* features, const char* standard) {
    if (!features || !standard) return false;
    
    char** new_standards = realloc(features->supported_standards,
        (features->standard_count + 1) * sizeof(char*));
    if (!new_standards) return false;
    
    features->supported_standards = new_standards;
    features->supported_standards[features->standard_count] = strdup(standard);
    features->standard_count++;
    features->compliance_certifications = true;
    
    return true;
}

bool wyn_enterprise_features_configure_sla(WynEnterpriseFeatures* features, const char* sla_terms) {
    if (!features || !sla_terms) return false;
    
    features->custom_sla = true;
    return true;
}

// Utility functions
const char* wyn_stability_level_name(WynStabilityLevel level) {
    switch (level) {
        case WYN_STABILITY_EXPERIMENTAL: return "Experimental";
        case WYN_STABILITY_UNSTABLE: return "Unstable";
        case WYN_STABILITY_STABLE: return "Stable";
        case WYN_STABILITY_DEPRECATED: return "Deprecated";
        case WYN_STABILITY_FROZEN: return "Frozen";
        default: return "Unknown";
    }
}

const char* wyn_support_tier_name(WynSupportTier tier) {
    switch (tier) {
        case WYN_SUPPORT_COMMUNITY: return "Community";
        case WYN_SUPPORT_STANDARD: return "Standard";
        case WYN_SUPPORT_EXTENDED: return "Extended";
        case WYN_SUPPORT_ENTERPRISE: return "Enterprise";
        default: return "Unknown";
    }
}

const char* wyn_vulnerability_level_name(WynVulnerabilityLevel level) {
    switch (level) {
        case WYN_VULN_LOW: return "Low";
        case WYN_VULN_MEDIUM: return "Medium";
        case WYN_VULN_HIGH: return "High";
        case WYN_VULN_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

bool wyn_is_production_ready(WynProductionManager* manager) {
    return manager && manager->production_ready;
}
