#include "test.h"
#include "production_readiness.h"
#include <stdio.h>

static int test_production_deployment() {
    WynProductionDeployment* deployment = wyn_production_deployment_new("test-app", "production", "1.0.0");
    
    bool validated = wyn_production_validate_deployment(deployment);
    if (!validated) {
        wyn_production_deployment_free(deployment);
        return 0;
    }
    
    bool deployed = wyn_production_deploy(deployment);
    if (!deployed) {
        wyn_production_deployment_free(deployment);
        return 0;
    }
    
    bool scaled = wyn_production_scale(deployment, 3);
    if (!scaled) {
        wyn_production_deployment_free(deployment);
        return 0;
    }
    
    wyn_production_deployment_free(deployment);
    return 1;
}

static int test_production_monitoring() {
    WynProductionMonitor* monitor = wyn_production_monitor_new("https://alerts.example.com");
    
    WynProductionMetric* metric1 = wyn_production_metric_new("cpu_usage", 45.0, "%");
    WynProductionMetric* metric2 = wyn_production_metric_new("memory_usage", 60.0, "%");
    
    bool added1 = wyn_production_monitor_add_metric(monitor, metric1);
    bool added2 = wyn_production_monitor_add_metric(monitor, metric2);
    
    if (!added1 || !added2) {
        wyn_production_monitor_free(monitor);
        return 0;
    }
    
    wyn_production_monitor_report(monitor);
    wyn_production_monitor_free(monitor);
    return 1;
}

static int test_production_health_check() {
    bool health_ok = wyn_production_health_check();
    return health_ok ? 1 : 0;
}

static int test_production_stats() {
    wyn_production_reset_stats();
    wyn_production_print_stats();
    return 1;
}

static int test_final_production_readiness() {
    printf("\n=== RUNNING FINAL PRODUCTION READINESS CHECK ===\n");
    bool ready = wyn_final_production_readiness_check();
    return ready ? 1 : 0;
}

int main() {
    int total = 0, passed = 0;
    
    printf("=== Final Production Readiness Tests ===\n");
    
    total++; if (test_production_deployment()) { printf("✓ Production deployment\n"); passed++; } else printf("✗ Production deployment\n");
    total++; if (test_production_monitoring()) { printf("✓ Production monitoring\n"); passed++; } else printf("✗ Production monitoring\n");
    total++; if (test_production_health_check()) { printf("✓ Health checks\n"); passed++; } else printf("✗ Health checks\n");
    total++; if (test_production_stats()) { printf("✓ Production statistics\n"); passed++; } else printf("✗ Production statistics\n");
    total++; if (test_final_production_readiness()) { printf("✓ Final readiness validation\n"); passed++; } else printf("✗ Final readiness validation\n");
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All production readiness tests passed!\n");
        printf("🎉 WYN LANGUAGE IS PRODUCTION READY!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}
