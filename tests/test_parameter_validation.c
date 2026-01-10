#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test macro
#define RUN_TEST(name) do { \
    printf("Running test: %s... ", #name); \
    if (name()) { \
        printf("✅ PASSED\n"); \
    } else { \
        printf("❌ FAILED\n"); \
        all_passed = false; \
    } \
} while(0)

// Minimal types for testing parameter validation
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID
} TypeKind;

typedef struct {
    const char* start;
    int length;
} Token;

typedef struct Type {
    TypeKind kind;
    Token name;
} Type;

typedef struct {
    Token name;
    Type* type;
} Parameter;

typedef struct {
    Token name;
    Parameter* params;
    int param_count;
    Type* return_type;
} FunctionDecl;

typedef struct {
    Type** arg_types;
    int arg_count;
} CallSite;

// Parameter validation results
typedef enum {
    VALIDATION_SUCCESS,
    VALIDATION_PARAM_COUNT_MISMATCH,
    VALIDATION_TYPE_MISMATCH,
    VALIDATION_NULL_FUNCTION,
    VALIDATION_NULL_ARGS
} ValidationResult;

// T1.5.4: Parameter Validation Functions
ValidationResult wyn_validate_function_call(FunctionDecl* func, CallSite* call) {
    if (!func) {
        return VALIDATION_NULL_FUNCTION;
    }
    
    if (!call || !call->arg_types) {
        return VALIDATION_NULL_ARGS;
    }
    
    // Check parameter count
    if (call->arg_count != func->param_count) {
        return VALIDATION_PARAM_COUNT_MISMATCH;
    }
    
    // Check type compatibility for each parameter
    for (int i = 0; i < func->param_count; i++) {
        if (call->arg_types[i]->kind != func->params[i].type->kind) {
            return VALIDATION_TYPE_MISMATCH;
        }
    }
    
    return VALIDATION_SUCCESS;
}

const char* wyn_validation_error_message(ValidationResult result) {
    switch (result) {
        case VALIDATION_SUCCESS:
            return "Function call is valid";
        case VALIDATION_PARAM_COUNT_MISMATCH:
            return "Parameter count mismatch";
        case VALIDATION_TYPE_MISMATCH:
            return "Parameter type mismatch";
        case VALIDATION_NULL_FUNCTION:
            return "Function is null";
        case VALIDATION_NULL_ARGS:
            return "Arguments are null";
        default:
            return "Unknown validation error";
    }
}

bool wyn_is_type_compatible(Type* expected, Type* actual) {
    if (!expected || !actual) {
        return false;
    }
    
    // Exact type match
    if (expected->kind == actual->kind) {
        return true;
    }
    
    // Allow int to float conversion
    if (expected->kind == TYPE_FLOAT && actual->kind == TYPE_INT) {
        return true;
    }
    
    return false;
}

ValidationResult wyn_validate_function_call_with_conversion(FunctionDecl* func, CallSite* call) {
    if (!func) {
        return VALIDATION_NULL_FUNCTION;
    }
    
    if (!call || !call->arg_types) {
        return VALIDATION_NULL_ARGS;
    }
    
    // Check parameter count
    if (call->arg_count != func->param_count) {
        return VALIDATION_PARAM_COUNT_MISMATCH;
    }
    
    // Check type compatibility with conversions
    for (int i = 0; i < func->param_count; i++) {
        if (!wyn_is_type_compatible(func->params[i].type, call->arg_types[i])) {
            return VALIDATION_TYPE_MISMATCH;
        }
    }
    
    return VALIDATION_SUCCESS;
}

// Helper functions for creating test data
Type* make_type(TypeKind kind) {
    Type* type = malloc(sizeof(Type));
    type->kind = kind;
    return type;
}

Parameter* make_parameter(const char* name, TypeKind type_kind) {
    Parameter* param = malloc(sizeof(Parameter));
    param->name.start = name;
    param->name.length = strlen(name);
    param->type = make_type(type_kind);
    return param;
}

FunctionDecl* make_function(const char* name, Parameter* params, int param_count) {
    FunctionDecl* func = malloc(sizeof(FunctionDecl));
    func->name.start = name;
    func->name.length = strlen(name);
    func->params = params;
    func->param_count = param_count;
    func->return_type = make_type(TYPE_VOID);
    return func;
}

CallSite* make_call_site(Type** arg_types, int arg_count) {
    CallSite* call = malloc(sizeof(CallSite));
    call->arg_types = arg_types;
    call->arg_count = arg_count;
    return call;
}

