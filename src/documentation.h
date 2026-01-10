#ifndef WYN_DOCUMENTATION_H
#define WYN_DOCUMENTATION_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct WynDocGenerator WynDocGenerator;
typedef struct WynLanguageReference WynLanguageReference;
typedef struct WynTutorial WynTutorial;
typedef struct WynBestPractices WynBestPractices;

// Documentation types
typedef enum {
    WYN_DOC_LANGUAGE_REFERENCE,
    WYN_DOC_API_REFERENCE,
    WYN_DOC_TUTORIAL,
    WYN_DOC_BEST_PRACTICES,
    WYN_DOC_MIGRATION_GUIDE,
    WYN_DOC_EXAMPLES,
    WYN_DOC_CHANGELOG
} WynDocumentationType;

// Output formats
typedef enum {
    WYN_FORMAT_MARKDOWN,
    WYN_FORMAT_HTML,
    WYN_FORMAT_PDF,
    WYN_FORMAT_JSON,
    WYN_FORMAT_PLAIN_TEXT
} WynDocumentationFormat;

// Documentation section
typedef struct {
    char* title;
    char* content;
    char** code_examples;
    size_t example_count;
    char** cross_references;
    size_t reference_count;
} WynDocSection;

// Language reference structure
typedef struct WynLanguageReference {
    char* version;
    WynDocSection* sections;
    size_t section_count;
    char* introduction;
    char* quick_start;
    char* syntax_overview;
} WynLanguageReference;

// Tutorial structure
typedef struct WynTutorial {
    char* title;
    char* description;
    char* difficulty_level; // "beginner", "intermediate", "advanced"
    WynDocSection* lessons;
    size_t lesson_count;
    char** prerequisites;
    size_t prerequisite_count;
} WynTutorial;

// Best practices guide
typedef struct WynBestPractices {
    WynDocSection* practices;
    size_t practice_count;
    char** anti_patterns;
    size_t anti_pattern_count;
    char* style_guide;
} WynBestPractices;

// Documentation generator
typedef struct WynDocGenerator {
    WynLanguageReference* language_ref;
    WynTutorial* tutorials;
    size_t tutorial_count;
    WynBestPractices* best_practices;
    char* output_directory;
    WynDocumentationFormat format;
} WynDocGenerator;

// Documentation generator functions
WynDocGenerator* wyn_doc_generator_new(void);
void wyn_doc_generator_free(WynDocGenerator* generator);
bool wyn_doc_generator_set_output_dir(WynDocGenerator* generator, const char* directory);
bool wyn_doc_generator_set_format(WynDocGenerator* generator, WynDocumentationFormat format);
bool wyn_doc_generator_generate_all(WynDocGenerator* generator);

// Language reference functions
WynLanguageReference* wyn_language_reference_new(const char* version);
void wyn_language_reference_free(WynLanguageReference* reference);
bool wyn_language_reference_add_section(WynLanguageReference* reference, const char* title, const char* content);
bool wyn_language_reference_add_example(WynLanguageReference* reference, const char* section_title, const char* code);
bool wyn_language_reference_generate(WynLanguageReference* reference, const char* output_file, WynDocumentationFormat format);

// Tutorial functions
WynTutorial* wyn_tutorial_new(const char* title, const char* description, const char* difficulty);
void wyn_tutorial_free(WynTutorial* tutorial);
bool wyn_tutorial_add_lesson(WynTutorial* tutorial, const char* title, const char* content);
bool wyn_tutorial_add_prerequisite(WynTutorial* tutorial, const char* prerequisite);
bool wyn_tutorial_generate(WynTutorial* tutorial, const char* output_file, WynDocumentationFormat format);

// Best practices functions
WynBestPractices* wyn_best_practices_new(void);
void wyn_best_practices_free(WynBestPractices* practices);
bool wyn_best_practices_add_practice(WynBestPractices* practices, const char* title, const char* description);
bool wyn_best_practices_add_anti_pattern(WynBestPractices* practices, const char* anti_pattern);
bool wyn_best_practices_set_style_guide(WynBestPractices* practices, const char* style_guide);
bool wyn_best_practices_generate(WynBestPractices* practices, const char* output_file, WynDocumentationFormat format);

// Documentation section utilities
WynDocSection* wyn_doc_section_new(const char* title, const char* content);
void wyn_doc_section_free(WynDocSection* section);
bool wyn_doc_section_add_example(WynDocSection* section, const char* code);
bool wyn_doc_section_add_reference(WynDocSection* section, const char* reference);

