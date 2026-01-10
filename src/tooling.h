#ifndef WYN_TOOLING_H
#define WYN_TOOLING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
typedef struct WynToolingManager WynToolingManager;
typedef struct WynIDEIntegration WynIDEIntegration;
typedef struct WynDebugger WynDebugger;
typedef struct WynProfiler WynProfiler;

// Tool types
typedef enum {
    WYN_TOOL_COMPILER,
    WYN_TOOL_DEBUGGER,
    WYN_TOOL_PROFILER,
    WYN_TOOL_FORMATTER,
    WYN_TOOL_LINTER,
    WYN_TOOL_ANALYZER,
    WYN_TOOL_REFACTOR,
    WYN_TOOL_TESTER
} WynToolType;

// IDE types
typedef enum {
    WYN_IDE_VSCODE,
    WYN_IDE_INTELLIJ,
    WYN_IDE_VIM,
    WYN_IDE_EMACS,
    WYN_IDE_SUBLIME,
    WYN_IDE_ATOM
} WynIDEType;

// Debug information
typedef struct {
    char* file_path;
    size_t line_number;
    size_t column;
    char* function_name;
    char* variable_name;
    char* variable_value;
    char* variable_type;
} WynDebugInfo;

// Profiling data
typedef struct {
    char* function_name;
    uint64_t call_count;
    double total_time;
    double average_time;
    double percentage;
    size_t memory_usage;
} WynProfileData;

// IDE integration
typedef struct WynIDEIntegration {
    WynIDEType ide_type;
    char* plugin_name;
    char* plugin_version;
    char* installation_path;
    bool is_installed;
    bool is_enabled;
    char** supported_features;
    size_t feature_count;
} WynIDEIntegration;

// Debugger
typedef struct WynDebugger {
    char* target_executable;
    WynDebugInfo* breakpoints;
    size_t breakpoint_count;
    WynDebugInfo* call_stack;
    size_t stack_depth;
    char** watched_variables;
    size_t watch_count;
    bool is_running;
    bool is_attached;
} WynDebugger;

// Profiler
typedef struct WynProfiler {
    char* target_executable;
    WynProfileData* profile_data;
    size_t data_count;
    uint64_t start_time;
    uint64_t end_time;
    double total_execution_time;
    size_t total_memory_usage;
    bool is_profiling;
} WynProfiler;

// Code analysis
typedef struct {
    char* file_path;
    size_t line_number;
    char* message;
    char* severity;
    char* rule_id;
    char* suggestion;
} WynAnalysisResult;

// Refactoring operation
typedef struct {
    char* operation_type;
    char* file_path;
    size_t start_line;
    size_t end_line;
    char* old_code;
    char* new_code;
    char* description;
} WynRefactorOperation;

// Tooling manager
typedef struct WynToolingManager {
    WynIDEIntegration* ide_integrations;
    size_t ide_count;
    WynDebugger* debugger;
    WynProfiler* profiler;
    WynAnalysisResult* analysis_results;
    size_t analysis_count;
    WynRefactorOperation* refactor_operations;
    size_t refactor_count;
    char* toolchain_path;
    char* workspace_path;
} WynToolingManager;

// Tooling manager functions
WynToolingManager* wyn_tooling_manager_new(void);
void wyn_tooling_manager_free(WynToolingManager* manager);
bool wyn_tooling_manager_initialize(WynToolingManager* manager, const char* workspace_path);
bool wyn_tooling_manager_setup_toolchain(WynToolingManager* manager);

// IDE integration functions
WynIDEIntegration* wyn_ide_integration_new(WynIDEType ide_type, const char* plugin_name);
void wyn_ide_integration_free(WynIDEIntegration* integration);
bool wyn_ide_integration_install(WynIDEIntegration* integration);
bool wyn_ide_integration_enable(WynIDEIntegration* integration);
bool wyn_ide_integration_add_feature(WynIDEIntegration* integration, const char* feature);
bool wyn_tooling_manager_add_ide_integration(WynToolingManager* manager, WynIDEIntegration* integration);

// Debugger functions
WynDebugger* wyn_debugger_new(void);
void wyn_debugger_free(WynDebugger* debugger);
bool wyn_debugger_attach(WynDebugger* debugger, const char* executable);
bool wyn_debugger_detach(WynDebugger* debugger);
bool wyn_debugger_set_breakpoint(WynDebugger* debugger, const char* file, size_t line);
bool wyn_debugger_remove_breakpoint(WynDebugger* debugger, const char* file, size_t line);
bool wyn_debugger_step_over(WynDebugger* debugger);
bool wyn_debugger_step_into(WynDebugger* debugger);
bool wyn_debugger_continue(WynDebugger* debugger);
bool wyn_debugger_add_watch(WynDebugger* debugger, const char* variable);
WynDebugInfo* wyn_debugger_get_current_state(WynDebugger* debugger);

