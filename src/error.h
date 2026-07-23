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

// ── Diagnostics sink (wyn check --json) ─────────────────────────────
// Structured diagnostics for machine consumers (agents, LSP). Emission
// sites record through diag_record*; the text renderer at each site stays
// byte-identical (it is simply skipped in JSON mode). Sites not yet
// converted still surface via diag_capture_text(), which parses the
// captured ad-hoc output into fallback records.

typedef enum {
    WYN_DIAG_ERROR,
    WYN_DIAG_WARNING,
    WYN_DIAG_NOTE
} WynDiagSeverity;

void diag_json_enable(void);
bool diag_json_on(void);
// Current file being parsed/checked - tags each recorded diagnostic.
void diag_set_file(const char* path);
const char* diag_file(void);
// Record one diagnostic (printf-style message). line/col are 1-based as
// printed by the text renderer; 0 means unknown.
void diag_record(WynDiagSeverity sev, int line, int col, const char* fmt, ...);
// Attach a help string to the most recent diagnostic.
void diag_record_help(const char* fmt, ...);
// Attach a literal replacement to the most recent diagnostic - ONLY when
// the checker literally knows the fix text (did-you-mean, add-pub, and
// removed-syntax style errors).
void diag_record_fix(const char* fix_old, const char* fix_new);
// Parse captured ad-hoc text output (unconverted emission sites) into
// fallback records. ANSI escapes are stripped; source-context lines are
// skipped; Help/Did-you-mean/Suggestion lines attach to the last record.
void diag_capture_text(const char* text);
int diag_error_count(void);
int diag_warning_count(void);
// Write one NDJSON object per diagnostic, then the summary line.
void diag_write_json(void* out_file, bool ok);
void diag_reset(void);

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
void parser_error_missing_semicolon(const char* filename, int line, int column);
void parser_error_unclosed_delimiter(const char* filename, int line, int column, 
                                    char opening_char, int opening_line, int opening_column);

// T1.2.4: Type checker error message functions
void type_error_mismatch(const char* expected_type, const char* actual_type, const char* context, int line, int column);
void type_error_undefined_variable(const char* var_name, int line, int column);
void type_error_undefined_function(const char* func_name, int line, int column);
void type_error_wrong_arg_count(const char* func_name, int expected, int actual, int line, int column);
void type_error_invalid_assignment(const char* var_type, const char* value_type, int line, int column);
void type_suggest_conversion(const char* from_type, const char* to_type);

#endif
