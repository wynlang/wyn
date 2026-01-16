#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include "error.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WynError* errors = NULL;
static int error_count = 0;
static int error_capacity = 0;

void report_error(ErrorCode code, const char* filename, int line, int column, const char* message) {
    report_error_with_suggestion(code, filename, line, column, message, NULL);
}

void report_error_with_suggestion(ErrorCode code, const char* filename, int line, int column, const char* message, const char* suggestion) {
    // Expand error array if needed
    if (error_count >= error_capacity) {
        error_capacity = error_capacity == 0 ? 10 : error_capacity * 2;
        errors = realloc(errors, error_capacity * sizeof(WynError));
    }
    
    WynError* error = &errors[error_count++];
    error->code = code;
    error->severity = (code >= 4000) ? ERROR_FATAL : (code >= 3000) ? ERROR_ERROR : ERROR_WARNING;
    error->message = strdup(message);
    error->filename = filename ? strdup(filename) : NULL;
    error->line = line;
    error->column = column;
    error->suggestion = suggestion ? strdup(suggestion) : NULL;
}

void print_error(WynError* error) {
    const char* severity_str = (error->severity == ERROR_FATAL) ? "FATAL" :
                              (error->severity == ERROR_ERROR) ? "ERROR" : "WARNING";
    
    if (error->filename) {
        printf("%s:%d:%d: %s: %s\n", error->filename, error->line, error->column, severity_str, error->message);
    } else {
        printf("%s: %s\n", severity_str, error->message);
    }
    
    if (error->suggestion) {
        printf("  Suggestion: %s\n", error->suggestion);
    }
}

bool has_errors(void) {
    return error_count > 0;
}

int get_error_count(void) {
    return error_count;
}

void clear_errors(void) {
    for (int i = 0; i < error_count; i++) {
        free(errors[i].message);
        free(errors[i].filename);
        free(errors[i].suggestion);
    }
    error_count = 0;
}

// T1.2.3: Parser error recovery implementation (basic versions)
void parser_error_at_current(const char* message) {
    report_error(ERR_UNEXPECTED_TOKEN, "parser", 0, 0, message);
}

void parser_error_at_previous(const char* message) {
    report_error(ERR_UNEXPECTED_TOKEN, "parser", 0, 0, message);
}

void parser_synchronize(void) {
    // Basic implementation - will be enhanced when integrated with parser
    clear_errors();
}

void parser_suggest_fix(const char* expected, const char* got) {
    char suggestion[256];
    snprintf(suggestion, sizeof(suggestion), "Expected '%s' but got '%s'", expected, got);
    report_error_with_suggestion(ERR_UNEXPECTED_TOKEN, "parser", 0, 0, 
                               "Syntax error", suggestion);
}

bool parser_check_and_suggest(int expected_token, const char* context) {
    char message[256];
    snprintf(message, sizeof(message), "Expected token %d in %s context", expected_token, context);
    parser_error_at_current(message);
    return false;
}

// T1.2.4: Type Checker Error Message Implementation

void type_error_mismatch(const char* expected_type, const char* actual_type, const char* context, int line, int column) {
    char message[512];
    snprintf(message, sizeof(message), "Type mismatch in %s: expected '%s' but got '%s'", 
             context, expected_type, actual_type);
    
    char suggestion[256];
    snprintf(suggestion, sizeof(suggestion), "Convert '%s' to '%s' or change the expected type", 
             actual_type, expected_type);
    
    report_error_with_suggestion(ERR_TYPE_MISMATCH, "type_checker", line, column, message, suggestion);
}

void type_error_undefined_variable(const char* var_name, int line, int column) {
    char message[256];
    snprintf(message, sizeof(message), "Undefined variable '%s'", var_name);
    
    char suggestion[256];
    snprintf(suggestion, sizeof(suggestion), "Declare variable '%s' before using it", var_name);
    
    report_error_with_suggestion(ERR_UNDEFINED_VARIABLE, "type_checker", line, column, message, suggestion);
}

