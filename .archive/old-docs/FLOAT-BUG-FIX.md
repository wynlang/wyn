# Float Bug Fix - Complete

## Issue
LLVM backend crashed when compiling negative float literals and float.abs() method:
```
LLVM ERROR: Cannot select: f64 = sub ConstantFP:f64<0.000000e+00>, ...
```

## Root Causes

### 1. Constant Float Negation
When parsing `-3.14`, the parser creates a unary negation expression with operand `3.14`.
The LLVM codegen was using `LLVMBuildFNeg` for all float negations, but this generates
a subtraction instruction `0.0 - x` which LLVM's AArch64 backend cannot select for constants.

**Fix**: Detect constant floats and negate them directly:
```c
if (LLVMIsConstant(operand)) {
    LLVMBool loses_info = 0;
    double val = LLVMConstRealGetDouble(operand, &loses_info);
    return LLVMConstReal(LLVMTypeOf(operand), -val);
}
```

### 2. Missing float.abs() Implementation
The `abs()` method was only implemented for integers. When called on floats, it used
`LLVMBuildNeg` which doesn't work for float types.

**Fix**: Added float-specific abs() using FNeg:
```c
if (is_float_type(LLVMTypeOf(object))) {
    LLVMValueRef zero = LLVMConstReal(ctx->float_type, 0.0);
    LLVMValueRef is_neg = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, object, zero, "is_neg");
    LLVMValueRef neg_val = LLVMBuildFNeg(ctx->builder, object, "neg");
    return LLVMBuildSelect(ctx->builder, is_neg, neg_val, object, "abs");
}
```

### 3. Missing floor/ceil/round Methods
These methods were declared in the type system but not implemented in LLVM codegen.

**Fix**: Added LLVM intrinsic calls:
```c
LLVMTypeRef fn_type = LLVMFunctionType(ctx->float_type, (LLVMTypeRef[]){ctx->float_type}, 1, false);
LLVMValueRef floor_fn = LLVMGetNamedFunction(ctx->module, "llvm.floor.f64");
if (!floor_fn) {
    floor_fn = LLVMAddFunction(ctx->module, "llvm.floor.f64", fn_type);
}
LLVMValueRef args[] = { object };
return LLVMBuildCall2(ctx->builder, fn_type, floor_fn, args, 1, "floor");
```

## Changes Made

**File**: `src/llvm_expression_codegen.c`

1. **codegen_unary_expr()** (line ~295)
   - Added constant detection for float negation
   - Extract constant value and negate directly
   - Prevents LLVM from generating unsupported sub instruction

2. **codegen_method_call()** (line ~1143)
   - Added float type check for abs() method
   - Implemented float.abs() using FNeg and Select
   - Added floor(), ceil(), round() using LLVM intrinsics

## Test Results

### TDD Test
```wyn
fn main() -> int {
    var x = -3.14
    if x >= 0.0 { return 1; }
    
    var a = x.abs()
    if a < 3.13 { return 3; }
    if a > 3.15 { return 3; }
    
    return 0
}
```
✅ Passes (Exit: 0)

### Stdlib Test
`tests/stdlib/test_float_methods.wyn` - Tests abs, floor, ceil, round
✅ Passes (Exit: 0)

### Regression Suite
36/43 tests passing (83%) - Float bug no longer blocking

## Status
✅ **FIXED** - Float operations work correctly in LLVM backend
- Negative float literals compile and execute
- float.abs() returns correct absolute value
- floor/ceil/round methods implemented
- No more LLVM selection errors

## Next Steps
1. Implement advanced pattern features (or-patterns, destructuring)
2. Implement missing stdlib functions (System::, File::, Net::, Time::)
3. Fix remaining regression test failures
