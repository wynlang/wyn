#ifndef ARC_RUNTIME_H
#define ARC_RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>

// T2.3.1: Object Header Design
// Efficient object header layout for ARC memory management

#define ARC_MAGIC 0x41524330  // "ARC0" in hex (0x41='A', 0x52='R', 0x43='C', 0x30='0')
#define ARC_WEAK_FLAG 0x80000000
#define ARC_COUNT_MASK 0x7FFFFFFF

// Object header for ARC memory management
typedef struct WynObjectHeader {
    uint32_t magic;                    // Magic number for validation
    _Atomic uint32_t ref_count;        // Atomic reference count (MSB = weak flag)
    uint32_t type_id;                  // Type identifier for runtime type information
    uint32_t size;                     // Object size for deallocation
    void (*destructor)(void*);         // Custom destructor function pointer
} WynObjectHeader;

// Complete object structure with flexible array member
typedef struct WynObject {
    WynObjectHeader header;
    uint8_t data[];           // Flexible array member for object data
} WynObject;

// Type identifiers for built-in types
typedef enum {
    WYN_TYPE_UNKNOWN = 0,
    WYN_TYPE_INT = 1,
    WYN_TYPE_FLOAT = 2,
    WYN_TYPE_BOOL = 3,
    WYN_TYPE_STRING = 4,
    WYN_TYPE_ARRAY = 5,
    WYN_TYPE_STRUCT = 6,
    WYN_TYPE_FUNCTION = 7,
    WYN_TYPE_CUSTOM_BASE = 1000  // Base for user-defined types
} WynTypeId;

// Minimal ARC struct
typedef struct WynArc {
    int ref_count;
    char data[];
} WynArc;

// Minimal ARC operations
WynArc* wyn_arc_new(size_t size, void* init_data);
WynArc* wyn_arc_retain_arc(WynArc* arc);
void wyn_arc_release_arc(WynArc* arc);

// ARC operations - core interface
WynObject* wyn_arc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*));
WynObject* wyn_arc_retain(WynObject* obj);
void wyn_arc_release(WynObject* obj);
WynObject* wyn_arc_weak_retain(WynObject* obj);
void wyn_arc_weak_release(WynObject* obj);

// T2.3.2: Enhanced reference counting operations with performance optimizations
WynObject* wyn_arc_retain_optimized(WynObject* obj);
void wyn_arc_release_optimized(WynObject* obj);
void wyn_arc_retain_batch(WynObject** objects, size_t count);
void wyn_arc_release_batch(WynObject** objects, size_t count);
WynObject* wyn_arc_retain_if_not_null(WynObject* obj);
void wyn_arc_release_if_not_null(WynObject* obj);
WynObject* wyn_arc_move(WynObject** obj_ptr);

// T2.3.3: Weak Reference System
typedef struct WynWeakRef WynWeakRef;

WynWeakRef* wyn_weak_create(WynObject* obj);
WynObject* wyn_weak_access(WynWeakRef* weak_ref);
WynObject* wyn_weak_promote(WynWeakRef* weak_ref);
bool wyn_weak_is_valid(WynWeakRef* weak_ref);
void wyn_weak_destroy(WynWeakRef* weak_ref);
void wyn_weak_nullify_all(WynObject* obj);
void wyn_weak_create_batch(WynObject** objects, WynWeakRef** weak_refs, size_t count);
void wyn_weak_destroy_batch(WynWeakRef** weak_refs, size_t count);

// T2.3.4: Cycle Detection Algorithm
typedef struct {
    size_t collection_threshold;
    size_t max_objects_per_cycle;
    bool auto_collection_enabled;
    double collection_frequency;
    size_t min_cycle_size;
} CycleDetectionConfig;

typedef struct {
    size_t cycles_detected;
    size_t objects_collected;
    size_t collection_runs;
    size_t false_positives;
    size_t candidate_count;
    size_t allocations_since_collection;
    double collection_efficiency;
} WynCycleStats;