// Test functions
static bool test_parameter_count_validation() {
    // Create function: fn test(x: int, y: int)
    Parameter params[2];
    params[0] = *make_parameter("x", TYPE_INT);
    params[1] = *make_parameter("y", TYPE_INT);
    
    FunctionDecl* func = make_function("test", params, 2);
    
    // Test correct parameter count
    Type* args1[2] = {make_type(TYPE_INT), make_type(TYPE_INT)};
    CallSite* call1 = make_call_site(args1, 2);
    
    ValidationResult result1 = wyn_validate_function_call(func, call1);
    if (result1 != VALIDATION_SUCCESS) {
        return false;
    }
    
    // Test incorrect parameter count (too few)
    Type* args2[1] = {make_type(TYPE_INT)};
    CallSite* call2 = make_call_site(args2, 1);
    
    ValidationResult result2 = wyn_validate_function_call(func, call2);
    if (result2 != VALIDATION_PARAM_COUNT_MISMATCH) {
        return false;
    }
    
    // Test incorrect parameter count (too many)
    Type* args3[3] = {make_type(TYPE_INT), make_type(TYPE_INT), make_type(TYPE_INT)};
    CallSite* call3 = make_call_site(args3, 3);
    
    ValidationResult result3 = wyn_validate_function_call(func, call3);
    if (result3 != VALIDATION_PARAM_COUNT_MISMATCH) {
        return false;
    }
    
    return true;
}

static bool test_type_compatibility_validation() {
    // Create function: fn test(x: int, y: string)
    Parameter params[2];
    params[0] = *make_parameter("x", TYPE_INT);
    params[1] = *make_parameter("y", TYPE_STRING);
    
    FunctionDecl* func = make_function("test", params, 2);
    
    // Test correct types
    Type* args1[2] = {make_type(TYPE_INT), make_type(TYPE_STRING)};
    CallSite* call1 = make_call_site(args1, 2);
    
    ValidationResult result1 = wyn_validate_function_call(func, call1);
    if (result1 != VALIDATION_SUCCESS) {
        return false;
    }
    
    // Test incorrect types
    Type* args2[2] = {make_type(TYPE_STRING), make_type(TYPE_INT)};
    CallSite* call2 = make_call_site(args2, 2);
    
    ValidationResult result2 = wyn_validate_function_call(func, call2);
    if (result2 != VALIDATION_TYPE_MISMATCH) {
        return false;
    }
    
    return true;
}

static bool test_type_conversion_validation() {
    // Create function: fn test(x: float)
    Parameter params[1];
    params[0] = *make_parameter("x", TYPE_FLOAT);
    
    FunctionDecl* func = make_function("test", params, 1);
    
    // Test int to float conversion (should be allowed)
    Type* args1[1] = {make_type(TYPE_INT)};
    CallSite* call1 = make_call_site(args1, 1);
    
    ValidationResult result1 = wyn_validate_function_call_with_conversion(func, call1);
    if (result1 != VALIDATION_SUCCESS) {
        return false;
    }
    
    // Test string to float conversion (should not be allowed)
    Type* args2[1] = {make_type(TYPE_STRING)};
    CallSite* call2 = make_call_site(args2, 1);
    
    ValidationResult result2 = wyn_validate_function_call_with_conversion(func, call2);
    if (result2 != VALIDATION_TYPE_MISMATCH) {
        return false;
    }
    
    return true;
}

static bool test_null_validation() {
    // Test null function
    CallSite* call = make_call_site(NULL, 0);
    ValidationResult result1 = wyn_validate_function_call(NULL, call);
    if (result1 != VALIDATION_NULL_FUNCTION) {
        return false;
    }
    
    // Test null call site
    Parameter params[1];
    params[0] = *make_parameter("x", TYPE_INT);
    FunctionDecl* func = make_function("test", params, 1);
    
    ValidationResult result2 = wyn_validate_function_call(func, NULL);
    if (result2 != VALIDATION_NULL_ARGS) {
        return false;
    }
    
    return true;
}

static bool test_error_messages() {
    // Test all error message types
    const char* msg1 = wyn_validation_error_message(VALIDATION_SUCCESS);
    if (strcmp(msg1, "Function call is valid") != 0) {
        return false;
    }
    
    const char* msg2 = wyn_validation_error_message(VALIDATION_PARAM_COUNT_MISMATCH);
    if (strcmp(msg2, "Parameter count mismatch") != 0) {
        return false;
    }
    
    const char* msg3 = wyn_validation_error_message(VALIDATION_TYPE_MISMATCH);
    if (strcmp(msg3, "Parameter type mismatch") != 0) {
        return false;
    }
    
    return true;
}

int main() {
    printf("🔥 Testing T1.5.4: Parameter Validation Implementation\n");
    printf("====================================================\n\n");
    
    bool all_passed = true;
    
    RUN_TEST(test_parameter_count_validation);
    RUN_TEST(test_type_compatibility_validation);
    RUN_TEST(test_type_conversion_validation);
    RUN_TEST(test_null_validation);
    RUN_TEST(test_error_messages);
    
    printf("\n====================================================\n");
    if (all_passed) {
        printf("✅ All T1.5.4 parameter validation tests PASSED!\n");
        printf("T1.5.4: Parameter Validation Implementation - COMPLETED ✅\n");
        return 0;
    } else {
        printf("❌ Some T1.5.4 tests FAILED!\n");
        return 1;
    }
}
