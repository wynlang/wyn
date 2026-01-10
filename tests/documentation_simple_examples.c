#include "../src/documentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple working examples
void example_basic_documentation() {
    printf("=== Basic Documentation Example ===\n");
    
    // Create language reference
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    
    wyn_language_reference_add_section(reference, "Variables", 
        "Variables in Wyn are declared with the var keyword.");
    
    wyn_language_reference_add_section(reference, "Functions", 
        "Functions are declared with the fn keyword.");
    
    printf("Language reference created with %zu sections\n", reference->section_count);
    
    // Generate file
    const char* output_file = "/tmp/simple_reference.md";
    if (wyn_language_reference_generate(reference, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Reference generated: %s\n", output_file);
    }
    
    wyn_language_reference_free(reference);
}

void example_tutorial() {
    printf("\n=== Tutorial Example ===\n");
    
    WynTutorial* tutorial = wyn_tutorial_new("Basic Tutorial", "Learn Wyn basics", "beginner");
    
    wyn_tutorial_add_lesson(tutorial, "Hello World", "Your first Wyn program");
    wyn_tutorial_add_lesson(tutorial, "Variables", "Working with variables");
    
    printf("Tutorial created with %zu lessons\n", tutorial->lesson_count);
    
    const char* output_file = "/tmp/simple_tutorial.md";
    if (wyn_tutorial_generate(tutorial, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Tutorial generated: %s\n", output_file);
    }
    
    wyn_tutorial_free(tutorial);
}

void example_best_practices() {
    printf("\n=== Best Practices Example ===\n");
    
    WynBestPractices* practices = wyn_best_practices_new();
    
    wyn_best_practices_add_practice(practices, "Memory Safety", "Use safe memory operations");
    wyn_best_practices_add_practice(practices, "Error Handling", "Handle all errors explicitly");
    
    printf("Best practices created with %zu practices\n", practices->practice_count);
    
    const char* output_file = "/tmp/simple_practices.md";
    if (wyn_best_practices_generate(practices, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Best practices generated: %s\n", output_file);
    }
    
    wyn_best_practices_free(practices);
}

void example_changelog() {
    printf("\n=== Changelog Example ===\n");
    
    WynChangelog* changelog = wyn_changelog_new("Wyn Language");
    
    wyn_changelog_add_entry(changelog, "1.0.0", "2026-01-09");
    
    printf("Changelog created with %zu entries\n", changelog->entry_count);
    
    const char* output_file = "/tmp/simple_changelog.md";
    if (wyn_changelog_generate(changelog, output_file, WYN_FORMAT_MARKDOWN)) {
        printf("Changelog generated: %s\n", output_file);
    }
    
    wyn_changelog_free(changelog);
}

int main() {
    printf("Wyn Documentation System - Simple Examples\n");
    printf("==========================================\n\n");
    
    example_basic_documentation();
    example_tutorial();
    example_best_practices();
    example_changelog();
    
    printf("\n✓ All simple documentation examples completed!\n");
    return 0;
}