// Built-in documentation content generators
bool wyn_generate_syntax_reference(WynLanguageReference* reference);
bool wyn_generate_type_system_docs(WynLanguageReference* reference);
bool wyn_generate_standard_library_docs(WynLanguageReference* reference);
bool wyn_generate_memory_management_docs(WynLanguageReference* reference);
bool wyn_generate_concurrency_docs(WynLanguageReference* reference);

// Tutorial generators
bool wyn_generate_getting_started_tutorial(WynDocGenerator* generator);
bool wyn_generate_basic_syntax_tutorial(WynDocGenerator* generator);
bool wyn_generate_memory_safety_tutorial(WynDocGenerator* generator);
bool wyn_generate_advanced_features_tutorial(WynDocGenerator* generator);
bool wyn_generate_performance_tutorial(WynDocGenerator* generator);

// Migration guide generators
bool wyn_generate_c_migration_guide(WynDocGenerator* generator);
bool wyn_generate_rust_migration_guide(WynDocGenerator* generator);
bool wyn_generate_go_migration_guide(WynDocGenerator* generator);
bool wyn_generate_cpp_migration_guide(WynDocGenerator* generator);

// API documentation extraction
typedef struct {
    char* function_name;
    char* signature;
    char* description;
    char** parameters;
    size_t parameter_count;
    char* return_description;
    char** examples;
    size_t example_count;
} WynAPIFunction;

typedef struct {
    WynAPIFunction* functions;
    size_t function_count;
    char* module_name;
    char* description;
} WynAPIModule;

WynAPIModule* wyn_extract_api_documentation(const char* source_file);
bool wyn_generate_api_reference(WynAPIModule* module, const char* output_file, WynDocumentationFormat format);
void wyn_api_module_free(WynAPIModule* module);

// Documentation validation
typedef struct {
    bool has_description;
    bool has_examples;
    bool has_cross_references;
    size_t word_count;
    double readability_score;
} WynDocQuality;

WynDocQuality wyn_validate_documentation_quality(const WynDocSection* section);
bool wyn_check_documentation_completeness(const WynLanguageReference* reference);
bool wyn_validate_code_examples(const WynDocSection* section);

// Interactive documentation
typedef struct {
    char* title;
    char* code;
    char* expected_output;
    bool is_runnable;
} WynInteractiveExample;

WynInteractiveExample* wyn_create_interactive_example(const char* title, const char* code, const char* expected_output);
bool wyn_generate_interactive_docs(WynDocGenerator* generator, const char* output_file);
void wyn_interactive_example_free(WynInteractiveExample* example);

// Documentation search and indexing
typedef struct {
    char** keywords;
    size_t keyword_count;
    char* section_title;
    char* content_preview;
} WynDocSearchResult;

typedef struct {
    WynDocSearchResult* results;
    size_t result_count;
    char* query;
} WynDocSearchIndex;

WynDocSearchIndex* wyn_create_documentation_index(const WynLanguageReference* reference);
WynDocSearchResult* wyn_search_documentation(const WynDocSearchIndex* index, const char* query, size_t* result_count);
void wyn_doc_search_index_free(WynDocSearchIndex* index);

// Documentation templates
const char* wyn_get_markdown_template(WynDocumentationType type);
const char* wyn_get_html_template(WynDocumentationType type);
bool wyn_apply_documentation_template(const char* content, const char* template, char** output);

// Changelog generation
typedef struct {
    char* version;
    char* date;
    char** added_features;
    size_t added_count;
    char** changed_features;
    size_t changed_count;
    char** fixed_bugs;
    size_t fixed_count;
    char** breaking_changes;
    size_t breaking_count;
} WynChangelogEntry;

typedef struct {
    WynChangelogEntry* entries;
    size_t entry_count;
    char* project_name;
} WynChangelog;

WynChangelog* wyn_changelog_new(const char* project_name);
void wyn_changelog_free(WynChangelog* changelog);
bool wyn_changelog_add_entry(WynChangelog* changelog, const char* version, const char* date);
bool wyn_changelog_add_feature(WynChangelog* changelog, const char* version, const char* feature);
bool wyn_changelog_add_change(WynChangelog* changelog, const char* version, const char* change);
bool wyn_changelog_add_fix(WynChangelog* changelog, const char* version, const char* fix);
bool wyn_changelog_add_breaking_change(WynChangelog* changelog, const char* version, const char* change);
bool wyn_changelog_generate(WynChangelog* changelog, const char* output_file, WynDocumentationFormat format);

#endif // WYN_DOCUMENTATION_H
