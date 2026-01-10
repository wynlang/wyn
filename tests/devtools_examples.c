#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/devtools.h"

void example_code_formatter() {
    printf("=== Code Formatter Example ===\n");
    
    // Create formatter with default configuration
    WynFormatter* formatter = wyn_formatter_new();
    
    printf("Formatter configuration:\n");
    printf("  - Indent size: %d\n", formatter->config.indent_size);
    printf("  - Use tabs: %s\n", formatter->config.use_tabs ? "yes" : "no");
    printf("  - Max line length: %d\n", formatter->config.max_line_length);
    printf("  - Space after comma: %s\n", formatter->config.space_after_comma ? "yes" : "no");
    
    // Example unformatted code
    const char* unformatted = 
        "fn calculate(x:i32,y:i32)->i32{if x>y{return x*2;}else{return y*2;}}";
    
    printf("\nUnformatted code:\n%s\n", unformatted);
    
    // Format the code
    char* formatted = NULL;
    bool result = wyn_formatter_format_string(formatter, unformatted, &formatted);
    
    if (result && formatted) {
        printf("\nFormatted code:\n%s\n", formatted);
        free(formatted);
    } else {
        printf("Error: Failed to format code\n");
    }
    
    // Customize configuration
    printf("\nCustomizing formatter settings...\n");
    formatter->config.indent_size = 2;
    formatter->config.use_tabs = true;
    formatter->config.newline_before_brace = true;
    
    result = wyn_formatter_format_string(formatter, unformatted, &formatted);
    if (result && formatted) {
        printf("Formatted with custom settings:\n%s\n", formatted);
        free(formatted);
    }
    
    wyn_formatter_free(formatter);
    printf("\n");
}

void example_documentation_generator() {
    printf("=== Documentation Generator Example ===\n");
    
    // Create documentation generator
    WynDocGenerator* generator = wyn_doc_generator_new();
    
    printf("Documentation configuration:\n");
    printf("  - Output directory: %s\n", generator->config.output_dir);
    printf("  - Include private: %s\n", generator->config.include_private ? "yes" : "no");
    printf("  - Generate index: %s\n", generator->config.generate_index ? "yes" : "no");
    printf("  - Markdown output: %s\n", generator->config.markdown_output ? "yes" : "no");
    printf("  - Project name: %s\n", generator->config.project_name);
    printf("  - Project version: %s\n", generator->config.project_version);
    
    // Create sample source file for documentation
    const char* sample_code = 
        "/// This is a sample module for mathematical operations\n"
        "/// It provides basic arithmetic functions\n"
        "\n"
        "/// Adds two integers together\n"
        "/// Returns the sum of x and y\n"
        "fn add(x: i32, y: i32) -> i32 {\n"
        "    x + y\n"
        "}\n"
        "\n"
        "/// Represents a point in 2D space\n"
        "struct Point {\n"
        "    x: f64,\n"
        "    y: f64,\n"
        "}\n"
        "\n"
        "impl Point {\n"
        "    /// Creates a new point at the origin\n"
        "    fn new() -> Point {\n"
        "        Point { x: 0.0, y: 0.0 }\n"
        "    }\n"
        "}\n";
    
    // Write sample file
    const char* sample_file = "/tmp/math_module.wyn";
    wyn_devtools_write_file(sample_file, sample_code);
    
    // Add source file to generator
    wyn_doc_generator_add_source(generator, sample_file);
    
    printf("\nAdded source file: %s\n", sample_file);
    printf("Source files to document: %zu\n", generator->source_file_count);
    
    // Generate documentation
    bool result = wyn_doc_generator_generate(generator);
    
    if (result) {
        printf("Documentation generated successfully!\n");
        printf("Check the '%s' directory for output files\n", generator->config.output_dir);
        
        // Show what would be generated
        printf("\nGenerated files:\n");
        printf("  - %s/README.md (index)\n", generator->config.output_dir);
        printf("  - %s/math_module.wyn.md (module docs)\n", generator->config.output_dir);
    } else {
        printf("Error: Failed to generate documentation\n");
    }
    
    wyn_doc_generator_free(generator);
    printf("\n");
}

