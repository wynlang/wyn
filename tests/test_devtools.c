#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/devtools.h"

void test_formatter() {
    printf("Testing code formatter...\n");
    
    // Create formatter
    WynFormatter* formatter = wyn_formatter_new();
    assert(formatter != NULL);
    
    // Test default configuration
    assert(formatter->config.indent_size == 4);
    assert(formatter->config.use_tabs == false);
    assert(formatter->config.max_line_length == 100);
    assert(formatter->config.space_after_comma == true);
    
    // Test simple formatting
    const char* input = "fn main(){let x=1,y=2;if(x>0){println(\"hello\");}}";
    char* output = NULL;
    
    bool result = wyn_formatter_format_string(formatter, input, &output);
    assert(result == true);
    assert(output != NULL);
    
    printf("Input:  %s\n", input);
    printf("Output: %s\n", output);
    
    // Check that braces are formatted
    assert(strstr(output, "{\n") != NULL);
    assert(strstr(output, "}\n") != NULL);
    
    free(output);
    wyn_formatter_free(formatter);
    
    printf("✓ Formatter tests passed\n");
}

void test_doc_generator() {
    printf("Testing documentation generator...\n");
    
    // Create doc generator
    WynDocGenerator* generator = wyn_doc_generator_new();
    assert(generator != NULL);
    
    // Test default configuration
    assert(generator->config.output_dir != NULL);
    assert(strcmp(generator->config.output_dir, "docs") == 0);
    assert(generator->config.include_private == false);
    assert(generator->config.generate_index == true);
    assert(generator->config.markdown_output == true);
    
    // Add source files
    bool result = wyn_doc_generator_add_source(generator, "test1.wyn");
    assert(result == true);
    assert(generator->source_file_count == 1);
    
    result = wyn_doc_generator_add_source(generator, "test2.wyn");
    assert(result == true);
    assert(generator->source_file_count == 2);
    
    // Test source file storage
    assert(strcmp(generator->source_files[0], "test1.wyn") == 0);
    assert(strcmp(generator->source_files[1], "test2.wyn") == 0);
    
    wyn_doc_generator_free(generator);
    
    printf("✓ Documentation generator tests passed\n");
}

void test_repl() {
    printf("Testing REPL...\n");
    
    // Create REPL
    WynRepl* repl = wyn_repl_new();
    assert(repl != NULL);
    
    // Test default configuration
    assert(repl->config.show_types == true);
    assert(repl->config.auto_complete == true);
    assert(repl->config.history_size == 1000);
    assert(repl->config.prompt != NULL);
    assert(strcmp(repl->config.prompt, "wyn> ") == 0);
    
    // Test history management
    bool result = wyn_repl_add_history(repl, "let x = 42");
    assert(result == true);
    assert(repl->history_count == 1);
    assert(strcmp(repl->history[0], "let x = 42") == 0);
    
    result = wyn_repl_add_history(repl, "println(x)");
    assert(result == true);
    assert(repl->history_count == 2);
    
    // Test duplicate prevention
    result = wyn_repl_add_history(repl, "println(x)");
    assert(result == true);
    assert(repl->history_count == 2); // Should not increase
    
    // Test empty line prevention
    result = wyn_repl_add_history(repl, "");
    assert(result == true);
    assert(repl->history_count == 2); // Should not increase
    
    wyn_repl_free(repl);
    
    printf("✓ REPL tests passed\n");
}

void test_utility_functions() {
    printf("Testing utility functions...\n");
    
    // Test file extension checking
    assert(wyn_devtools_is_wyn_file("test.wyn") == true);
    assert(wyn_devtools_is_wyn_file("test.c") == false);
    assert(wyn_devtools_is_wyn_file("main.wyn") == true);
    assert(wyn_devtools_is_wyn_file("noextension") == false);
    
    // Test file operations (create a temporary file)
    const char* test_content = "fn main() {\n    println(\"Hello, World!\");\n}";
    const char* test_file = "/tmp/test_devtools.wyn";
    
    bool result = wyn_devtools_write_file(test_file, test_content);
    assert(result == true);
    
    char* read_content = wyn_devtools_read_file(test_file);
    assert(read_content != NULL);
    assert(strcmp(read_content, test_content) == 0);
    
    free(read_content);
    
    // Test directory creation
    result = wyn_devtools_create_directory("/tmp/test_devtools_dir");
    assert(result == true);
    
    printf("✓ Utility function tests passed\n");
}

