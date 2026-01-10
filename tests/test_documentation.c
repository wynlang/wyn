#include "../src/documentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_doc_generator_creation() {
    printf("Testing documentation generator creation...\n");
    
    WynDocGenerator* generator = wyn_doc_generator_new();
    assert(generator != NULL);
    assert(generator->format == WYN_FORMAT_MARKDOWN);
    assert(generator->tutorial_count == 0);
    
    wyn_doc_generator_free(generator);
    printf("✓ Documentation generator creation test passed\n");
}

void test_language_reference() {
    printf("Testing language reference...\n");
    
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    assert(reference != NULL);
    assert(strcmp(reference->version, "1.0.0") == 0);
    assert(reference->section_count == 0);
    
    // Add sections
    assert(wyn_language_reference_add_section(reference, "Variables", "Variables are declared with var keyword") == true);
    assert(wyn_language_reference_add_section(reference, "Functions", "Functions are declared with fn keyword") == true);
    assert(reference->section_count == 2);
    
    // Check section content
    assert(strcmp(reference->sections[0].title, "Variables") == 0);
    assert(strcmp(reference->sections[1].title, "Functions") == 0);
    
    wyn_language_reference_free(reference);
    printf("✓ Language reference test passed\n");
}

void test_tutorial_system() {
    printf("Testing tutorial system...\n");
    
    WynTutorial* tutorial = wyn_tutorial_new("Getting Started", "Learn Wyn basics", "beginner");
    assert(tutorial != NULL);
    assert(strcmp(tutorial->title, "Getting Started") == 0);
    assert(strcmp(tutorial->difficulty_level, "beginner") == 0);
    assert(tutorial->lesson_count == 0);
    
    // Add lessons
    assert(wyn_tutorial_add_lesson(tutorial, "Hello World", "Your first Wyn program") == true);
    assert(wyn_tutorial_add_lesson(tutorial, "Variables", "Working with variables") == true);
    assert(tutorial->lesson_count == 2);
    
    // Check lesson content
    assert(strcmp(tutorial->lessons[0].title, "Hello World") == 0);
    assert(strcmp(tutorial->lessons[1].title, "Variables") == 0);
    
    wyn_tutorial_free(tutorial);
    printf("✓ Tutorial system test passed\n");
}

void test_best_practices() {
    printf("Testing best practices...\n");
    
    WynBestPractices* practices = wyn_best_practices_new();
    assert(practices != NULL);
    assert(practices->practice_count == 0);
    
    // Add practices
    assert(wyn_best_practices_add_practice(practices, "Memory Safety", "Always use safe memory operations") == true);
    assert(wyn_best_practices_add_practice(practices, "Error Handling", "Handle all possible error cases") == true);
    assert(practices->practice_count == 2);
    
    // Set style guide
    assert(wyn_best_practices_set_style_guide(practices, "Use 4 spaces for indentation") == true);
    assert(practices->style_guide != NULL);
    
    wyn_best_practices_free(practices);
    printf("✓ Best practices test passed\n");
}

void test_doc_section_utilities() {
    printf("Testing documentation section utilities...\n");
    
    WynDocSection* section = wyn_doc_section_new("Test Section", "This is test content");
    assert(section != NULL);
    assert(strcmp(section->title, "Test Section") == 0);
    assert(strcmp(section->content, "This is test content") == 0);
    assert(section->example_count == 0);
    
    // Add code example
    assert(wyn_doc_section_add_example(section, "var x = 42;") == true);
    assert(section->example_count == 1);
    assert(strcmp(section->code_examples[0], "var x = 42;") == 0);
    
    wyn_doc_section_free(section);
    free(section);
    printf("✓ Documentation section utilities test passed\n");
}

void test_syntax_reference_generation() {
    printf("Testing syntax reference generation...\n");
    
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    assert(reference != NULL);
    
    // Generate syntax reference
    assert(wyn_generate_syntax_reference(reference) == true);
    assert(reference->section_count > 0);
    
    // Check that sections were added
    bool found_variables = false, found_functions = false, found_control_flow = false;
    for (size_t i = 0; i < reference->section_count; i++) {
        if (strcmp(reference->sections[i].title, "Variables and Types") == 0) {
            found_variables = true;
        }
        if (strcmp(reference->sections[i].title, "Functions") == 0) {
            found_functions = true;
        }
        if (strcmp(reference->sections[i].title, "Control Flow") == 0) {
            found_control_flow = true;
        }
    }
    
    assert(found_variables == true);
    assert(found_functions == true);
    assert(found_control_flow == true);
    
    wyn_language_reference_free(reference);
    printf("✓ Syntax reference generation test passed\n");
}

