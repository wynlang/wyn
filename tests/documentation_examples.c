#include "../src/documentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example: Creating a language reference
void example_language_reference() {
    printf("=== Language Reference Example ===\n");
    
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    
    // Add sections to the reference
    wyn_language_reference_add_section(reference, "Variables and Types", 
        "Wyn supports several built-in types:\n"
        "- `int`: 32-bit signed integers\n"
        "- `float`: 64-bit floating point numbers\n"
        "- `string`: UTF-8 encoded strings\n"
        "- `bool`: Boolean values (true/false)\n\n"
        "Variables are declared using the `var` keyword:");
    
    wyn_language_reference_add_section(reference, "Functions",
        "Functions in Wyn are declared using the `fn` keyword:\n\n"
        "```wyn\n"
        "fn add(a: int, b: int) -> int {\n"
        "    return a + b;\n"
        "}\n"
        "```");
    
    wyn_language_reference_add_section(reference, "Control Flow",
        "Wyn supports standard control flow constructs:\n\n"
        "**If Statements:**\n"
        "```wyn\n"
        "if condition {\n"
        "    // code\n"
        "} else {\n"
        "    // alternative code\n"
        "}\n"
        "```\n\n"
        "**Loops:**\n"
        "```wyn\n"
        "for i in 0..10 {\n"
        "    print(i);\n"
        "}\n"
        "```");
    
    // Generate syntax reference automatically
    wyn_generate_syntax_reference(reference);
    
    printf("Created language reference with %zu sections:\n", reference->section_count);
    for (size_t i = 0; i < reference->section_count; i++) {
        printf("  %zu. %s\n", i + 1, reference->sections[i].title);
    }
    
    // Generate the reference document
    const char* output_file = "/tmp/wyn_language_reference.md";
    if (wyn_language_reference_generate(reference, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Language reference generated: %s\n", output_file);
    }
    
    wyn_language_reference_free(reference);
}

// Example: Creating tutorials
void example_tutorial_creation() {
    printf("\n=== Tutorial Creation Example ===\n");
    
    WynTutorial* tutorial = wyn_tutorial_new("Wyn for Beginners", 
        "A comprehensive introduction to the Wyn programming language", "beginner");
    
    // Add lessons
    wyn_tutorial_add_lesson(tutorial, "Getting Started",
        "Welcome to Wyn! In this lesson, you'll learn how to:\n"
        "- Install the Wyn compiler\n"
        "- Set up your development environment\n"
        "- Write your first Wyn program\n\n"
        "Let's start with the classic \"Hello, World!\" program:");
    
    wyn_tutorial_add_lesson(tutorial, "Variables and Data Types",
        "Wyn has a strong, static type system. You'll learn about:\n"
        "- Declaring variables with `var`\n"
        "- Basic data types (int, float, string, bool)\n"
        "- Type inference and explicit typing\n\n"
        "Example:\n"
        "```wyn\n"
        "var name = \"Alice\";  // string (inferred)\n"
        "var age: int = 25;    // explicit type\n"
        "```");
    
    wyn_tutorial_add_lesson(tutorial, "Functions and Control Flow",
        "Learn to structure your code with functions and control flow:\n"
        "- Defining functions with parameters and return types\n"
        "- If statements and conditional logic\n"
        "- Loops and iteration\n\n"
        "Functions are the building blocks of Wyn programs.");
    
    wyn_tutorial_add_lesson(tutorial, "Memory Safety",
        "Wyn's memory management ensures safety without garbage collection:\n"
        "- Automatic Reference Counting (ARC)\n"
        "- Ownership and borrowing concepts\n"
        "- Preventing memory leaks and use-after-free bugs\n\n"
        "Memory safety is built into the language design.");
    
    printf("Created tutorial '%s' with %zu lessons:\n", tutorial->title, tutorial->lesson_count);
    for (size_t i = 0; i < tutorial->lesson_count; i++) {
        printf("  Lesson %zu: %s\n", i + 1, tutorial->lessons[i].title);
    }
    
    // Generate tutorial document
    const char* output_file = "/tmp/wyn_beginner_tutorial.md";
    if (wyn_tutorial_generate(tutorial, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Tutorial generated: %s\n", output_file);
    }
    
    wyn_tutorial_free(tutorial);
}

// Example: Best practices guide
void example_best_practices() {
    printf("\n=== Best Practices Guide Example ===\n");
    
    WynBestPractices* practices = wyn_best_practices_new();
    
    // Set style guide
    wyn_best_practices_set_style_guide(practices,
        "# Wyn Style Guide\n\n"
        "## Formatting\n"
        "- Use 4 spaces for indentation (no tabs)\n"
        "- Maximum line length: 100 characters\n"
        "- Place opening braces on the same line\n\n"
        "## Naming Conventions\n"
        "- Variables and functions: snake_case\n"
        "- Types and structs: PascalCase\n"
        "- Constants: SCREAMING_SNAKE_CASE\n\n"
        "## Comments\n"
        "- Use `//` for single-line comments\n"
        "- Use `/* */` for multi-line comments\n"
        "- Document all public functions");
    
    // Add best practices
    wyn_best_practices_add_practice(practices, "Memory Safety First",
        "Always prioritize memory safety in your code:\n"
        "- Use Wyn's built-in memory management (ARC)\n"
        "- Avoid manual memory management when possible\n"
        "- Be explicit about ownership and lifetimes\n"
        "- Use safe array access methods\n\n"
        "Example:\n"
        "```wyn\n"
        "// Good: Safe array access\n"
        "if index < array.length {\n"
        "    value = array[index];\n"
        "}\n"
        "```");
    
    wyn_best_practices_add_practice(practices, "Error Handling",
        "Handle errors explicitly and gracefully:\n"
        "- Use Result types for operations that can fail\n"
        "- Don't ignore potential errors\n"
        "- Provide meaningful error messages\n"
        "- Use early returns for error conditions\n\n"
        "Example:\n"
        "```wyn\n"
        "fn divide(a: float, b: float) -> Result<float, string> {\n"
        "    if b == 0.0 {\n"
        "        return Err(\"Division by zero\");\n"
        "    }\n"
        "    return Ok(a / b);\n"
        "}\n"
        "```");
    
    wyn_best_practices_add_practice(practices, "Code Organization",
        "Structure your code for maintainability:\n"
        "- Keep functions small and focused\n"
        "- Use meaningful names for variables and functions\n"
        "- Group related functionality into modules\n"
        "- Write self-documenting code\n\n"
        "A well-organized codebase is easier to understand and maintain.");
    
    wyn_best_practices_add_practice(practices, "Performance Considerations",
        "Write efficient code without sacrificing safety:\n"
        "- Prefer stack allocation when possible\n"
        "- Use appropriate data structures for your use case\n"
        "- Profile before optimizing\n"
        "- Leverage Wyn's compile-time optimizations\n\n"
        "Remember: premature optimization is the root of all evil.");
    
    printf("Created best practices guide with %zu practices:\n", practices->practice_count);
    for (size_t i = 0; i < practices->practice_count; i++) {
        printf("  %zu. %s\n", i + 1, practices->practices[i].title);
    }
    
    // Generate best practices document
    const char* output_file = "/tmp/wyn_best_practices.md";
    if (wyn_best_practices_generate(practices, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Best practices guide generated: %s\n", output_file);
    }
    
    wyn_best_practices_free(practices);
}

// Example: Changelog management
void example_changelog() {
    printf("\n=== Changelog Example ===\n");
    
    WynChangelog* changelog = wyn_changelog_new("Wyn Programming Language");
    
    // Add version entries
    wyn_changelog_add_entry(changelog, "1.0.0", "2026-01-09");
    wyn_changelog_add_entry(changelog, "0.9.0", "2025-12-15");
    wyn_changelog_add_entry(changelog, "0.8.0", "2025-11-20");
    
    printf("Created changelog for '%s' with %zu entries:\n", 
           changelog->project_name, changelog->entry_count);
    
    for (size_t i = 0; i < changelog->entry_count; i++) {
        WynChangelogEntry* entry = &changelog->entries[i];
        printf("  Version %s (%s)\n", entry->version, entry->date);
    }
    
    // Generate changelog document
    const char* output_file = "/tmp/wyn_changelog.md";
    if (wyn_changelog_generate(changelog, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Changelog generated: %s\n", output_file);
    }
    
    wyn_changelog_free(changelog);
}

// Example: Complete documentation generation
void example_complete_documentation() {
    printf("\n=== Complete Documentation Generation Example ===\n");
    
    WynDocGenerator* generator = wyn_doc_generator_new();
    
    // Set output directory and format
    wyn_doc_generator_set_output_dir(generator, "/tmp/wyn_docs");
    wyn_doc_generator_set_format(generator, WYN_FORMAT_MARKDOWN);
    
    // Create language reference
    generator->language_ref = wyn_language_reference_new("1.0.0");
    generator->language_ref->introduction = strdup(
        "Wyn is a modern systems programming language that combines the performance of C "
        "with the safety of Rust. It features automatic memory management, a strong type "
        "system, and zero-cost abstractions.");
    
    generator->language_ref->quick_start = strdup(
        "To get started with Wyn:\n\n"
        "1. Install the Wyn compiler: `curl -sSf https://wyn-lang.org/install.sh | sh`\n"
        "2. Create a new file: `hello.wyn`\n"
        "3. Write your first program:\n"
        "   ```wyn\n"
        "   fn main() {\n"
        "       print(\"Hello, World!\");\n"
        "   }\n"
        "   ```\n"
        "4. Compile and run: `wyn run hello.wyn`");
    
    wyn_generate_syntax_reference(generator->language_ref);
    
    // Generate getting started tutorial
    wyn_generate_getting_started_tutorial(generator);
    
    // Create best practices
    generator->best_practices = wyn_best_practices_new();
    wyn_best_practices_add_practice(generator->best_practices, "Safety First", 
        "Always prioritize memory safety and error handling in your Wyn code.");
    
    printf("Documentation generator configured:\n");
    printf("  Output directory: %s\n", generator->output_directory);
    printf("  Format: Markdown\n");
    printf("  Language reference sections: %zu\n", generator->language_ref->section_count);
    printf("  Tutorials: %zu\n", generator->tutorial_count);
    printf("  Best practices: %zu\n", generator->best_practices->practice_count);
    
    // Generate all documentation
    if (wyn_doc_generator_generate_all(generator)) {
        printf("Complete documentation generated successfully!\n");
    }
    
    wyn_doc_generator_free(generator);
}

// Example: Documentation quality validation
void example_documentation_quality() {
    printf("\n=== Documentation Quality Validation Example ===\n");
    
    WynDocSection* section = wyn_doc_section_new("Memory Management", 
        "Wyn uses Automatic Reference Counting (ARC) for memory management. "
        "This provides deterministic memory cleanup without the overhead of "
        "garbage collection. Objects are automatically freed when their "
        "reference count reaches zero.");
    
    // Add code example
    wyn_doc_section_add_example(section, 
        "var data = Array.new(1000);\n"
        "// data is automatically freed when it goes out of scope");
    
    // Validate quality
    WynDocQuality quality = wyn_validate_documentation_quality(section);
    
    printf("Documentation quality assessment:\n");
    printf("  Has description: %s\n", quality.has_description ? "✓" : "✗");
    printf("  Has examples: %s\n", quality.has_examples ? "✓" : "✗");
    printf("  Has cross-references: %s\n", quality.has_cross_references ? "✓" : "✗");
    printf("  Word count: %zu\n", quality.word_count);
    printf("  Readability score: %.1f/10\n", quality.readability_score);
    
    if (quality.has_description && quality.has_examples && quality.word_count > 20) {
        printf("  Overall quality: Good ✓\n");
    } else {
        printf("  Overall quality: Needs improvement ⚠️\n");
    }
    
    wyn_doc_section_free(section);
    free(section);
}

int main() {
    printf("Wyn Documentation System Examples\n");
    printf("=================================\n\n");
    
    example_language_reference();
    example_tutorial_creation();
    example_best_practices();
    example_changelog();
    example_complete_documentation();
    example_documentation_quality();
    
    printf("\n✓ All documentation examples completed!\n");
    return 0;
}
