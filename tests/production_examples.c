#include "../src/production.h"
#include <stdio.h>
#include <stdlib.h>

void demonstrate_semantic_versioning() {
    printf("=== Semantic Versioning Example ===\n");
    
    // Create and parse versions
    WynSemanticVersion* current = wyn_semantic_version_new(1, 2, 3);
    WynSemanticVersion* required = wyn_semantic_version_new(0, 0, 0);
    wyn_semantic_version_parse("1.1.0", required);
    
    // Check compatibility
    if (wyn_semantic_version_is_compatible(required, current)) {
        printf("✓ Version %d.%d.%d is compatible with requirement %d.%d.%d\n",
               current->major, current->minor, current->patch,
               required->major, required->minor, required->patch);
    }
    
    // Compare versions
    int comparison = wyn_semantic_version_compare(current, required);
    printf("Version comparison result: %d (positive means current > required)\n", comparison);
    
    wyn_semantic_version_free(current);
    wyn_semantic_version_free(required);
    printf("\n");
}

void demonstrate_stability_guarantees() {
    printf("=== Stability Guarantees Example ===\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    wyn_production_manager_initialize(manager, "1.0.0");
    
    // Add stable API guarantee
    WynStabilityGuarantee* core_guarantee = wyn_stability_guarantee_new("wyn_core_api", WYN_STABILITY_STABLE);
    wyn_stability_guarantee_add(manager, core_guarantee);
    printf("✓ Added stability guarantee for wyn_core_api: %s\n", 
           wyn_stability_level_name(core_guarantee->level));
    
    // Add experimental API
    WynStabilityGuarantee* exp_guarantee = wyn_stability_guarantee_new("wyn_experimental_feature", WYN_STABILITY_EXPERIMENTAL);
    wyn_stability_guarantee_add(manager, exp_guarantee);
    printf("✓ Added stability guarantee for wyn_experimental_feature: %s\n", 
           wyn_stability_level_name(exp_guarantee->level));
    
    // Deprecate an API
    WynStabilityGuarantee* old_api = wyn_stability_guarantee_new("wyn_old_api", WYN_STABILITY_STABLE);
    wyn_stability_guarantee_add(manager, old_api);
    
    WynStabilityGuarantee* found = wyn_stability_guarantee_find(manager, "wyn_old_api");
    if (found) {
        wyn_stability_guarantee_deprecate(found, "Replaced by wyn_new_api", "Use wyn_new_api() instead of wyn_old_api()");
        printf("✓ Deprecated wyn_old_api: %s\n", found->deprecation_reason);
        printf("  Migration guide: %s\n", found->migration_guide);
    }
    
    printf("Total stability guarantees: %zu\n", manager->guarantee_count);
    
    wyn_production_manager_free(manager);
    printf("\n");
}

void demonstrate_security_audit() {
    printf("=== Security Audit Example ===\n");
    
    WynSecurityAudit* audit = wyn_security_audit_new();
    
    // Simulate discovering vulnerabilities
    wyn_security_audit_add_vulnerability(audit, "WYN-2024-001", 
        "Buffer overflow in string parsing", WYN_VULN_HIGH);
    wyn_security_audit_add_vulnerability(audit, "WYN-2024-002", 
        "Memory leak in garbage collector", WYN_VULN_MEDIUM);
    wyn_security_audit_add_vulnerability(audit, "WYN-2024-003", 
        "Information disclosure in debug mode", WYN_VULN_LOW);
    
    printf("Discovered %zu vulnerabilities\n", audit->vulnerability_count);
    
    // Calculate initial security score
    double initial_score = wyn_security_audit_calculate_score(audit);
    printf("Initial security score: %.1f/10.0\n", initial_score);
    
    // Fix critical and high vulnerabilities
    wyn_security_audit_fix_vulnerability(audit, "WYN-2024-001", "1.0.1");
    wyn_security_audit_fix_vulnerability(audit, "WYN-2024-002", "1.0.2");
    
    // Recalculate score
    double fixed_score = wyn_security_audit_calculate_score(audit);
    printf("Security score after fixes: %.1f/10.0\n", fixed_score);
    
    // Run full audit
    wyn_security_audit_run(audit, "/path/to/wyn/codebase");
    printf("Audit completed. Passed: %s\n", audit->passed_audit ? "Yes" : "No");
    
    wyn_security_audit_free(audit);
    printf("\n");
}

void demonstrate_lts_support() {
    printf("=== Long-Term Support Example ===\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    
    // Create LTS plans for different versions
    WynSemanticVersion* v1_0 = wyn_semantic_version_new(1, 0, 0);
    WynLTSPlan* lts_v1 = wyn_lts_plan_new(v1_0, WYN_SUPPORT_EXTENDED);
    wyn_lts_plan_add(manager, lts_v1);
    
    WynSemanticVersion* v2_0 = wyn_semantic_version_new(2, 0, 0);
    WynLTSPlan* lts_v2 = wyn_lts_plan_new(v2_0, WYN_SUPPORT_ENTERPRISE);
    wyn_lts_plan_add(manager, lts_v2);
    
    printf("Created LTS plans:\n");
    for (size_t i = 0; i < manager->lts_plan_count; i++) {
        WynLTSPlan* plan = &manager->lts_plans[i];
        printf("- Version %d.%d.%d: %s support\n",
               plan->version->major, plan->version->minor, plan->version->patch,
               wyn_support_tier_name(plan->support_tier));
    }
    
    // Find current LTS
    WynLTSPlan* current_lts = wyn_lts_plan_find_current(manager);
    if (current_lts) {
        printf("Current LTS version: %d.%d.%d (%s)\n",
               current_lts->version->major, current_lts->version->minor, current_lts->version->patch,
               wyn_support_tier_name(current_lts->support_tier));
    }
    
    wyn_production_manager_free(manager);
    printf("\n");
}

void demonstrate_enterprise_features() {
    printf("=== Enterprise Features Example ===\n");
    
    WynEnterpriseFeatures* features = wyn_enterprise_features_new();
    
    // Enable commercial licensing
    wyn_enterprise_features_enable_commercial_license(features);
    printf("✓ Commercial license enabled\n");
    printf("✓ Priority support: %s\n", features->priority_support ? "Yes" : "No");
    printf("✓ Custom SLA: %s\n", features->custom_sla ? "Yes" : "No");
    printf("✓ On-premise deployment: %s\n", features->on_premise_deployment ? "Yes" : "No");
    
    // Add compliance standards
    wyn_enterprise_features_add_compliance_standard(features, "SOC 2 Type II");
    wyn_enterprise_features_add_compliance_standard(features, "ISO 27001");
    wyn_enterprise_features_add_compliance_standard(features, "GDPR Compliant");
    
    printf("Compliance certifications:\n");
    for (size_t i = 0; i < features->standard_count; i++) {
        printf("- %s\n", features->supported_standards[i]);
    }
    
    wyn_enterprise_features_free(features);
    printf("\n");
}

void demonstrate_production_readiness() {
    printf("=== Production Readiness Validation ===\n");
    
    WynProductionManager* manager = wyn_production_manager_new();
    
    // Initialize with production version
    wyn_production_manager_initialize(manager, "1.0.0");
    printf("Initialized with version 1.0.0\n");
    
    // Add required stability guarantees
    WynStabilityGuarantee* core_guarantee = wyn_stability_guarantee_new("wyn_core", WYN_STABILITY_STABLE);
    wyn_stability_guarantee_add(manager, core_guarantee);
    
    WynStabilityGuarantee* stdlib_guarantee = wyn_stability_guarantee_new("wyn_stdlib", WYN_STABILITY_STABLE);
    wyn_stability_guarantee_add(manager, stdlib_guarantee);
    
    // Set up security audit
    manager->security_audit->passed_audit = true;
    manager->security_audit->security_score = 9.2;
    
    // Add LTS plan
    WynSemanticVersion* version = wyn_semantic_version_new(1, 0, 0);
    WynLTSPlan* lts = wyn_lts_plan_new(version, WYN_SUPPORT_ENTERPRISE);
    wyn_lts_plan_add(manager, lts);
    
    // Enable enterprise features
    wyn_enterprise_features_enable_commercial_license(manager->enterprise_features);
    
    // Validate production readiness
    bool ready = wyn_production_manager_validate_readiness(manager);
    printf("Production readiness check: %s\n", ready ? "PASSED ✓" : "FAILED ✗");
    
    if (ready) {
        printf("✓ Version 1.0.0 or higher\n");
        printf("✓ Security audit passed (score: %.1f)\n", manager->security_audit->security_score);
        printf("✓ %zu stability guarantees in place\n", manager->guarantee_count);
        printf("✓ LTS support available\n");
        printf("✓ Enterprise features configured\n");
        printf("\n🎉 Wyn language is PRODUCTION READY!\n");
    }
    
    wyn_production_manager_free(manager);
    printf("\n");
}

int main() {
    printf("Wyn Language Production Readiness and Stability Examples\n");
    printf("========================================================\n\n");
    
    demonstrate_semantic_versioning();
    demonstrate_stability_guarantees();
    demonstrate_security_audit();
    demonstrate_lts_support();
    demonstrate_enterprise_features();
    demonstrate_production_readiness();
    
    return 0;
}
