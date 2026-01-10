#include "../src/documentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_basic_functionality() {
    printf("Testing basic documentation functionality...\n");
    
    // Test doc generator
    WynDocGenerator* generator = wyn_doc_generator_new();
    assert(generator != NULL);
    
    // Test language reference
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    assert(reference != NULL);
    assert(strcmp(reference->version, "1.0.0") == 0);
    
    wyn_language_reference_add_section(reference, "Variables", "Test content");
    assert(reference->section_count == 1);
    
    // Test tutorial
    WynTutorial* tutorial = wyn_tutorial_new("Test Tutorial", "Test description", "beginner");
    assert(tutorial != NULL);
    
    wyn_tutorial_add_lesson(tutorial, "Lesson 1", "Test lesson content");
    assert(tutorial->lesson_count == 1);
    
    // Test best practices
    WynBestPractices* practices = wyn_best_practices_new();
    assert(practices != NULL);
    
    wyn_best_practices_add_practice(practices, "Practice 1", "Test practice");
    assert(practices->practice_count == 1);
    
    // Test changelog
    WynChangelog* changelog = wyn_changelog_new("Test Project");
    assert(changelog != NULL);
    
    wyn_changelog_add_entry(changelog, "1.0.0", "2026-01-09");
    assert(changelog->entry_count == 1);
    
    // Cleanup
    wyn_changelog_free(changelog);
    wyn_best_practices_free(practices);
    wyn_tutorial_free(tutorial);
    wyn_language_reference_free(reference);
    wyn_doc_generator_free(generator);
    
    printf("✓ Basic documentation functionality test passed\n");
}

void test_file_generation() {
    printf("Testing file generation...\n");
    
    WynLanguageReference* reference = wyn_language_reference_new("1.0.0");
    wyn_language_reference_add_section(reference, "Test", "Test content");
    
    const char* test_file = "/tmp/test_doc.md";
    bool result = wyn_language_reference_generate(reference, test_file, WYN_FORMAT_MARKDOWN);
    assert(result == true);
    
    // Check file exists
    FILE* file = fopen(test_file, "r");
    assert(file != NULL);
    fclose(file);
    remove(test_file);
    
    wyn_language_reference_free(reference);
    printf("✓ File generation test passed\n");
}

int main() {
    printf("Running Simple Documentation Tests\n");
    printf("==================================\n");
    
    test_basic_functionality();
    test_file_generation();
    
    printf("\n✓ All simple documentation tests passed!\n");
    return 0;
}