void type_error_undefined_function(const char* func_name, int line, int column) {
    char message[256];
    snprintf(message, sizeof(message), "Undefined function '%s'", func_name);
    
    char suggestion[256];
    snprintf(suggestion, sizeof(suggestion), "Define function '%s' or check for typos in the function name", func_name);
    
    report_error_with_suggestion(ERR_UNDEFINED_FUNCTION, "type_checker", line, column, message, suggestion);
}

void type_error_wrong_arg_count(const char* func_name, int expected, int actual, int line, int column) {
    char message[256];
    snprintf(message, sizeof(message), "Function '%s' expects %d arguments but got %d", 
             func_name, expected, actual);
    
    char suggestion[256];
    if (actual < expected) {
        snprintf(suggestion, sizeof(suggestion), "Add %d more argument(s) to the function call", 
                 expected - actual);
    } else {
        snprintf(suggestion, sizeof(suggestion), "Remove %d argument(s) from the function call", 
                 actual - expected);
    }
    
    report_error_with_suggestion(ERR_WRONG_ARG_COUNT, "type_checker", line, column, message, suggestion);
}

void type_error_invalid_assignment(const char* var_type, const char* value_type, int line, int column) {
    char message[256];
    snprintf(message, sizeof(message), "Cannot assign '%s' to variable of type '%s'", 
             value_type, var_type);
    
    char suggestion[256];
    snprintf(suggestion, sizeof(suggestion), "Cast '%s' to '%s' or change the variable type", 
             value_type, var_type);
    
    report_error_with_suggestion(ERR_INVALID_ASSIGNMENT, "type_checker", line, column, message, suggestion);
}

void type_suggest_conversion(const char* from_type, const char* to_type) {
    char message[256];
    snprintf(message, sizeof(message), "Type conversion suggestion: '%s' to '%s'", from_type, to_type);
    
    char suggestion[512];
    if (strcmp(from_type, "int") == 0 && strcmp(to_type, "float") == 0) {
        snprintf(suggestion, sizeof(suggestion), "Use explicit cast: (float)value or add .0 to make it a float literal");
    } else if (strcmp(from_type, "float") == 0 && strcmp(to_type, "int") == 0) {
        snprintf(suggestion, sizeof(suggestion), "Use explicit cast: (int)value (note: this truncates the decimal part)");
    } else if (strcmp(from_type, "string") == 0 && strcmp(to_type, "int") == 0) {
        snprintf(suggestion, sizeof(suggestion), "Use string to integer conversion function: parse_int(value)");
    } else if (strcmp(from_type, "int") == 0 && strcmp(to_type, "string") == 0) {
        snprintf(suggestion, sizeof(suggestion), "Use integer to string conversion: to_string(value)");
    } else {
        snprintf(suggestion, sizeof(suggestion), "Check if conversion between '%s' and '%s' is supported", from_type, to_type);
    }
    
    report_error_with_suggestion(ERR_TYPE_MISMATCH, "type_checker", 0, 0, message, suggestion);
}


// Show source code context for better error messages
void show_error_context(const char* filename, int line, int column, const char* message, const char* suggestion) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Error at %s:%d:%d: %s\n", filename, line, column, message);
        if (suggestion) printf("  help: %s\n", suggestion);
        return;
    }
    
    // Read file lines
    char* lines[1000] = {0};
    char buffer[1024];
    int line_count = 0;
    while (fgets(buffer, sizeof(buffer), f) && line_count < 1000) {
        lines[line_count++] = strdup(buffer);
    }
    fclose(f);
    
    // Print error header
    printf("\nError: %s\n", message);
    printf("  --> %s:%d:%d\n", filename, line, column);
    printf("   |\n");
    
    // Show context (3 lines before and after)
    int start = (line - 3 > 0) ? line - 3 : 1;
    int end = (line + 3 < line_count) ? line + 3 : line_count;
    
    for (int i = start; i <= end && i <= line_count; i++) {
        if (i == line) {
            printf(" %3d | %s", i, lines[i-1]);
            printf("     | ");
            for (int j = 0; j < column - 1; j++) printf(" ");
            printf("^\n");
        } else {
            printf(" %3d | %s", i, lines[i-1]);
        }
    }
    
    printf("   |\n");
    if (suggestion) {
        printf("help: %s\n", suggestion);
    }
    printf("\n");
    
    // Free lines
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
}
