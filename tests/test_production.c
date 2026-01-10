#include "../src/production.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

void test_semantic_versioning() {
    printf("Testing semantic versioning...\n");
    
    // Test version creation
    WynSemanticVersion* v1 = wyn_semantic_version_new(1, 2, 3);
    assert(v1 != NULL);
    assert(v1->major == 1);
    assert(v1->minor == 2);
    assert(v1->patch == 3);
    
    // Test version parsing
    WynSemanticVersion* v2 = wyn_semantic_version_new(0, 0, 0);
    assert(wyn_semantic_version_parse("2.1.0", v2));
    assert(v2->major == 2);
    assert(v2->minor == 1);
    assert(v2->patch == 0);
    
    // Test version comparison
    assert(wyn_semantic_version_compare(v2, v1) > 0);
    assert(wyn_semantic_version_compare(v1, v2) < 0);
    
    // Test compatibility
    assert(wyn_semantic_version_is_compatible(v1, v2) == false); // Different major versions
    
    WynSemanticVersion* v3 = wyn_semantic_version_new(1, 3, 0);
    assert(wyn_semantic_version_is_compatible(v1, v3) == true); // Same major, higher minor
    
    wyn_semantic_version_free(v1);
    wyn_semantic_version_free(v2);
    wyn_semantic_version_free(v3);
    
    printf("✓ Semantic versioning tests passed\n");
}

