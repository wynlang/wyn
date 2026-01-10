#ifndef WYN_DEVTOOLS_H
#define WYN_DEVTOOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Forward declarations
typedef struct WynFormatter WynFormatter;
typedef struct WynDocGenerator WynDocGenerator;
typedef struct WynRepl WynRepl;

// Formatter configuration
typedef struct {
    int indent_size;
    bool use_tabs;
    int max_line_length;
    bool space_before_paren;
    bool space_after_comma;
    bool newline_before_brace;
    bool align_assignments;
    bool sort_imports;
} WynFormatterConfig;

// Documentation configuration
typedef struct {
    char* output_dir;
    char* template_dir;
    bool include_private;
    bool generate_index;
    bool markdown_output;
    bool html_output;
    char* project_name;
    char* project_version;
} WynDocConfig;

// REPL configuration
typedef struct {
    bool show_types;
    bool show_memory_usage;
    bool auto_complete;
    int history_size;
    char* prompt;
    char* continuation_prompt;
} WynReplConfig;

// Code formatter
typedef struct WynFormatter {
    WynFormatterConfig config;
    char* input_buffer;
    size_t input_size;
    char* output_buffer;
    size_t output_size;
    size_t output_capacity;
    int current_indent;
    bool in_string;
    bool in_comment;
} WynFormatter;

// Documentation generator
typedef struct WynDocGenerator {
    WynDocConfig config;
    char** source_files;
    size_t source_file_count;
    char* current_module;
    FILE* output_file;
    bool in_doc_comment;
} WynDocGenerator;

// REPL state
typedef struct WynRepl {
    WynReplConfig config;
    char** history;
    size_t history_count;
    size_t history_capacity;
    char* input_buffer;
    size_t input_size;
    size_t input_capacity;
    bool running;
    int exit_code;
} WynRepl;

// Formatter functions
WynFormatter* wyn_formatter_new(void);
void wyn_formatter_free(WynFormatter* formatter);
bool wyn_formatter_load_config(WynFormatter* formatter, const char* config_file);
bool wyn_formatter_save_config(WynFormatter* formatter, const char* config_file);
WynFormatterConfig wyn_formatter_config_default(void);
bool wyn_formatter_format_file(WynFormatter* formatter, const char* input_file, const char* output_file);
bool wyn_formatter_format_string(WynFormatter* formatter, const char* input, char** output);
bool wyn_formatter_format_directory(WynFormatter* formatter, const char* directory, bool recursive);

// Documentation generator functions
WynDocGenerator* wyn_doc_generator_new(void);
void wyn_doc_generator_free(WynDocGenerator* generator);
bool wyn_doc_generator_load_config(WynDocGenerator* generator, const char* config_file);
bool wyn_doc_generator_save_config(WynDocGenerator* generator, const char* config_file);
WynDocConfig wyn_doc_config_default(void);
bool wyn_doc_generator_add_source(WynDocGenerator* generator, const char* source_file);
bool wyn_doc_generator_generate(WynDocGenerator* generator);
bool wyn_doc_generator_generate_file(WynDocGenerator* generator, const char* source_file);
bool wyn_doc_generator_generate_module(WynDocGenerator* generator, const char* module_name);

// REPL functions
WynRepl* wyn_repl_new(void);
void wyn_repl_free(WynRepl* repl);
bool wyn_repl_load_config(WynRepl* repl, const char* config_file);
bool wyn_repl_save_config(WynRepl* repl, const char* config_file);
WynReplConfig wyn_repl_config_default(void);
bool wyn_repl_start(WynRepl* repl);
bool wyn_repl_execute_line(WynRepl* repl, const char* line);
bool wyn_repl_execute_file(WynRepl* repl, const char* file_path);
bool wyn_repl_add_history(WynRepl* repl, const char* line);
char** wyn_repl_get_completions(WynRepl* repl, const char* partial, size_t* count);

// Utility functions
bool wyn_devtools_is_wyn_file(const char* filename);
char* wyn_devtools_read_file(const char* filename);
bool wyn_devtools_write_file(const char* filename, const char* content);
bool wyn_devtools_create_directory(const char* path);
char** wyn_devtools_list_files(const char* directory, const char* extension, size_t* count);

// Command-line interface functions
int wyn_fmt_main(int argc, char** argv);
int wyn_doc_main(int argc, char** argv);
int wyn_repl_main(int argc, char** argv);

// Integration functions
bool wyn_devtools_init_project(const char* project_dir);
bool wyn_devtools_setup_vscode(const char* project_dir);
bool wyn_devtools_setup_vim(const char* project_dir);

#endif // WYN_DEVTOOLS_H