// Profiler functions
WynProfiler* wyn_profiler_new(void);
void wyn_profiler_free(WynProfiler* profiler);
bool wyn_profiler_start(WynProfiler* profiler, const char* executable);
bool wyn_profiler_stop(WynProfiler* profiler);
bool wyn_profiler_analyze(WynProfiler* profiler);
WynProfileData* wyn_profiler_get_hotspots(WynProfiler* profiler, size_t* count);
bool wyn_profiler_export_report(WynProfiler* profiler, const char* output_file);

// Code analysis functions
bool wyn_analyze_code(WynToolingManager* manager, const char* file_path);
bool wyn_analyze_project(WynToolingManager* manager);
WynAnalysisResult* wyn_get_analysis_results(WynToolingManager* manager, size_t* count);
bool wyn_fix_analysis_issues(WynToolingManager* manager, bool auto_fix);

// Refactoring functions
bool wyn_refactor_rename(WynToolingManager* manager, const char* file_path, size_t line, size_t column, const char* new_name);
bool wyn_refactor_extract_function(WynToolingManager* manager, const char* file_path, size_t start_line, size_t end_line, const char* function_name);
bool wyn_refactor_inline_variable(WynToolingManager* manager, const char* file_path, size_t line, size_t column);
bool wyn_refactor_move_function(WynToolingManager* manager, const char* source_file, const char* target_file, const char* function_name);
bool wyn_apply_refactor_operations(WynToolingManager* manager);

// Code formatting functions
bool wyn_format_file(const char* file_path);
bool wyn_format_project(const char* project_path);
bool wyn_format_code_string(const char* code, char** formatted_code);

// Linting functions
typedef struct {
    char* rule_id;
    char* description;
    char* severity;
    bool is_enabled;
} WynLintRule;

bool wyn_lint_file(const char* file_path, WynAnalysisResult** results, size_t* count);
bool wyn_lint_project(const char* project_path, WynAnalysisResult** results, size_t* count);
bool wyn_configure_lint_rules(WynLintRule* rules, size_t rule_count);

// Build system integration
typedef struct {
    char* build_system;
    char* config_file;
    char* build_command;
    char* test_command;
    char* clean_command;
    bool is_configured;
} WynBuildIntegration;

WynBuildIntegration* wyn_build_integration_new(const char* build_system);
void wyn_build_integration_free(WynBuildIntegration* integration);
bool wyn_build_integration_configure(WynBuildIntegration* integration, const char* project_path);
bool wyn_build_integration_build(WynBuildIntegration* integration);
bool wyn_build_integration_test(WynBuildIntegration* integration);
bool wyn_build_integration_clean(WynBuildIntegration* integration);

// Language server functions
typedef struct {
    char* server_executable;
    int port;
    bool is_running;
    char* workspace_root;
} WynLanguageServer;

WynLanguageServer* wyn_language_server_new(void);
void wyn_language_server_free(WynLanguageServer* server);
bool wyn_language_server_start(WynLanguageServer* server, const char* workspace_root);
bool wyn_language_server_stop(WynLanguageServer* server);
bool wyn_language_server_restart(WynLanguageServer* server);

// Documentation generation
typedef struct {
    char* input_path;
    char* output_path;
    char* format;
    bool include_private;
    bool include_examples;
} WynDocGenConfig;

bool wyn_generate_documentation(WynDocGenConfig* config);
bool wyn_generate_api_docs(const char* source_path, const char* output_path);
bool wyn_generate_user_guide(const char* template_path, const char* output_path);

// Testing integration
typedef struct {
    char* test_framework;
    char* test_directory;
    char* test_pattern;
    bool run_parallel;
    bool generate_coverage;
} WynTestConfig;

bool wyn_run_tests(WynTestConfig* config);
bool wyn_generate_test_coverage(WynTestConfig* config, const char* output_file);
bool wyn_run_benchmarks(const char* benchmark_path);

// Utility functions
const char* wyn_tool_type_name(WynToolType type);
const char* wyn_ide_type_name(WynIDEType type);
bool wyn_is_tool_available(WynToolType type);
bool wyn_setup_development_environment(const char* project_path);

#endif // WYN_TOOLING_H