void test_stability_guarantees() {
    printf("Testing stability guarantees...\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    assert(manager != NULL);
    
    // Create stability guarantee
    WynStabilityGuarantee* guarantee = wyn_stability_guarantee_new("core_api", WYN_STABILITY_STABLE);
    assert(guarantee != NULL);
    assert(strcmp(guarantee->api_name, "core_api") == 0);
    assert(guarantee->level == WYN_STABILITY_STABLE);
    
    // Add guarantee to manager
    assert(wyn_stability_guarantee_add(manager, guarantee));
    assert(manager->guarantee_count == 1);
    
    // Find guarantee
    WynStabilityGuarantee* found = wyn_stability_guarantee_find(manager, "core_api");
    assert(found != NULL);
    assert(strcmp(found->api_name, "core_api") == 0);
    
    // Test deprecation
    assert(wyn_stability_guarantee_deprecate(found, "Replaced by new API", "Use new_api() instead"));
    assert(found->level == WYN_STABILITY_DEPRECATED);
    assert(found->deprecation_reason != NULL);
    assert(found->migration_guide != NULL);
    
    wyn_production_manager_free(manager);
    
    printf("✓ Stability guarantee tests passed\n");
}

void test_security_audit() {
    printf("Testing security audit...\n");
    
    WynSecurityAudit* audit = wyn_security_audit_new();
    assert(audit != NULL);
    assert(audit->vulnerability_count == 0);
    assert(audit->passed_audit == false);
    
    // Add vulnerabilities
    assert(wyn_security_audit_add_vulnerability(audit, "CVE-2024-001", "Buffer overflow", WYN_VULN_HIGH));
    assert(wyn_security_audit_add_vulnerability(audit, "CVE-2024-002", "Memory leak", WYN_VULN_MEDIUM));
    assert(audit->vulnerability_count == 2);
    
    // Calculate initial score (should be low due to unfixed vulnerabilities)
    double initial_score = wyn_security_audit_calculate_score(audit);
    assert(initial_score < 8.0);
    
    // Fix vulnerabilities
    assert(wyn_security_audit_fix_vulnerability(audit, "CVE-2024-001", "1.0.1"));
    assert(wyn_security_audit_fix_vulnerability(audit, "CVE-2024-002", "1.0.1"));
    
    // Score should improve after fixes
    double fixed_score = wyn_security_audit_calculate_score(audit);
    assert(fixed_score > initial_score);
    
    // Run audit
    assert(wyn_security_audit_run(audit, "/fake/path"));
    
    wyn_security_audit_free(audit);
    
    printf("✓ Security audit tests passed\n");
}

void test_lts_plans() {
    printf("Testing LTS plans...\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    assert(manager != NULL);
    
    // Create LTS plan
    WynSemanticVersion* version = wyn_semantic_version_new(1, 0, 0);
    WynLTSPlan* plan = wyn_lts_plan_new(version, WYN_SUPPORT_ENTERPRISE);
    assert(plan != NULL);
    assert(plan->support_tier == WYN_SUPPORT_ENTERPRISE);
    
    // Add to manager
    assert(wyn_lts_plan_add(manager, plan));
    assert(manager->lts_plan_count == 1);
    
    // Test support validation
    uint64_t current_time = time(NULL);
    assert(wyn_lts_plan_is_supported(&manager->lts_plans[0], current_time));
    
    // Find current LTS plan
    WynLTSPlan* current = wyn_lts_plan_find_current(manager);
    assert(current != NULL);
    assert(current->support_tier == WYN_SUPPORT_ENTERPRISE);
    
    wyn_production_manager_free(manager);
    
    printf("✓ LTS plan tests passed\n");
}

void test_enterprise_features() {
    printf("Testing enterprise features...\n");
    
    WynEnterpriseFeatures* features = wyn_enterprise_features_new();
    assert(features != NULL);
    assert(features->commercial_license_available == false);
    
    // Enable commercial license
    assert(wyn_enterprise_features_enable_commercial_license(features));
    assert(features->commercial_license_available == true);
    assert(features->priority_support == true);
    
    // Add compliance standards
    assert(wyn_enterprise_features_add_compliance_standard(features, "SOC2"));
    assert(wyn_enterprise_features_add_compliance_standard(features, "ISO27001"));
    assert(features->standard_count == 2);
    assert(features->compliance_certifications == true);
    
    // Configure SLA
    assert(wyn_enterprise_features_configure_sla(features, "99.9% uptime"));
    assert(features->custom_sla == true);
    
    wyn_enterprise_features_free(features);
    
    printf("✓ Enterprise features tests passed\n");
}

void test_production_readiness() {
    printf("Testing production readiness validation...\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    assert(manager != NULL);
    
    // Initialize with version 1.0.0
    assert(wyn_production_manager_initialize(manager, "1.0.0"));
    assert(manager->current_version != NULL);
    assert(manager->current_version->major == 1);
    
    // Should not be ready initially (no guarantees)
    assert(!wyn_production_manager_validate_readiness(manager));
    
    // Add stability guarantee
    WynStabilityGuarantee* guarantee = wyn_stability_guarantee_new("core_api", WYN_STABILITY_STABLE);
    assert(wyn_stability_guarantee_add(manager, guarantee));
    
    // Set security audit as passed
    manager->security_audit->passed_audit = true;
    
    // Now should be ready
    assert(wyn_production_manager_validate_readiness(manager));
    assert(wyn_is_production_ready(manager));
    
    wyn_production_manager_free(manager);
    
    printf("✓ Production readiness tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test stability level names
    assert(strcmp(wyn_stability_level_name(WYN_STABILITY_STABLE), "Stable") == 0);
    assert(strcmp(wyn_stability_level_name(WYN_STABILITY_DEPRECATED), "Deprecated") == 0);
    
    // Test support tier names
    assert(strcmp(wyn_support_tier_name(WYN_SUPPORT_ENTERPRISE), "Enterprise") == 0);
    assert(strcmp(wyn_support_tier_name(WYN_SUPPORT_COMMUNITY), "Community") == 0);
    
    // Test vulnerability level names
    assert(strcmp(wyn_vulnerability_level_name(WYN_VULN_CRITICAL), "Critical") == 0);
    assert(strcmp(wyn_vulnerability_level_name(WYN_VULN_LOW), "Low") == 0);
    
    printf("✓ Utility function tests passed\n");
}

int main() {
    printf("Running Production Readiness and Stability Tests...\n\n");
    
    test_semantic_versioning();
    test_stability_guarantees();
    test_security_audit();
    test_lts_plans();
    test_enterprise_features();
    test_production_readiness();
    test_utility_functions();
    
    printf("\n🎉 All production readiness tests passed!\n");
    printf("Production system provides:\n");
    printf("- Semantic versioning with compatibility checks\n");
    printf("- API stability guarantees and deprecation management\n");
    printf("- Security audit framework with vulnerability tracking\n");
    printf("- Long-term support planning with multiple tiers\n");
    printf("- Enterprise features and compliance standards\n");
    printf("- Production readiness validation\n");
    
    return 0;
}