void wyn_cycle_detection_init(void);
void wyn_cycle_configure(CycleDetectionConfig* config);
void wyn_cycle_add_candidate(WynObject* obj);
size_t wyn_cycle_collect(void);
void wyn_cycle_check_collection_trigger(void);
CycleDetectionConfig wyn_cycle_get_config(void);
WynCycleStats wyn_cycle_get_stats(void);
void wyn_cycle_reset_stats(void);
void wyn_cycle_print_stats(void);
void wyn_cycle_cleanup(void);

// T2.3.5: Memory Pool Optimization
typedef struct {
    size_t total_pools;
    size_t total_blocks;
    size_t free_blocks;
    size_t total_allocations;
    size_t total_deallocations;
    size_t large_allocations;
    size_t large_deallocations;
    size_t total_memory_used;
    size_t peak_memory_used;
    double fragmentation_ratio;
    double pool_utilization;
} WynPoolStats;

void wyn_pool_init(void);
void* wyn_pool_alloc(size_t size);
void wyn_pool_free(void* ptr, size_t size);
WynObject* wyn_arc_alloc_pooled(size_t size, uint32_t type_id, void (*destructor)(void*));
void wyn_arc_deallocate_pooled(WynObject* obj);
WynPoolStats wyn_pool_get_stats(void);
void wyn_pool_reset_stats(void);
void wyn_pool_print_stats(void);
void wyn_pool_print_detailed_stats(void);
void wyn_pool_cleanup(void);
void wyn_pool_alloc_batch(void** ptrs, size_t* sizes, size_t count);
void wyn_pool_free_batch(void** ptrs, size_t* sizes, size_t count);

// T2.3.6: Runtime Performance Monitoring
typedef struct {
    size_t total_arc_allocs;
    size_t total_arc_deallocs;
    size_t total_retains;
    size_t total_releases;
    size_t total_weak_creates;
    size_t total_weak_destroys;
    size_t total_cycle_collections;
    size_t total_pool_allocs;
    size_t total_pool_frees;
    double avg_alloc_time_us;
    double avg_dealloc_time_us;
    size_t current_memory_usage;
    size_t sample_count;
    bool regression_detected;
    char regression_message[256];
} WynPerfStats;

void wyn_perf_init(void);
void wyn_perf_configure(bool enabled, bool detailed_logging, double regression_threshold);
void wyn_perf_record_alloc(uint64_t time_us);
void wyn_perf_record_dealloc(uint64_t time_us);
void wyn_perf_record_retain(void);
void wyn_perf_record_release(void);
void wyn_perf_record_weak_create(void);
void wyn_perf_record_weak_destroy(void);
void wyn_perf_record_cycle_collection(void);
void wyn_perf_record_pool_alloc(void);
void wyn_perf_record_pool_free(void);
void wyn_perf_sample(void);
WynPerfStats wyn_perf_get_stats(void);
void wyn_perf_reset_stats(void);
void wyn_perf_print_stats(void);
void wyn_perf_cleanup(void);

// T2.4.1: Escape Analysis Implementation
typedef struct AllocationSite AllocationSite;

typedef enum {
    ESCAPE_UNKNOWN = 0,
    ESCAPE_NO_ESCAPE,
    ESCAPE_LOCAL_ESCAPE,
    ESCAPE_GLOBAL_ESCAPE,
    ESCAPE_ARGUMENT_ESCAPE
} EscapeStatus;

typedef struct {
    size_t total_allocation_sites;
    size_t stack_allocatable_sites;
    size_t eliminated_retain_release_pairs;
    double stack_allocation_ratio;
    double optimization_ratio;
} WynEscapeStats;

