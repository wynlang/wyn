#include "../src/tooling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void demonstrate_ide_integration() {
    printf("=== IDE Integration Setup ===\n");
    
    WynToolingManager* manager = wyn_tooling_manager_new();
    wyn_tooling_manager_initialize(manager, "/workspace/wyn-project");
    
    printf("Setting up IDE integrations for Wyn development...\n\n");
    
    // VS Code integration
    WynIDEIntegration* vscode = wyn_ide_integration_new(WYN_IDE_VSCODE, "wyn-vscode");
    wyn_ide_integration_install(vscode);
    wyn_ide_integration_enable(vscode);
    wyn_tooling_manager_add_ide_integration(manager, vscode);
    
    printf("VS Code Features:\n");
    for (size_t i = 0; i < vscode->feature_count; i++) {
        printf("  - %s\n", vscode->supported_features[i]);
    }
    printf("\n");
    
    // IntelliJ integration
    WynIDEIntegration* intellij = wyn_ide_integration_new(WYN_IDE_INTELLIJ, "wyn-intellij");
    wyn_ide_integration_install(intellij);
    wyn_ide_integration_enable(intellij);
    wyn_tooling_manager_add_ide_integration(manager, intellij);
    
    // Vim integration
    WynIDEIntegration* vim = wyn_ide_integration_new(WYN_IDE_VIM, "wyn-vim");
    wyn_ide_integration_install(vim);
    wyn_ide_integration_enable(vim);
    wyn_tooling_manager_add_ide_integration(manager, vim);
    
    printf("📊 IDE Integration Summary:\n");
    printf("Total integrations: %zu\n", manager->ide_count);
    for (size_t i = 0; i < manager->ide_count; i++) {
        WynIDEIntegration* integration = &manager->ide_integrations[i];
        printf("- %s: %s (v%s) - %zu features\n", 
               wyn_ide_type_name(integration->ide_type),
               integration->plugin_name,
               integration->plugin_version,
               integration->feature_count);
    }
    
    wyn_ide_integration_free(vscode);
    wyn_ide_integration_free(intellij);
    wyn_ide_integration_free(vim);
    wyn_tooling_manager_free(manager);
    printf("\n");
}

void demonstrate_debugging_workflow() {
    printf("=== Debugging Workflow ===\n");
    
    WynDebugger* debugger = wyn_debugger_new();
    
    printf("Starting debugging session...\n");
    
    // Attach to program
    wyn_debugger_attach(debugger, "my_wyn_program");
    
    // Set breakpoints
    printf("\nSetting breakpoints:\n");
    wyn_debugger_set_breakpoint(debugger, "main.wyn", 15);
    wyn_debugger_set_breakpoint(debugger, "utils.wyn", 42);
    wyn_debugger_set_breakpoint(debugger, "network.wyn", 128);
    
    printf("Active breakpoints: %zu\n", debugger->breakpoint_count);
    for (size_t i = 0; i < debugger->breakpoint_count; i++) {
        printf("  %zu. %s:%zu\n", i+1, 
               debugger->breakpoints[i].file_path,
               debugger->breakpoints[i].line_number);
    }
    
    // Debugging operations
    printf("\nDebugging operations:\n");
    wyn_debugger_continue(debugger);
    wyn_debugger_step_over(debugger);
    
    printf("Debug session status: %s\n", 
           debugger->is_running ? "Running" : "Paused");
    
    // Detach
    wyn_debugger_detach(debugger);
    
    wyn_debugger_free(debugger);
    printf("\n");
}

void demonstrate_performance_profiling() {
    printf("=== Performance Profiling ===\n");
    
    WynProfiler* profiler = wyn_profiler_new();
    
    printf("Starting performance profiling...\n");
    
    // Start profiling
    wyn_profiler_start(profiler, "my_wyn_program");
    
    // Simulate program execution
    printf("Program execution in progress...\n");
    
    // Stop profiling
    wyn_profiler_stop(profiler);
    
    // Analyze results
    wyn_profiler_analyze(profiler);
    
    printf("\n📈 Performance Analysis Results:\n");
    printf("Total execution time: %.2f seconds\n", profiler->total_execution_time);
    
    size_t count;
    WynProfileData* hotspots = wyn_profiler_get_hotspots(profiler, &count);
    
    printf("\nHotspot Analysis:\n");
    printf("%-20s %10s %12s %12s %10s %12s\n", 
           "Function", "Calls", "Total (s)", "Avg (s)", "Percent", "Memory (B)");
    printf("%-20s %10s %12s %12s %10s %12s\n", 
           "--------", "-----", "---------", "-------", "-------", "----------");
    
    for (size_t i = 0; i < count; i++) {
        printf("%-20s %10llu %12.3f %12.6f %9.1f%% %12zu\n",
               hotspots[i].function_name,
               (unsigned long long)hotspots[i].call_count,
               hotspots[i].total_time,
               hotspots[i].average_time,
               hotspots[i].percentage,
               hotspots[i].memory_usage);
    }
    
    wyn_profiler_free(profiler);
    printf("\n");
}

