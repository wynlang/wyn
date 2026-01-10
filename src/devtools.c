#include "devtools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

// Formatter implementation
WynFormatter* wyn_formatter_new(void) {
    WynFormatter* formatter = malloc(sizeof(WynFormatter));
    if (!formatter) return NULL;
    
    memset(formatter, 0, sizeof(WynFormatter));
    formatter->config = wyn_formatter_config_default();
    formatter->output_capacity = 4096;
    formatter->output_buffer = malloc(formatter->output_capacity);
    
    return formatter;
}

void wyn_formatter_free(WynFormatter* formatter) {
    if (!formatter) return;
    
    free(formatter->input_buffer);
    free(formatter->output_buffer);
    free(formatter);
}

WynFormatterConfig wyn_formatter_config_default(void) {
    WynFormatterConfig config;
    config.indent_size = 4;
    config.use_tabs = false;
    config.max_line_length = 100;
    config.space_before_paren = false;
    config.space_after_comma = true;
    config.newline_before_brace = false;
    config.align_assignments = true;
    config.sort_imports = true;
    
    return config;
}

bool wyn_formatter_format_string(WynFormatter* formatter, const char* input, char** output) {
    if (!formatter || !input || !output) return false;
    
    size_t input_len = strlen(input);
    
    // Ensure output buffer is large enough
    if (formatter->output_capacity < input_len * 2) {
        formatter->output_capacity = input_len * 2;
        formatter->output_buffer = realloc(formatter->output_buffer, formatter->output_capacity);
        if (!formatter->output_buffer) return false;
    }
    
    formatter->output_size = 0;
    formatter->current_indent = 0;
    formatter->in_string = false;
    formatter->in_comment = false;
    
    // Simple formatting implementation
    for (size_t i = 0; i < input_len; i++) {
        char c = input[i];
        
        // Handle string literals
        if (c == '"' && !formatter->in_comment) {
            formatter->in_string = !formatter->in_string;
            formatter->output_buffer[formatter->output_size++] = c;
            continue;
        }
        
        if (formatter->in_string) {
            formatter->output_buffer[formatter->output_size++] = c;
            continue;
        }
        
        // Handle comments
        if (c == '/' && i + 1 < input_len && input[i + 1] == '/') {
            formatter->in_comment = true;
            formatter->output_buffer[formatter->output_size++] = c;
            continue;
        }
        
        if (formatter->in_comment && c == '\n') {
            formatter->in_comment = false;
            formatter->output_buffer[formatter->output_size++] = c;
            continue;
        }
        
        if (formatter->in_comment) {
            formatter->output_buffer[formatter->output_size++] = c;
            continue;
        }
        
        // Format braces
        if (c == '{') {
            if (formatter->config.newline_before_brace) {
                formatter->output_buffer[formatter->output_size++] = '\n';
                // Add indentation
                for (int j = 0; j < formatter->current_indent; j++) {
                    if (formatter->config.use_tabs) {
                        formatter->output_buffer[formatter->output_size++] = '\t';
                    } else {
                        for (int k = 0; k < formatter->config.indent_size; k++) {
                            formatter->output_buffer[formatter->output_size++] = ' ';
                        }
                    }
                }
            }
            formatter->output_buffer[formatter->output_size++] = c;
            formatter->current_indent++;
            formatter->output_buffer[formatter->output_size++] = '\n';
        } else if (c == '}') {
            formatter->current_indent--;
            formatter->output_buffer[formatter->output_size++] = '\n';
            // Add indentation
            for (int j = 0; j < formatter->current_indent; j++) {
                if (formatter->config.use_tabs) {
                    formatter->output_buffer[formatter->output_size++] = '\t';
                } else {
                    for (int k = 0; k < formatter->config.indent_size; k++) {
                        formatter->output_buffer[formatter->output_size++] = ' ';
                    }
                }
            }
            formatter->output_buffer[formatter->output_size++] = c;
            formatter->output_buffer[formatter->output_size++] = '\n';
        } else if (c == ';') {
            formatter->output_buffer[formatter->output_size++] = c;
            formatter->output_buffer[formatter->output_size++] = '\n';
        } else if (c == ',' && formatter->config.space_after_comma) {
            formatter->output_buffer[formatter->output_size++] = c;
            formatter->output_buffer[formatter->output_size++] = ' ';
        } else {
            formatter->output_buffer[formatter->output_size++] = c;
        }
    }
    
    formatter->output_buffer[formatter->output_size] = '\0';
    *output = strdup(formatter->output_buffer);
    
    return true;
}