void example_repl_session() {
    printf("=== REPL Example ===\n");
    
    // Create REPL
    WynRepl* repl = wyn_repl_new();
    
    printf("REPL configuration:\n");
    printf("  - Show types: %s\n", repl->config.show_types ? "yes" : "no");
    printf("  - Auto complete: %s\n", repl->config.auto_complete ? "yes" : "no");
    printf("  - History size: %d\n", repl->config.history_size);
    printf("  - Prompt: '%s'\n", repl->config.prompt);
    
    // Simulate REPL commands (without starting interactive mode)
    printf("\nSimulating REPL session:\n");
    
    // Execute some commands
    printf("%s", repl->config.prompt);
    printf("let x = 42\n");
    wyn_repl_execute_line(repl, "let x = 42");
    
    printf("%s", repl->config.prompt);
    printf("let y = x * 2\n");
    wyn_repl_execute_line(repl, "let y = x * 2");
    
    printf("%s", repl->config.prompt);
    printf("println(y)\n");
    wyn_repl_execute_line(repl, "println(y)");
    
    printf("%s", repl->config.prompt);
    printf("help\n");
    wyn_repl_execute_line(repl, "help");
    
    printf("%s", repl->config.prompt);
    printf("history\n");
    wyn_repl_execute_line(repl, "history");
    
    printf("\nRELP history contains %zu entries\n", repl->history_count);
    
    wyn_repl_free(repl);
    printf("\n");
}

void example_file_operations() {
    printf("=== File Operations Example ===\n");
    
    // Test file type detection
    printf("File type detection:\n");
    printf("  - main.wyn: %s\n", wyn_devtools_is_wyn_file("main.wyn") ? "Wyn file" : "Not Wyn file");
    printf("  - utils.c: %s\n", wyn_devtools_is_wyn_file("utils.c") ? "Wyn file" : "Not Wyn file");
    printf("  - lib.wyn: %s\n", wyn_devtools_is_wyn_file("lib.wyn") ? "Wyn file" : "Not Wyn file");
    
    // Test file I/O
    const char* test_file = "/tmp/devtools_example.wyn";
    const char* test_content = 
        "// Example Wyn program\n"
        "fn main() {\n"
        "    let message = \"Hello from Wyn!\";\n"
        "    println(message);\n"
        "}\n";
    
    printf("\nFile I/O operations:\n");
    printf("Writing to: %s\n", test_file);
    
    bool result = wyn_devtools_write_file(test_file, test_content);
    if (result) {
        printf("File written successfully\n");
        
        char* read_content = wyn_devtools_read_file(test_file);
        if (read_content) {
            printf("File content:\n%s", read_content);
            free(read_content);
        } else {
            printf("Error: Failed to read file\n");
        }
    } else {
        printf("Error: Failed to write file\n");
    }
    
    // Test directory creation
    const char* test_dir = "/tmp/wyn_project_example";
    printf("\nCreating directory: %s\n", test_dir);
    result = wyn_devtools_create_directory(test_dir);
    printf("Directory creation: %s\n", result ? "success" : "failed");
    
    printf("\n");
}

void example_command_line_tools() {
    printf("=== Command-Line Tools Example ===\n");
    
    // Create a sample file to format
    const char* sample_file = "/tmp/format_example.wyn";
    const char* unformatted_code = 
        "fn fibonacci(n:u32)->u32{if n<=1{return n;}return fibonacci(n-1)+fibonacci(n-2);}";
    
    wyn_devtools_write_file(sample_file, unformatted_code);
    
    printf("Sample file created: %s\n", sample_file);
    printf("Original content: %s\n", unformatted_code);
    
    // Demonstrate formatter CLI
    printf("\nFormatter CLI usage:\n");
    printf("Command: wyn fmt %s\n", sample_file);
    
    char* fmt_argv[] = {"wyn_fmt", (char*)sample_file};
    int result = wyn_fmt_main(2, fmt_argv);
    printf("Formatter result: %s\n", result == 0 ? "success" : "failed");
    
    // Show formatted content
    char* formatted_content = wyn_devtools_read_file(sample_file);
    if (formatted_content) {
        printf("Formatted content:\n%s\n", formatted_content);
        free(formatted_content);
    }
    
    // Demonstrate documentation CLI
    printf("\nDocumentation CLI usage:\n");
    printf("Command: wyn doc %s\n", sample_file);
    
    char* doc_argv[] = {"wyn_doc", (char*)sample_file};
    result = wyn_doc_main(2, doc_argv);
    printf("Documentation result: %s\n", result == 0 ? "success" : "failed");
    
    printf("\nREPL CLI usage:\n");
    printf("Command: wyn repl\n");
    printf("(Interactive mode - not demonstrated in this example)\n");
    
    printf("\n");
}

