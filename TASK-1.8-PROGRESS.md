# Task 1.8 Progress Report

## Status: IN PROGRESS

## Completed
- ✅ Added EXPR_FIELD_ACCESS case to codegen_expression
- ✅ Implemented codegen_field_access() function
- ✅ Added EXPR_STRUCT_INIT case to codegen_expression
- ✅ Implemented codegen_struct_init() function
- ✅ Added Program* field to LLVMCodegenContext
- ✅ Struct initialization generates correct LLVM IR
- ✅ Compiler builds without errors

## Current Bug
**Struct field access doesn't generate extractvalue instruction**

### Symptoms
```wyn
struct Point { x: int, y: int }
fn main() -> int { 
    var p = Point { x: 42, y: 10 }
    return p.x  // Should return 42, returns 0
}
```

### Generated LLVM IR
```llvm
define i32 @wyn_main() {
entry:
  %p = alloca %Point, align 8
  store %Point { i32 42, i32 10 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4      ; Struct loaded
  ret i32 0                                 ; Returns 0 instead of field value
}
```

### Expected LLVM IR
```llvm
define i32 @wyn_main() {
entry:
  %p = alloca %Point, align 8
  store %Point { i32 42, i32 10 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 0   ; Extract field 0 (x)
  ret i32 %field_val
}
```

### Root Cause
The field access expression `p.x` is being parsed/checked but not reaching codegen_field_access().
Possible causes:
1. Parser not creating EXPR_FIELD_ACCESS node
2. Checker transforming field access to something else
3. Return statement codegen not calling codegen_expression properly
4. Field access expression being optimized away

### Next Steps
1. Add debug logging to codegen_field_access to verify it's called
2. Check parser output for `p.x` expression type
3. Verify checker doesn't transform EXPR_FIELD_ACCESS
4. Test simpler case: assign field to variable first

## Test Results
- test_01_variables.wyn: ❌ (returns 30 instead of 0)
- test_02_functions.wyn: ❌ (returns 7 instead of 0)
- test_03_structs.wyn: ❌ (returns 0 instead of 42)
- test_04_enums.wyn: ✅ (returns 42)
- test_05_arrays.wyn: ✅ (returns 1)

## Files Modified
- src/llvm_expression_codegen.c: Added struct init and field access
- src/llvm_expression_codegen.h: Added function declarations
- src/llvm_context.h: Added Program* field
- src/llvm_codegen.c: Store program in context
- test_runner.wyn: Spawn-based parallel test runner

## Commits
- 5a22675: wip: task-1.8 - Add LLVM struct init and field access (partial)
- 9a78ef9: progress: Update task-1.8 status to in_progress