bool wyn_formatter_format_file(WynFormatter* formatter, const char* input_file, const char* output_file) {
    if (!formatter || !input_file) return false;
    
    char* content = wyn_devtools_read_file(input_file);
    if (!content) return false;
    
    char* formatted = NULL;
    bool result = wyn_formatter_format_string(formatter, content, &formatted);
    
    if (result && formatted) {
        const char* target_file = output_file ? output_file : input_file;
        result = wyn_devtools_write_file(target_file, formatted);
        free(formatted);
    }
    
    free(content);
    return result;
}

// Documentation generator implementation
WynDocGenerator* wyn_doc_generator_new(void) {
    WynDocGenerator* generator = malloc(sizeof(WynDocGenerator));
    if (!generator) return NULL;
    
    memset(generator, 0, sizeof(WynDocGenerator));
    generator->config = wyn_doc_config_default();
    
    return generator;
}

void wyn_doc_generator_free(WynDocGenerator* generator) {
    if (!generator) return;
    
    free(generator->config.output_dir);
    free(generator->config.template_dir);
    free(generator->config.project_name);
    free(generator->config.project_version);
    free(generator->current_module);
    
    for (size_t i = 0; i < generator->source_file_count; i++) {
        free(generator->source_files[i]);
    }
    free(generator->source_files);
    
    if (generator->output_file) {
        fclose(generator->output_file);
    }
    
    free(generator);
}

WynDocConfig wyn_doc_config_default(void) {
    WynDocConfig config;
    memset(&config, 0, sizeof(WynDocConfig));
    
    config.output_dir = strdup("docs");
    config.include_private = false;
    config.generate_index = true;
    config.markdown_output = true;
    config.html_output = false;
    config.project_name = strdup("Wyn Project");
    config.project_version = strdup("1.0.0");
    
    return config;
}

bool wyn_doc_generator_add_source(WynDocGenerator* generator, const char* source_file) {
    if (!generator || !source_file) return false;
    
    generator->source_files = realloc(generator->source_files, 
                                     (generator->source_file_count + 1) * sizeof(char*));
    if (!generator->source_files) return false;
    
    generator->source_files[generator->source_file_count] = strdup(source_file);
    generator->source_file_count++;
    
    return true;
}

bool wyn_doc_generator_generate_file(WynDocGenerator* generator, const char* source_file) {
    if (!generator || !source_file) return false;
    
    char* content = wyn_devtools_read_file(source_file);
    if (!content) return false;
    
    // Extract filename for output
    const char* filename = strrchr(source_file, '/');
    if (!filename) filename = source_file;
    else filename++; // Skip the '/'
    
    // Create output filename
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s.md", 
             generator->config.output_dir, filename);
    
    generator->output_file = fopen(output_path, "w");
    if (!generator->output_file) {
        free(content);
        return false;
    }
    
    // Generate documentation header
    fprintf(generator->output_file, "# %s\n\n", filename);
    fprintf(generator->output_file, "Generated from: `%s`\n\n", source_file);
    
    // Parse content for documentation
    char* line = strtok(content, "\n");
    bool in_doc_comment = false;
    
    while (line) {
        // Check for documentation comments
        if (strstr(line, "///") == line) {
            if (!in_doc_comment) {
                fprintf(generator->output_file, "## Documentation\n\n");
                in_doc_comment = true;
            }
            // Remove /// prefix and write
            char* doc_text = line + 3;
            while (*doc_text == ' ') doc_text++; // Skip leading spaces
            fprintf(generator->output_file, "%s\n", doc_text);
        } else if (strstr(line, "fn ") || strstr(line, "struct ") || strstr(line, "impl ")) {
            if (in_doc_comment) {
                fprintf(generator->output_file, "\n");
                in_doc_comment = false;
            }
            fprintf(generator->output_file, "### %s\n\n", line);
            fprintf(generator->output_file, "```wyn\n%s\n```\n\n", line);
        }
        
        line = strtok(NULL, "\n");
    }
    
    fclose(generator->output_file);
    generator->output_file = NULL;
    free(content);
    
    return true;
}

bool wyn_doc_generator_generate(WynDocGenerator* generator) {
    if (!generator) return false;
    
    // Create output directory
    wyn_devtools_create_directory(generator->config.output_dir);
    
    // Generate documentation for each source file
    for (size_t i = 0; i < generator->source_file_count; i++) {
        if (!wyn_doc_generator_generate_file(generator, generator->source_files[i])) {
            return false;
        }
    }
    
    // Generate index file if requested
    if (generator->config.generate_index) {
        char index_path[512];
        snprintf(index_path, sizeof(index_path), "%s/README.md", generator->config.output_dir);
        
        FILE* index_file = fopen(index_path, "w");
        if (index_file) {
            fprintf(index_file, "# %s Documentation\n\n", generator->config.project_name);
            fprintf(index_file, "Version: %s\n\n", generator->config.project_version);
            fprintf(index_file, "## Modules\n\n");
            
            for (size_t i = 0; i < generator->source_file_count; i++) {
                const char* filename = strrchr(generator->source_files[i], '/');
                if (!filename) filename = generator->source_files[i];
                else filename++;
                
                fprintf(index_file, "- [%s](%s.md)\n", filename, filename);
            }
            
            fclose(index_file);
        }
    }
    
    return true;
}

