#ifndef WYN_LLVM_PASSES_H
#define WYN_LLVM_PASSES_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct WynPassManager WynPassManager;
typedef struct WynOptimizationPass WynOptimizationPass;
typedef struct WynProfileData WynProfileData;

// Optimization pass types
typedef enum {
    WYN_PASS_DEAD_CODE_ELIMINATION,
    WYN_PASS_CONSTANT_FOLDING,
    WYN_PASS_FUNCTION_INLINING,
    WYN_PASS_LOOP_UNROLLING,
    WYN_PASS_VECTORIZATION,
    WYN_PASS_ESCAPE_ANALYSIS,
    WYN_PASS_TAIL_CALL_OPTIMIZATION,
    WYN_PASS_GLOBAL_VALUE_NUMBERING,
    WYN_PASS_LOOP_INVARIANT_CODE_MOTION,
    WYN_PASS_STRENGTH_REDUCTION,
    WYN_PASS_PROFILE_GUIDED_OPTIMIZATION,
    WYN_PASS_LINK_TIME_OPTIMIZATION
} WynPassType;

// Optimization levels
typedef enum {
    WYN_OPT_LEVEL_NONE,     // -O0
    WYN_OPT_LEVEL_LESS,     // -O1
    WYN_OPT_LEVEL_DEFAULT,  // -O2
    WYN_OPT_LEVEL_AGGRESSIVE, // -O3
    WYN_OPT_LEVEL_SIZE      // -Os
} WynOptLevel;

// Pass configuration
typedef struct {
    WynPassType type;
    bool enabled;
    int priority;
    void* config_data;
} WynPassConfig;

// Profile-guided optimization data
typedef struct WynProfileData {
    char* function_name;
    size_t call_count;
    double execution_time_ms;
    size_t hot_path_count;
    bool is_hot_function;
} WynProfileData;

// Optimization pass
typedef struct WynOptimizationPass {
    WynPassType type;
    char* name;
    char* description;
    bool (*run)(void* module, void* context);
    bool (*validate)(void* module);
    void (*cleanup)(struct WynOptimizationPass* pass);
} WynOptimizationPass;

// Pass manager
typedef struct WynPassManager {
    WynOptimizationPass** passes;
    size_t pass_count;
    size_t pass_capacity;
    WynOptLevel opt_level;
    WynProfileData* profile_data;
    size_t profile_count;
    bool link_time_optimization;
    bool profile_guided_optimization;
} WynPassManager;

// Pass manager functions
WynPassManager* wyn_pass_manager_new(void);
void wyn_pass_manager_free(WynPassManager* manager);
bool wyn_pass_manager_set_opt_level(WynPassManager* manager, WynOptLevel level);
bool wyn_pass_manager_add_pass(WynPassManager* manager, WynPassType type);
bool wyn_pass_manager_remove_pass(WynPassManager* manager, WynPassType type);
bool wyn_pass_manager_run_passes(WynPassManager* manager, void* module);

// Individual pass creation
WynOptimizationPass* wyn_create_dead_code_elimination_pass(void);
WynOptimizationPass* wyn_create_constant_folding_pass(void);
WynOptimizationPass* wyn_create_function_inlining_pass(void);
WynOptimizationPass* wyn_create_loop_unrolling_pass(void);
WynOptimizationPass* wyn_create_vectorization_pass(void);
WynOptimizationPass* wyn_create_escape_analysis_pass(void);
WynOptimizationPass* wyn_create_tail_call_optimization_pass(void);
WynOptimizationPass* wyn_create_global_value_numbering_pass(void);
WynOptimizationPass* wyn_create_loop_invariant_code_motion_pass(void);
WynOptimizationPass* wyn_create_strength_reduction_pass(void);

// Profile-guided optimization
bool wyn_pass_manager_load_profile_data(WynPassManager* manager, const char* profile_file);
bool wyn_pass_manager_enable_pgo(WynPassManager* manager, bool enable);
WynProfileData* wyn_profile_data_create(const char* function_name, size_t call_count, double execution_time);
void wyn_profile_data_free(WynProfileData* data);

// Link-time optimization
bool wyn_pass_manager_enable_lto(WynPassManager* manager, bool enable);
bool wyn_run_link_time_optimization(WynPassManager* manager, void** modules, size_t module_count);

// Pass configuration
WynPassConfig* wyn_pass_config_create(WynPassType type, bool enabled, int priority);
void wyn_pass_config_free(WynPassConfig* config);
bool wyn_pass_manager_configure_pass(WynPassManager* manager, const WynPassConfig* config);

// Optimization utilities
bool wyn_is_hot_function(const WynProfileData* profile_data, const char* function_name);
double wyn_get_function_execution_time(const WynProfileData* profile_data, size_t count, const char* function_name);
size_t wyn_get_function_call_count(const WynProfileData* profile_data, size_t count, const char* function_name);

// Pass validation and analysis
bool wyn_validate_pass_order(WynPassManager* manager);
bool wyn_analyze_pass_effectiveness(WynPassManager* manager, void* module_before, void* module_after);
void wyn_print_pass_statistics(WynPassManager* manager);

// Compile-time evaluation support
typedef struct {
    char* expression;
    void* result;
    bool is_constant;
    bool can_evaluate_at_compile_time;
} WynConstantExpression;

WynConstantExpression* wyn_analyze_constant_expression(void* expr);
bool wyn_evaluate_at_compile_time(WynConstantExpression* const_expr);
void wyn_constant_expression_free(WynConstantExpression* const_expr);

// Advanced optimization strategies
typedef struct {
    bool aggressive_inlining;
    bool cross_module_optimization;
    bool whole_program_optimization;
    bool profile_guided_inlining;
    size_t inline_threshold;
    size_t unroll_threshold;
} WynAdvancedOptConfig;

bool wyn_pass_manager_set_advanced_config(WynPassManager* manager, const WynAdvancedOptConfig* config);
WynAdvancedOptConfig wyn_get_default_advanced_config(WynOptLevel level);

// Pass scheduling and dependencies
typedef struct {
    WynPassType pass;
    WynPassType* dependencies;
    size_t dependency_count;
    WynPassType* conflicts;
    size_t conflict_count;
} WynPassDependency;

bool wyn_pass_manager_add_dependency(WynPassManager* manager, const WynPassDependency* dependency);
bool wyn_pass_manager_schedule_passes(WynPassManager* manager);

#endif // WYN_LLVM_PASSES_H