void test_formatter_config() {
    printf("Testing formatter configuration...\n");
    
    WynFormatterConfig config = wyn_formatter_config_default();
    
    // Test default values
    assert(config.indent_size == 4);
    assert(config.use_tabs == false);
    assert(config.max_line_length == 100);
    assert(config.space_before_paren == false);
    assert(config.space_after_comma == true);
    assert(config.newline_before_brace == false);
    assert(config.align_assignments == true);
    assert(config.sort_imports == true);
    
    // Test configuration modification
    config.indent_size = 2;
    config.use_tabs = true;
    config.max_line_length = 80;
    
    assert(config.indent_size == 2);
    assert(config.use_tabs == true);
    assert(config.max_line_length == 80);
    
    printf("✓ Formatter configuration tests passed\n");
}

void test_doc_config() {
    printf("Testing documentation configuration...\n");
    
    WynDocConfig config = wyn_doc_config_default();
    
    // Test default values
    assert(config.output_dir != NULL);
    assert(strcmp(config.output_dir, "docs") == 0);
    assert(config.include_private == false);
    assert(config.generate_index == true);
    assert(config.markdown_output == true);
    assert(config.html_output == false);
    assert(config.project_name != NULL);
    assert(strcmp(config.project_name, "Wyn Project") == 0);
    assert(config.project_version != NULL);
    assert(strcmp(config.project_version, "1.0.0") == 0);
    
    // Clean up allocated strings
    free(config.output_dir);
    free(config.project_name);
    free(config.project_version);
    
    printf("✓ Documentation configuration tests passed\n");
}

void test_repl_config() {
    printf("Testing REPL configuration...\n");
    
    WynReplConfig config = wyn_repl_config_default();
    
    // Test default values
    assert(config.show_types == true);
    assert(config.show_memory_usage == false);
    assert(config.auto_complete == true);
    assert(config.history_size == 1000);
    assert(config.prompt != NULL);
    assert(strcmp(config.prompt, "wyn> ") == 0);
    assert(config.continuation_prompt != NULL);
    assert(strcmp(config.continuation_prompt, "...> ") == 0);
    
    // Clean up allocated strings
    free(config.prompt);
    free(config.continuation_prompt);
    
    printf("✓ REPL configuration tests passed\n");
}

void test_command_line_interfaces() {
    printf("Testing command-line interfaces...\n");
    
    // Test formatter CLI (with invalid args)
    char* fmt_argv[] = {"wyn_fmt"};
    int result = wyn_fmt_main(1, fmt_argv);
    assert(result == 1); // Should fail with usage message
    
    // Test doc generator CLI (with invalid args)
    char* doc_argv[] = {"wyn_doc"};
    result = wyn_doc_main(1, doc_argv);
    assert(result == 1); // Should fail with usage message
    
    // Note: We don't test wyn_repl_main as it would start an interactive session
    
    printf("✓ Command-line interface tests passed\n");
}

int main() {
    printf("Running Development Tools Tests\n");
    printf("===============================\n\n");
    
    test_formatter();
    test_doc_generator();
    test_repl();
    test_utility_functions();
    test_formatter_config();
    test_doc_config();
    test_repl_config();
    test_command_line_interfaces();
    
    printf("\n✓ All development tools tests passed!\n");
    printf("Development tools provide:\n");
    printf("  - Code formatter with configurable style\n");
    printf("  - Documentation generator with markdown output\n");
    printf("  - Interactive REPL with history and commands\n");
    printf("  - File and directory utilities\n");
    printf("  - Command-line interfaces for all tools\n");
    printf("  - Configurable behavior for different workflows\n");
    
    return 0;
}