// REPL implementation
WynRepl* wyn_repl_new(void) {
    WynRepl* repl = malloc(sizeof(WynRepl));
    if (!repl) return NULL;
    
    memset(repl, 0, sizeof(WynRepl));
    repl->config = wyn_repl_config_default();
    repl->history_capacity = repl->config.history_size;
    repl->history = malloc(repl->history_capacity * sizeof(char*));
    repl->input_capacity = 1024;
    repl->input_buffer = malloc(repl->input_capacity);
    
    return repl;
}

void wyn_repl_free(WynRepl* repl) {
    if (!repl) return;
    
    free(repl->config.prompt);
    free(repl->config.continuation_prompt);
    
    for (size_t i = 0; i < repl->history_count; i++) {
        free(repl->history[i]);
    }
    free(repl->history);
    free(repl->input_buffer);
    free(repl);
}

WynReplConfig wyn_repl_config_default(void) {
    WynReplConfig config;
    memset(&config, 0, sizeof(WynReplConfig));
    
    config.show_types = true;
    config.show_memory_usage = false;
    config.auto_complete = true;
    config.history_size = 1000;
    config.prompt = strdup("wyn> ");
    config.continuation_prompt = strdup("...> ");
    
    return config;
}

bool wyn_repl_start(WynRepl* repl) {
    if (!repl) return false;
    
    repl->running = true;
    
    printf("Wyn REPL v1.0.0\n");
    printf("Type 'exit' or 'quit' to exit, 'help' for help\n\n");
    
    while (repl->running) {
        printf("%s", repl->config.prompt);
        fflush(stdout);
        
        if (fgets(repl->input_buffer, repl->input_capacity, stdin)) {
            // Remove newline
            size_t len = strlen(repl->input_buffer);
            if (len > 0 && repl->input_buffer[len - 1] == '\n') {
                repl->input_buffer[len - 1] = '\0';
            }
            
            if (!wyn_repl_execute_line(repl, repl->input_buffer)) {
                break;
            }
        } else {
            break;
        }
    }
    
    return true;
}

bool wyn_repl_execute_line(WynRepl* repl, const char* line) {
    if (!repl || !line) return false;
    
    // Skip empty lines
    if (strlen(line) == 0) return true;
    
    // Add to history
    wyn_repl_add_history(repl, line);
    
    // Handle built-in commands
    if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
        repl->running = false;
        return false;
    }
    
    if (strcmp(line, "help") == 0) {
        printf("Wyn REPL Commands:\n");
        printf("  help     - Show this help\n");
        printf("  exit     - Exit the REPL\n");
        printf("  quit     - Exit the REPL\n");
        printf("  history  - Show command history\n");
        printf("  clear    - Clear the screen\n");
        return true;
    }
    
    if (strcmp(line, "history") == 0) {
        printf("Command history:\n");
        for (size_t i = 0; i < repl->history_count; i++) {
            printf("  %zu: %s\n", i + 1, repl->history[i]);
        }
        return true;
    }
    
    if (strcmp(line, "clear") == 0) {
        printf("\033[2J\033[H"); // ANSI escape codes to clear screen
        return true;
    }
    
    // Simple expression evaluation (stub)
    printf("Evaluating: %s\n", line);
    printf("Result: (not implemented yet)\n");
    
    return true;
}

bool wyn_repl_add_history(WynRepl* repl, const char* line) {
    if (!repl || !line) return false;
    
    // Don't add empty lines or duplicates
    if (strlen(line) == 0) return true;
    if (repl->history_count > 0 && strcmp(repl->history[repl->history_count - 1], line) == 0) {
        return true;
    }
    
    // Add to history
    if (repl->history_count < repl->history_capacity) {
        repl->history[repl->history_count] = strdup(line);
        repl->history_count++;
    } else {
        // Shift history and add new entry
        free(repl->history[0]);
        for (size_t i = 1; i < repl->history_capacity; i++) {
            repl->history[i - 1] = repl->history[i];
        }
        repl->history[repl->history_capacity - 1] = strdup(line);
    }
    
    return true;
}

