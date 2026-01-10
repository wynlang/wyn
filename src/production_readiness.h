#ifndef WYN_PRODUCTION_READINESS_H
#define WYN_PRODUCTION_READINESS_H

#include <stdbool.h>
#include <stddef.h>

// T9.3.1: Final Production Readiness - Deployment, Monitoring, Validation

// Production Deployment
typedef struct WynProductionDeployment WynProductionDeployment;
typedef struct WynProductionMetric WynProductionMetric;
typedef struct WynProductionMonitor WynProductionMonitor;

// Deployment Management
WynProductionDeployment* wyn_production_deployment_new(const char* name, const char* env, const char* version);
bool wyn_production_deploy(WynProductionDeployment* deployment);
bool wyn_production_scale(WynProductionDeployment* deployment, size_t new_instance_count);
void wyn_production_deployment_free(WynProductionDeployment* deployment);

// Production Monitoring
WynProductionMonitor* wyn_production_monitor_new(const char* alert_endpoint);
WynProductionMetric* wyn_production_metric_new(const char* name, double value, const char* unit);
bool wyn_production_monitor_add_metric(WynProductionMonitor* monitor, WynProductionMetric* metric);
void wyn_production_monitor_report(WynProductionMonitor* monitor);
void wyn_production_metric_free(WynProductionMetric* metric);
void wyn_production_monitor_free(WynProductionMonitor* monitor);

// Production Validation
bool wyn_production_validate_deployment(WynProductionDeployment* deployment);
bool wyn_production_health_check(void);

// Statistics
void wyn_production_print_stats(void);
void wyn_production_reset_stats(void);

// Final Readiness Check
bool wyn_final_production_readiness_check(void);

#endif