void example_configuration_management() {
    printf("=== Configuration Management Example ===\n");
    
    // Formatter configuration
    printf("Formatter configurations:\n");
    
    WynFormatterConfig default_fmt = wyn_formatter_config_default();
    printf("  Default: indent=%d, tabs=%s, line_length=%d\n",
           default_fmt.indent_size,
           default_fmt.use_tabs ? "yes" : "no",
           default_fmt.max_line_length);
    
    WynFormatterConfig compact_fmt = wyn_formatter_config_default();
    compact_fmt.indent_size = 2;
    compact_fmt.max_line_length = 80;
    compact_fmt.space_after_comma = false;
    printf("  Compact: indent=%d, tabs=%s, line_length=%d\n",
           compact_fmt.indent_size,
           compact_fmt.use_tabs ? "yes" : "no",
           compact_fmt.max_line_length);
    
    // Documentation configuration
    printf("\nDocumentation configurations:\n");
    
    WynDocConfig default_doc = wyn_doc_config_default();
    printf("  Default: output='%s', private=%s, index=%s\n",
           default_doc.output_dir,
           default_doc.include_private ? "yes" : "no",
           default_doc.generate_index ? "yes" : "no");
    
    WynDocConfig custom_doc = wyn_doc_config_default();
    free(custom_doc.output_dir);
    custom_doc.output_dir = strdup("api-docs");
    custom_doc.include_private = true;
    custom_doc.html_output = true;
    printf("  Custom: output='%s', private=%s, html=%s\n",
           custom_doc.output_dir,
           custom_doc.include_private ? "yes" : "no",
           custom_doc.html_output ? "yes" : "no");
    
    // REPL configuration
    printf("\nREPL configurations:\n");
    
    WynReplConfig default_repl = wyn_repl_config_default();
    printf("  Default: types=%s, complete=%s, history=%d\n",
           default_repl.show_types ? "yes" : "no",
           default_repl.auto_complete ? "yes" : "no",
           default_repl.history_size);
    
    WynReplConfig minimal_repl = wyn_repl_config_default();
    minimal_repl.show_types = false;
    minimal_repl.show_memory_usage = false;
    minimal_repl.auto_complete = false;
    minimal_repl.history_size = 100;
    printf("  Minimal: types=%s, complete=%s, history=%d\n",
           minimal_repl.show_types ? "yes" : "no",
           minimal_repl.auto_complete ? "yes" : "no",
           minimal_repl.history_size);
    
    // Clean up allocated strings
    free(default_doc.output_dir);
    free(default_doc.project_name);
    free(default_doc.project_version);
    free(custom_doc.output_dir);
    free(custom_doc.project_name);
    free(custom_doc.project_version);
    free(default_repl.prompt);
    free(default_repl.continuation_prompt);
    free(minimal_repl.prompt);
    free(minimal_repl.continuation_prompt);
    
    printf("\n");
}

int main() {
    printf("Wyn Development Tools Examples\n");
    printf("==============================\n\n");
    
    example_code_formatter();
    example_documentation_generator();
    example_repl_session();
    example_file_operations();
    example_command_line_tools();
    example_configuration_management();
    
    printf("Development Tools Features:\n");
    printf("  ✓ Code formatter with configurable style options\n");
    printf("  ✓ Documentation generator with markdown output\n");
    printf("  ✓ Interactive REPL with history and built-in commands\n");
    printf("  ✓ File and directory utilities for project management\n");
    printf("  ✓ Command-line interfaces for all development tools\n");
    printf("  ✓ Configurable behavior for different development workflows\n");
    printf("  ✓ Integration-ready for IDEs and build systems\n");
    printf("\nComplete development toolchain for Wyn language!\n");
    
    return 0;
}