void wyn_escape_analysis_init(void);
AllocationSite* wyn_escape_register_allocation(void* allocation_point, size_t size, uint32_t type_id);
void wyn_escape_add_reference(AllocationSite* site);
void wyn_escape_analyze_all(void);
bool wyn_escape_can_stack_allocate(AllocationSite* site);
bool wyn_escape_needs_retain_release(AllocationSite* site);
void* wyn_stack_alloc(size_t size);
void wyn_stack_free(void* ptr);
WynObject* wyn_arc_alloc_optimized(size_t size, uint32_t type_id, void (*destructor)(void*));
WynObject* wyn_arc_retain_escape_optimized(WynObject* obj, AllocationSite* site);
void wyn_arc_release_escape_optimized(WynObject* obj, AllocationSite* site);
WynEscapeStats wyn_escape_get_stats(void);
void wyn_escape_print_stats(void);
void wyn_escape_reset(void);
void wyn_escape_cleanup(void);
void wyn_escape_llvm_optimization_pass(void);
void wyn_escape_set_enabled(bool enabled);

// T2.4.2: Automatic Retain/Release Insertion
typedef struct InsertionPoint InsertionPoint;

typedef enum {
    INSERT_POINT_ASSIGNMENT,
    INSERT_POINT_FUNCTION_PARAM,
    INSERT_POINT_FUNCTION_RETURN,
    INSERT_POINT_SCOPE_EXIT,
    INSERT_POINT_CONDITIONAL,
    INSERT_POINT_LOOP_ENTRY,
    INSERT_POINT_LOOP_EXIT
} InsertionPointType;

typedef enum {
    ARC_OP_RETAIN,
    ARC_OP_RELEASE,
    ARC_OP_WEAK_RETAIN,
    ARC_OP_WEAK_RELEASE,
    ARC_OP_MOVE
} ARCOperation;

typedef struct {
    size_t total_insertions;
    size_t optimized_away;
    size_t function_param_insertions;
    size_t function_return_insertions;
    size_t scope_exit_insertions;
    double optimization_ratio;
} WynARCInsertionStats;

void wyn_arc_insertion_init(void);
InsertionPoint* wyn_arc_register_insertion(InsertionPointType type, ARCOperation operation, void* code_location, void* object_reference);
void wyn_arc_optimize_insertions(void);
void wyn_arc_insert_assignment_retain(void* code_location, WynObject* obj);
void wyn_arc_insert_scope_exit_release(void* code_location, WynObject* obj);
WynObject* wyn_arc_handle_function_param(WynObject* obj, bool transfer_ownership);
WynObject* wyn_arc_handle_function_return(WynObject* obj, bool transfer_ownership);
void wyn_arc_handle_conditional_branch(WynObject* obj, bool entering_branch);
void wyn_arc_handle_loop(WynObject* obj, bool entering_loop);
void wyn_arc_optimize_common_patterns(void);
void wyn_arc_apply_insertions(void);
WynARCInsertionStats wyn_arc_insertion_get_stats(void);
void wyn_arc_insertion_print_stats(void);
void wyn_arc_insertion_reset(void);
void wyn_arc_insertion_cleanup(void);
void wyn_arc_insertion_set_enabled(bool enabled);

// T2.4.3: Weak Reference Code Generation
typedef struct {
    size_t weak_conversions_generated;
    size_t null_checks_generated;
    size_t strong_promotions_generated;
    size_t weak_assignments_generated;
    double null_safety_ratio;
    double conversion_efficiency;
} WynWeakCodegenStats;

void wyn_weak_codegen_init(void);
void* wyn_weak_codegen_create_weak(void* strong_ref, void* code_location);
void* wyn_weak_codegen_promote_to_strong(void* weak_ref, void* code_location);
bool wyn_weak_codegen_null_check(void* weak_ref, void* code_location);
void wyn_weak_codegen_assign_weak(void** weak_ref_ptr, void* source_weak_ref, void* code_location);
void* wyn_weak_codegen_auto_convert_to_weak(void* strong_ref, void* code_location);
void* wyn_weak_codegen_auto_convert_to_strong(void* weak_ref, void* code_location);
void* wyn_weak_codegen_safe_access(void* weak_ref, void* code_location);
void wyn_weak_codegen_cleanup_weak(void* weak_ref, void* code_location);
bool wyn_weak_codegen_compare_weak(void* weak_ref1, void* weak_ref2, void* code_location);
void wyn_weak_codegen_array_set_weak(void** weak_array, size_t index, void* weak_ref, void* code_location);
WynWeakCodegenStats wyn_weak_codegen_get_stats(void);
void wyn_weak_codegen_print_stats(void);
void wyn_weak_codegen_reset(void);
void wyn_weak_codegen_cleanup(void);
void wyn_weak_codegen_set_enabled(bool enabled);