// Utility functions
bool wyn_devtools_is_wyn_file(const char* filename) {
    if (!filename) return false;
    
    const char* ext = strrchr(filename, '.');
    return ext && strcmp(ext, ".wyn") == 0;
}

char* wyn_devtools_read_file(const char* filename) {
    if (!filename) return NULL;
    
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, size, file);
    content[size] = '\0';
    
    fclose(file);
    return content;
}

bool wyn_devtools_write_file(const char* filename, const char* content) {
    if (!filename || !content) return false;
    
    FILE* file = fopen(filename, "w");
    if (!file) return false;
    
    fputs(content, file);
    fclose(file);
    
    return true;
}

bool wyn_devtools_create_directory(const char* path) {
    if (!path) return false;
    
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", path);
    
    return system(mkdir_cmd) == 0;
}

// Command-line interface functions
int wyn_fmt_main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: wyn fmt <file.wyn>\n");
        return 1;
    }
    
    WynFormatter* formatter = wyn_formatter_new();
    if (!formatter) {
        printf("Error: Failed to create formatter\n");
        return 1;
    }
    
    bool result = wyn_formatter_format_file(formatter, argv[1], NULL);
    
    if (result) {
        printf("Formatted: %s\n", argv[1]);
    } else {
        printf("Error: Failed to format %s\n", argv[1]);
    }
    
    wyn_formatter_free(formatter);
    return result ? 0 : 1;
}

int wyn_doc_main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: wyn doc <source_files...>\n");
        return 1;
    }
    
    WynDocGenerator* generator = wyn_doc_generator_new();
    if (!generator) {
        printf("Error: Failed to create documentation generator\n");
        return 1;
    }
    
    // Add all source files
    for (int i = 1; i < argc; i++) {
        wyn_doc_generator_add_source(generator, argv[i]);
    }
    
    bool result = wyn_doc_generator_generate(generator);
    
    if (result) {
        printf("Documentation generated in: %s\n", generator->config.output_dir);
    } else {
        printf("Error: Failed to generate documentation\n");
    }
    
    wyn_doc_generator_free(generator);
    return result ? 0 : 1;
}

int wyn_repl_main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    WynRepl* repl = wyn_repl_new();
    if (!repl) {
        printf("Error: Failed to create REPL\n");
        return 1;
    }
    
    bool result = wyn_repl_start(repl);
    
    wyn_repl_free(repl);
    return result ? 0 : 1;
}

// Stub implementations for unimplemented features
bool wyn_formatter_load_config(WynFormatter* formatter, const char* config_file) {
    (void)formatter; (void)config_file;
    return false; // Stub
}

bool wyn_formatter_save_config(WynFormatter* formatter, const char* config_file) {
    (void)formatter; (void)config_file;
    return false; // Stub
}

bool wyn_formatter_format_directory(WynFormatter* formatter, const char* directory, bool recursive) {
    (void)formatter; (void)directory; (void)recursive;
    return false; // Stub
}

bool wyn_doc_generator_load_config(WynDocGenerator* generator, const char* config_file) {
    (void)generator; (void)config_file;
    return false; // Stub
}

bool wyn_doc_generator_save_config(WynDocGenerator* generator, const char* config_file) {
    (void)generator; (void)config_file;
    return false; // Stub
}

bool wyn_doc_generator_generate_module(WynDocGenerator* generator, const char* module_name) {
    (void)generator; (void)module_name;
    return false; // Stub
}

bool wyn_repl_load_config(WynRepl* repl, const char* config_file) {
    (void)repl; (void)config_file;
    return false; // Stub
}

bool wyn_repl_save_config(WynRepl* repl, const char* config_file) {
    (void)repl; (void)config_file;
    return false; // Stub
}

bool wyn_repl_execute_file(WynRepl* repl, const char* file_path) {
    (void)repl; (void)file_path;
    return false; // Stub
}

char** wyn_repl_get_completions(WynRepl* repl, const char* partial, size_t* count) {
    (void)repl; (void)partial;
    if (count) *count = 0;
    return NULL; // Stub
}

char** wyn_devtools_list_files(const char* directory, const char* extension, size_t* count) {
    (void)directory; (void)extension;
    if (count) *count = 0;
    return NULL; // Stub
}

bool wyn_devtools_init_project(const char* project_dir) {
    (void)project_dir;
    return false; // Stub
}

bool wyn_devtools_setup_vscode(const char* project_dir) {
    (void)project_dir;
    return false; // Stub
}

bool wyn_devtools_setup_vim(const char* project_dir) {
    (void)project_dir;
    return false; // Stub
}
