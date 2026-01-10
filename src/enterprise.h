#ifndef WYN_ENTERPRISE_H
#define WYN_ENTERPRISE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// T9.2.1: Enterprise Features - Licensing, Compliance, Monitoring

// Enterprise License Management
typedef struct WynEnterpriseLicense WynEnterpriseLicense;
typedef struct WynEnterpriseFeature WynEnterpriseFeature;

WynEnterpriseLicense* wyn_enterprise_license_new(const char* key, const char* org);
bool wyn_enterprise_validate_license(WynEnterpriseLicense* license);
void wyn_enterprise_license_free(WynEnterpriseLicense* license);

// Compliance Management
typedef struct WynComplianceManager WynComplianceManager;

WynComplianceManager* wyn_compliance_manager_new(const char* standard);
void wyn_compliance_log_event(WynComplianceManager* manager, const char* event, const char* user);
bool wyn_compliance_validate_access(WynComplianceManager* manager, const char* user, const char* resource);
void wyn_compliance_manager_free(WynComplianceManager* manager);

// System Monitoring
typedef struct {
    double cpu_usage;
    size_t memory_usage;
    size_t disk_usage;
    int active_connections;
    time_t last_update;
} WynSystemMetrics;

WynSystemMetrics* wyn_monitoring_get_metrics(void);
void wyn_monitoring_log_metrics(WynSystemMetrics* metrics, const char* log_file);
void wyn_monitoring_free_metrics(WynSystemMetrics* metrics);

// Feature Management
WynEnterpriseFeature* wyn_enterprise_feature_new(const char* name, const char* description);
bool wyn_enterprise_feature_is_enabled(WynEnterpriseFeature* feature);
void wyn_enterprise_feature_toggle(WynEnterpriseFeature* feature, bool enabled);
void wyn_enterprise_feature_free(WynEnterpriseFeature* feature);

// Statistics
void wyn_enterprise_print_stats(void);
void wyn_enterprise_reset_stats(void);

#endif