// T2.4.4: ARC Optimization Passes
typedef struct {
    size_t original_operations;
    size_t redundant_pairs_eliminated;
    size_t move_optimizations_applied;
    size_t temporary_objects_optimized;
    size_t final_operations;
    double optimization_ratio;
    bool success;
} WynARCOptimizationResult;

typedef struct {
    size_t total_arc_operations;
    size_t redundant_pairs_found;
    size_t move_candidates_found;
    size_t temporary_objects_found;
    double potential_optimization_ratio;
    bool optimization_recommended;
} WynOptimizationAnalysis;

typedef struct {
    size_t redundant_pairs_eliminated;
    size_t move_optimizations_applied;
    size_t temporary_objects_optimized;
    size_t llvm_passes_integrated;
    size_t total_optimizations;
    double optimization_efficiency;
} WynARCOptimizationStats;

void wyn_arc_optimization_init(void);
size_t wyn_arc_eliminate_redundant_pairs(void** operations, size_t operation_count);
size_t wyn_arc_optimize_temporary_moves(void** temp_objects, size_t temp_count);
bool wyn_arc_integrate_llvm_passes(void* llvm_module, void* pass_manager);
WynARCOptimizationResult wyn_arc_run_optimization_pass(void* code_context, size_t operation_count);
WynOptimizationAnalysis wyn_arc_analyze_optimization_opportunities(void* code_context, size_t code_size);
size_t wyn_arc_eliminate_dead_code(void** operations, size_t operation_count);
size_t wyn_arc_optimize_loop_operations(void* loop_context, size_t loop_iterations);
WynARCOptimizationStats wyn_arc_optimization_get_stats(void);
void wyn_arc_optimization_print_stats(void);
void wyn_arc_optimization_reset(void);
void wyn_arc_optimization_cleanup(void);
void wyn_arc_optimization_set_enabled(bool enabled);

// Utility functions
bool wyn_arc_is_valid(WynObject* obj);
uint32_t wyn_arc_get_ref_count(WynObject* obj);
uint32_t wyn_arc_get_type_id(WynObject* obj);
size_t wyn_arc_get_size(WynObject* obj);
void* wyn_arc_get_data(WynObject* obj);

// Internal functions (not for public use)
void wyn_arc_deallocate(WynObject* obj);
void wyn_panic(const char* message);

// Statistics and debugging
typedef struct {
    size_t total_allocations;
    size_t total_deallocations;
    size_t current_objects;
    size_t peak_objects;
    size_t total_memory;
    size_t peak_memory;
} WynARCStats;

// T2.3.2: Performance monitoring for reference counting operations
typedef struct {
    size_t total_retains;
    size_t total_releases;
    size_t fast_path_retains;
    size_t fast_path_releases;
    size_t overflow_checks;
    size_t double_release_checks;
    double fast_path_ratio_retain;
    double fast_path_ratio_release;
} WynARCPerformanceStats;

// T2.3.3: Weak reference statistics
typedef struct {
    size_t total_weak_refs;
    size_t nullified_refs;
    size_t active_refs;
    double nullification_rate;
} WynWeakRefStats;

WynARCStats wyn_arc_get_stats(void);
void wyn_arc_reset_stats(void);
void wyn_arc_print_stats(void);

WynARCPerformanceStats wyn_arc_get_performance_stats(void);
void wyn_arc_reset_performance_stats(void);
void wyn_arc_print_performance_stats(void);

WynWeakRefStats wyn_weak_get_stats(void);
void wyn_weak_reset_stats(void);
void wyn_weak_print_stats(void);
void wyn_weak_cleanup_registry(void);

#endif // ARC_RUNTIME_H