void demonstrate_code_analysis() {
    printf("=== Code Analysis and Linting ===\n");
    
    WynToolingManager* manager = wyn_tooling_manager_new();
    wyn_tooling_manager_initialize(manager, "/workspace");
    
    printf("Running code analysis...\n");
    
    // Analyze multiple files
    wyn_analyze_code(manager, "src/main.wyn");
    wyn_analyze_code(manager, "src/utils.wyn");
    wyn_analyze_code(manager, "src/network.wyn");
    
    // Get analysis results
    size_t count;
    WynAnalysisResult* results = wyn_get_analysis_results(manager, &count);
    
    printf("\n🔍 Analysis Results Summary:\n");
    printf("Total issues found: %zu\n\n", count);
    
    // Group by severity
    size_t warnings = 0, errors = 0, info = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(results[i].severity, "warning") == 0) warnings++;
        else if (strcmp(results[i].severity, "error") == 0) errors++;
        else if (strcmp(results[i].severity, "info") == 0) info++;
    }
    
    printf("Issue breakdown:\n");
    printf("  Errors: %zu\n", errors);
    printf("  Warnings: %zu\n", warnings);
    printf("  Info: %zu\n", info);
    
    printf("\nDetailed Issues:\n");
    for (size_t i = 0; i < count && i < 5; i++) { // Show first 5 issues
        printf("%zu. [%s] %s:%zu - %s\n",
               i+1,
               results[i].severity,
               results[i].file_path,
               results[i].line_number,
               results[i].message);
        if (results[i].suggestion) {
            printf("   💡 Suggestion: %s\n", results[i].suggestion);
        }
    }
    
    if (count > 5) {
        printf("   ... and %zu more issues\n", count - 5);
    }
    
    wyn_tooling_manager_free(manager);
    printf("\n");
}

void demonstrate_language_server() {
    printf("=== Language Server Protocol ===\n");
    
    WynLanguageServer* server = wyn_language_server_new();
    
    printf("Language Server Configuration:\n");
    printf("  Executable: %s\n", server->server_executable);
    printf("  Port: %d\n", server->port);
    printf("  Status: %s\n", server->is_running ? "Running" : "Stopped");
    
    // Start language server
    wyn_language_server_start(server, "/workspace/wyn-project");
    
    printf("\nLanguage Server Features:\n");
    printf("  ✓ Code completion and IntelliSense\n");
    printf("  ✓ Go to definition and references\n");
    printf("  ✓ Real-time error checking\n");
    printf("  ✓ Hover information and documentation\n");
    printf("  ✓ Symbol search and navigation\n");
    printf("  ✓ Refactoring support\n");
    printf("  ✓ Code formatting\n");
    printf("  ✓ Signature help\n");
    
    // Stop language server
    wyn_language_server_stop(server);
    
    wyn_language_server_free(server);
    printf("\n");
}

void demonstrate_development_environment() {
    printf("=== Complete Development Environment ===\n");
    
    printf("Setting up complete Wyn development environment...\n\n");
    
    // Setup full environment
    wyn_setup_development_environment("/workspace/wyn-project");
    
    printf("\n🛠️ Available Tools:\n");
    
    WynToolType tools[] = {
        WYN_TOOL_COMPILER,
        WYN_TOOL_DEBUGGER,
        WYN_TOOL_PROFILER,
        WYN_TOOL_FORMATTER,
        WYN_TOOL_LINTER,
        WYN_TOOL_ANALYZER,
        WYN_TOOL_TESTER
    };
    
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        printf("  %s %s: %s\n",
               wyn_is_tool_available(tools[i]) ? "✓" : "✗",
               wyn_tool_type_name(tools[i]),
               wyn_is_tool_available(tools[i]) ? "Available" : "Not Available");
    }
    
    printf("\n📝 Quick Start Commands:\n");
    printf("  wyn build                 # Compile project\n");
    printf("  wyn test                  # Run tests\n");
    printf("  wyn fmt                   # Format code\n");
    printf("  wyn lint                  # Run linter\n");
    printf("  wyn debug program         # Start debugger\n");
    printf("  wyn profile program       # Profile performance\n");
    printf("  wyn doc                   # Generate documentation\n");
    printf("  wyn lsp                   # Start language server\n");
    
    printf("\n🎯 Development Workflow:\n");
    printf("1. Write code in your favorite IDE with Wyn plugin\n");
    printf("2. Get real-time error checking and completion\n");
    printf("3. Format code automatically on save\n");
    printf("4. Run tests with coverage reporting\n");
    printf("5. Debug with breakpoints and variable inspection\n");
    printf("6. Profile performance and optimize hotspots\n");
    printf("7. Generate documentation automatically\n");
    printf("8. Deploy with confidence\n");
    
    printf("\n");
}

int main() {
    printf("Wyn Language Advanced Tooling Integration Examples\n");
    printf("==================================================\n\n");
    
    demonstrate_ide_integration();
    demonstrate_debugging_workflow();
    demonstrate_performance_profiling();
    demonstrate_code_analysis();
    demonstrate_language_server();
    demonstrate_development_environment();
    
    printf("🎉 Wyn Language tooling ecosystem is ready for professional development!\n");
    
    return 0;
}
