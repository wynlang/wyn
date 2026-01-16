#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>

typedef enum {
    // Lexer errors (1000-1999)
    ERR_INVALID_CHARACTER = 1000,
    ERR_UNTERMINATED_STRING,
    ERR_INVALID_NUMBER,
    ERR_INVALID_ESCAPE_SEQUENCE,
    
    // Parser errors (2000-2999)
    ERR_UNEXPECTED_TOKEN = 2000,
    ERR_MISSING_SEMICOLON,
    ERR_UNMATCHED_PAREN,
    ERR_INVALID_EXPRESSION,
    ERR_MISSING_RBRACE,
    
    // Type checker errors (3000-3999)
    ERR_TYPE_MISMATCH = 3000,
    ERR_UNDEFINED_VARIABLE,
    ERR_UNDEFINED_FUNCTION,
    ERR_WRONG_ARG_COUNT,
    ERR_INVALID_ASSIGNMENT,
    
    // Codegen errors (4000-4999)
    ERR_CODEGEN_FAILED = 4000,
    ERR_FILE_WRITE_FAILED,
    ERR_OUT_OF_MEMORY
} ErrorCode;

typedef enum {
    ERROR_INFO,
    ERROR_WARNING,
    ERROR_ERROR,
    ERROR_FATAL
} ErrorSeverity;

typedef struct {
    ErrorCode code;
    ErrorSeverity severity;
    char* message;
    char* filename;
    int line;
    int column;
    char* suggestion;
} WynError;

// Error reporting functions
void report_error(ErrorCode code, const char* filename, int line, int column, const char* message);
void report_error_with_suggestion(ErrorCode code, const char* filename, int line, int column, const char* message, const char* suggestion);
void print_error(WynError* error);
void clear_errors(void);
bool has_errors(void);
int get_error_count(void);

// Show source code context for errors
void show_error_context(const char* filename, int line, int column, const char* message, const char* suggestion);

// T1.2.3: Parser error recovery functions
void parser_error_at_current(const char* message);
void parser_error_at_previous(const char* message);
void parser_synchronize(void);
void parser_suggest_fix(const char* expected, const char* got);
bool parser_check_and_suggest(int expected_token, const char* context);

// T1.2.4: Type checker error message functions
void type_error_mismatch(const char* expected_type, const char* actual_type, const char* context, int line, int column);
void type_error_undefined_variable(const char* var_name, int line, int column);
void type_error_undefined_function(const char* func_name, int line, int column);
void type_error_wrong_arg_count(const char* func_name, int expected, int actual, int line, int column);
void type_error_invalid_assignment(const char* var_type, const char* value_type, int line, int column);
void type_suggest_conversion(const char* from_type, const char* to_type);

#endif