void test_tutorial_generation() {
    printf("Testing tutorial generation...\n");
    
    WynDocGenerator* generator = wyn_doc_generator_new();
    assert(generator != NULL);
    
    // Generate getting started tutorial
    assert(wyn_generate_getting_started_tutorial(generator) == true);
    assert(generator->tutorial_count == 1);
    
    WynTutorial* tutorial = &generator->tutorials[0];
    assert(strcmp(tutorial->title, "Getting Started with Wyn") == 0);
    assert(tutorial->lesson_count > 0);
    
    wyn_doc_generator_free(generator);
    printf("✓ Tutorial generation test passed\n");
}

void test_changelog_system() {
    printf("Testing changelog system...\n");
    
    WynChangelog* changelog = wyn_changelog_new("Wyn Language");
    assert(changelog != NULL);
    assert(strcmp(changelog->project_name, "Wyn Language") == 0);
    assert(changelog->entry_count == 0);
    
    // Add changelog entry
    assert(wyn_changelog_add_entry(changelog, "1.0.0", "2026-01-09") == true);
    assert(changelog->entry_count == 1);
    
    WynChangelogEntry* entry = &changelog->entries[0];
    assert(strcmp(entry->version, "1.0.0") == 0);
    assert(strcmp(entry->date, "2026-01-09") == 0);
    
    wyn_changelog_free(changelog);
    printf("✓ Changelog system test passed\n");
}

void test_documentation_quality() {
    printf("Testing documentation quality validation...\n");
    
    WynDocSection* section = wyn_doc_section_new("Test", "This is a test section with content");
    assert(section != NULL);
    
    // Add example
    wyn_doc_section_add_example(section, "var x = 42;");
    
    // Validate quality
    WynDocQuality quality = wyn_validate_documentation_quality(section);
    assert(quality.has_description == true);
    assert(quality.has_examples == true);
    assert(quality.word_count > 0);
    assert(quality.readability_score > 0);
    
    wyn_doc_section_free(section);
    free(section);
    printf("✓ Documentation quality validation test passed\n");
}

void test_file_generation() {
    printf("Testing file generation...\n");
    
    // Test language reference generation
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    wyn_language_reference_add_section(reference, "Test Section", "Test content");
    
    const char* test_file = "/tmp/test_reference.md";
    assert(wyn_language_reference_generate(reference, test_file, WYN_FORMAT_MARKDOWN) == true);
    
    // Check file exists and has content
    FILE* file = fopen(test_file, "r");
    assert(file != NULL);
    
    char buffer[256];
    assert(fgets(buffer, sizeof(buffer), file) != NULL);
    assert(strstr(buffer, "Wyn Language Reference") != NULL);
    
    fclose(file);
    remove(test_file);
    
    wyn_language_reference_free(reference);
    printf("✓ File generation test passed\n");
}

void test_full_documentation_pipeline() {
    printf("Testing full documentation pipeline...\n");
    
    WynDocGenerator* generator = wyn_doc_generator_new();
    assert(generator != NULL);
    
    // Set output directory
    assert(wyn_doc_generator_set_output_dir(generator, "/tmp") == true);
    assert(wyn_doc_generator_set_format(generator, WYN_FORMAT_MARKDOWN) == true);
    
    // Create language reference
    generator->language_ref = wyn_language_reference_new("1.0.0");
    wyn_generate_syntax_reference(generator->language_ref);
    
    // Generate tutorial
    wyn_generate_getting_started_tutorial(generator);
    
    // Create best practices
    generator->best_practices = wyn_best_practices_new();
    wyn_best_practices_add_practice(generator->best_practices, "Safety First", "Always prioritize memory safety");
    
    // Generate all documentation
    assert(wyn_doc_generator_generate_all(generator) == true);
    
    // Check that files were created
    FILE* ref_file = fopen("/tmp/language_reference.md", "r");
    assert(ref_file != NULL);
    fclose(ref_file);
    remove("/tmp/language_reference.md");
    
    FILE* tutorial_file = fopen("/tmp/tutorial_0.md", "r");
    assert(tutorial_file != NULL);
    fclose(tutorial_file);
    remove("/tmp/tutorial_0.md");
    
    FILE* practices_file = fopen("/tmp/best_practices.md", "r");
    assert(practices_file != NULL);
    fclose(practices_file);
    remove("/tmp/best_practices.md");
    
    wyn_doc_generator_free(generator);
    printf("✓ Full documentation pipeline test passed\n");
}

int main() {
    printf("Running Documentation and Language Reference Tests\n");
    printf("=================================================\n");
    
    test_doc_generator_creation();
    test_language_reference();
    test_tutorial_system();
    test_best_practices();
    test_doc_section_utilities();
    test_syntax_reference_generation();
    test_tutorial_generation();
    test_changelog_system();
    test_documentation_quality();
    test_file_generation();
    test_full_documentation_pipeline();
    
    printf("\n✓ All documentation and language reference tests passed!\n");
    return 0;
}
