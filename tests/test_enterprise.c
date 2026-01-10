#include "test.h"
#include "enterprise.h"
#include <stdio.h>
#include <string.h>

static int test_enterprise_licensing() {
    WynEnterpriseLicense* license = wyn_enterprise_license_new("ENT-12345-ABCDE", "Acme Corp");
    
    bool valid = wyn_enterprise_validate_license(license);
    if (!valid) {
        wyn_enterprise_license_free(license);
        return 0;
    }
    
    wyn_enterprise_license_free(license);
    return 1;
}

static int test_compliance_management() {
    WynComplianceManager* manager = wyn_compliance_manager_new("SOC2");
    
    bool access_granted = wyn_compliance_validate_access(manager, "admin", "sensitive_data");
    if (!access_granted) {
        wyn_compliance_manager_free(manager);
        return 0;
    }
    
    wyn_compliance_log_event(manager, "DATA_ACCESS", "admin");
    
    wyn_compliance_manager_free(manager);
    return 1;
}

static int test_system_monitoring() {
    WynSystemMetrics* metrics = wyn_monitoring_get_metrics();
    if (!metrics) return 0;
    
    if (metrics->cpu_usage < 0 || metrics->cpu_usage > 100) {
        wyn_monitoring_free_metrics(metrics);
        return 0;
    }
    
    wyn_monitoring_log_metrics(metrics, "/tmp/wyn_metrics.log");
    
    wyn_monitoring_free_metrics(metrics);
    return 1;
}

static int test_feature_management() {
    WynEnterpriseFeature* feature = wyn_enterprise_feature_new("advanced_analytics", "Advanced data analytics");
    
    if (!wyn_enterprise_feature_is_enabled(feature)) {
        wyn_enterprise_feature_free(feature);
        return 0;
    }
    
    wyn_enterprise_feature_toggle(feature, false);
    if (wyn_enterprise_feature_is_enabled(feature)) {
        wyn_enterprise_feature_free(feature);
        return 0;
    }
    
    wyn_enterprise_feature_free(feature);
    return 1;
}

static int test_enterprise_stats() {
    wyn_enterprise_reset_stats();
    wyn_enterprise_print_stats();
    return 1;
}

int main() {
    int total = 0, passed = 0;
    
    printf("=== Enterprise Features Tests ===\n");
    
    total++; if (test_enterprise_licensing()) { printf("✓ Enterprise licensing\n"); passed++; } else printf("✗ Enterprise licensing\n");
    total++; if (test_compliance_management()) { printf("✓ Compliance management\n"); passed++; } else printf("✗ Compliance management\n");
    total++; if (test_system_monitoring()) { printf("✓ System monitoring\n"); passed++; } else printf("✗ System monitoring\n");
    total++; if (test_feature_management()) { printf("✓ Feature management\n"); passed++; } else printf("✗ Feature management\n");
    total++; if (test_enterprise_stats()) { printf("✓ Enterprise statistics\n"); passed++; } else printf("✗ Enterprise statistics\n");
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All enterprise features tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}
