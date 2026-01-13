#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

typedef struct {
    char filename[256];
    int total_lines;
    int covered_lines;
    double coverage_percent;
    int* line_coverage; // Array of coverage counts per line
} FileCoverage;

typedef struct {
    FileCoverage* files;
    int file_count;
    int capacity;
    int total_lines;
    int total_covered;
    double overall_coverage;
} CoverageReport;

CoverageReport* create_coverage_report() {
    CoverageReport* report = malloc(sizeof(CoverageReport));
    report->capacity = 50;
    report->files = malloc(sizeof(FileCoverage) * report->capacity);
    report->file_count = 0;
    report->total_lines = 0;
    report->total_covered = 0;
    report->overall_coverage = 0.0;
    return report;
}

int count_lines_in_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    
    int lines = 0;
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') lines++;
    }
    
    fclose(file);
    return lines;
}

void parse_gcov_file(const char* gcov_file, FileCoverage* coverage) {
    FILE* file = fopen(gcov_file, "r");
    if (!file) return;
    
    char line[1024];
    int line_num = 0;
    coverage->covered_lines = 0;
    coverage->total_lines = 0;
    
    // Count total lines first
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, ":")) {
            coverage->total_lines++;
        }
    }
    
    // Allocate line coverage array
    coverage->line_coverage = calloc(coverage->total_lines, sizeof(int));
    
    // Reset file pointer and parse coverage
    rewind(file);
    line_num = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, ":")) {
            char* colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                
                // Parse execution count
                char* trimmed = line;
                while (*trimmed == ' ') trimmed++;
                
                if (strcmp(trimmed, "-") == 0) {
                    // Non-executable line
                    coverage->line_coverage[line_num] = -1;
                } else if (strcmp(trimmed, "#####") == 0) {
                    // Uncovered executable line
                    coverage->line_coverage[line_num] = 0;
                } else {
                    // Covered line with execution count
                    int exec_count = atoi(trimmed);
                    coverage->line_coverage[line_num] = exec_count;
                    if (exec_count > 0) {
                        coverage->covered_lines++;
                    }
                }
                line_num++;
            }
        }
    }
    
    coverage->coverage_percent = coverage->total_lines > 0 ? 
        (double)coverage->covered_lines / coverage->total_lines * 100.0 : 0.0;
    
    fclose(file);
}

void add_file_coverage(CoverageReport* report, const char* source_file, const char* gcov_file) {
    if (report->file_count >= report->capacity) {
        report->capacity *= 2;
        report->files = realloc(report->files, sizeof(FileCoverage) * report->capacity);
    }
    
    FileCoverage* coverage = &report->files[report->file_count++];
    strncpy(coverage->filename, source_file, sizeof(coverage->filename) - 1);
    
    if (access(gcov_file, F_OK) == 0) {
        parse_gcov_file(gcov_file, coverage);
    } else {
        // Fallback: basic line counting without coverage data
        coverage->total_lines = count_lines_in_file(source_file);
        coverage->covered_lines = 0;
        coverage->coverage_percent = 0.0;
        coverage->line_coverage = calloc(coverage->total_lines, sizeof(int));
    }
    
    report->total_lines += coverage->total_lines;
    report->total_covered += coverage->covered_lines;
}

void generate_coverage_for_wyn_files(CoverageReport* report) {
    // Compile with coverage flags
    system("../wyn --coverage *.wyn");
    
    // Run tests to generate coverage data
    system("./run_all_tests.sh");
    
    // Process gcov files
    DIR* dir = opendir(".");
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wyn")) {
            char gcov_file[256];
            snprintf(gcov_file, sizeof(gcov_file), "%s.gcov", entry->d_name);
            
            add_file_coverage(report, entry->d_name, gcov_file);
        }
    }
    
    closedir(dir);
}

void calculate_overall_coverage(CoverageReport* report) {
    report->overall_coverage = report->total_lines > 0 ? 
        (double)report->total_covered / report->total_lines * 100.0 : 0.0;
}

void print_coverage_report(CoverageReport* report) {
    printf("\n=== Code Coverage Report ===\n");
    printf("%-40s %10s %10s %10s\n", "File", "Lines", "Covered", "Coverage");
    printf("%-40s %10s %10s %10s\n", "----", "-----", "-------", "--------");
    
    for (int i = 0; i < report->file_count; i++) {
        FileCoverage* f = &report->files[i];
        printf("%-40s %10d %10d %9.1f%%\n",
               f->filename, f->total_lines, f->covered_lines, f->coverage_percent);
    }
    
    printf("%-40s %10s %10s %10s\n", "----", "-----", "-------", "--------");
    printf("%-40s %10d %10d %9.1f%%\n",
           "TOTAL", report->total_lines, report->total_covered, report->overall_coverage);
    
    printf("\n=== Coverage Summary ===\n");
    printf("Overall Coverage: %.1f%%\n", report->overall_coverage);
    
    if (report->overall_coverage >= 90.0) {
        printf("Coverage Status: EXCELLENT\n");
    } else if (report->overall_coverage >= 80.0) {
        printf("Coverage Status: GOOD\n");
    } else if (report->overall_coverage >= 70.0) {
        printf("Coverage Status: FAIR\n");
    } else {
        printf("Coverage Status: POOR\n");
    }
}

void generate_html_report(CoverageReport* report, const char* output_dir) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_dir);
    system(cmd);
    
    char index_file[256];
    snprintf(index_file, sizeof(index_file), "%s/index.html", output_dir);
    
    FILE* html = fopen(index_file, "w");
    if (!html) return;
    
    fprintf(html, "<!DOCTYPE html>\n<html>\n<head>\n");
    fprintf(html, "<title>Wyn Code Coverage Report</title>\n");
    fprintf(html, "<style>\n");
    fprintf(html, "body { font-family: Arial, sans-serif; margin: 20px; }\n");
    fprintf(html, "table { border-collapse: collapse; width: 100%%; }\n");
    fprintf(html, "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n");
    fprintf(html, "th { background-color: #f2f2f2; }\n");
    fprintf(html, ".high-coverage { background-color: #d4edda; }\n");
    fprintf(html, ".medium-coverage { background-color: #fff3cd; }\n");
    fprintf(html, ".low-coverage { background-color: #f8d7da; }\n");
    fprintf(html, "</style>\n</head>\n<body>\n");
    
    fprintf(html, "<h1>Wyn Code Coverage Report</h1>\n");
    time_t now = time(NULL);
    fprintf(html, "<p>Generated: %s</p>\n", ctime(&now));
    
    fprintf(html, "<h2>Summary</h2>\n");
    fprintf(html, "<p>Overall Coverage: <strong>%.1f%%</strong></p>\n", report->overall_coverage);
    fprintf(html, "<p>Total Lines: %d</p>\n", report->total_lines);
    fprintf(html, "<p>Covered Lines: %d</p>\n", report->total_covered);
    
    fprintf(html, "<h2>File Coverage</h2>\n");
    fprintf(html, "<table>\n");
    fprintf(html, "<tr><th>File</th><th>Lines</th><th>Covered</th><th>Coverage</th></tr>\n");
    
    for (int i = 0; i < report->file_count; i++) {
        FileCoverage* f = &report->files[i];
        const char* css_class = f->coverage_percent >= 80.0 ? "high-coverage" :
                               f->coverage_percent >= 60.0 ? "medium-coverage" : "low-coverage";
        
        fprintf(html, "<tr class=\"%s\">", css_class);
        fprintf(html, "<td><a href=\"%s.html\">%s</a></td>", f->filename, f->filename);
        fprintf(html, "<td>%d</td><td>%d</td><td>%.1f%%</td>", 
                f->total_lines, f->covered_lines, f->coverage_percent);
        fprintf(html, "</tr>\n");
        
        // Generate individual file report
        char file_report[256];
        snprintf(file_report, sizeof(file_report), "%s/%s.html", output_dir, f->filename);
        
        FILE* file_html = fopen(file_report, "w");
        if (file_html) {
            fprintf(file_html, "<!DOCTYPE html>\n<html>\n<head>\n");
            fprintf(file_html, "<title>Coverage: %s</title>\n", f->filename);
            fprintf(file_html, "<style>\n");
            fprintf(file_html, "body { font-family: monospace; margin: 20px; }\n");
            fprintf(file_html, ".covered { background-color: #d4edda; }\n");
            fprintf(file_html, ".uncovered { background-color: #f8d7da; }\n");
            fprintf(file_html, ".non-executable { background-color: #f8f9fa; }\n");
            fprintf(file_html, "</style>\n</head>\n<body>\n");
            
            fprintf(file_html, "<h1>Coverage: %s</h1>\n", f->filename);
            fprintf(file_html, "<p><a href=\"index.html\">&larr; Back to Summary</a></p>\n");
            fprintf(file_html, "<p>Coverage: %.1f%% (%d/%d lines)</p>\n", 
                    f->coverage_percent, f->covered_lines, f->total_lines);
            
            // Show source code with coverage highlighting
            FILE* source = fopen(f->filename, "r");
            if (source) {
                fprintf(file_html, "<pre>\n");
                char line[1024];
                int line_num = 0;
                
                while (fgets(line, sizeof(line), source) && line_num < f->total_lines) {
                    const char* css_class = "non-executable";
                    if (f->line_coverage[line_num] > 0) {
                        css_class = "covered";
                    } else if (f->line_coverage[line_num] == 0) {
                        css_class = "uncovered";
                    }
                    
                    fprintf(file_html, "<span class=\"%s\">%4d: %s</span>", 
                            css_class, line_num + 1, line);
                    line_num++;
                }
                
                fprintf(file_html, "</pre>\n");
                fclose(source);
            }
            
            fprintf(file_html, "</body>\n</html>\n");
            fclose(file_html);
        }
    }
    
    fprintf(html, "</table>\n");
    fprintf(html, "</body>\n</html>\n");
    fclose(html);
}

int main(int argc, char* argv[]) {
    const char* output_dir = "coverage_report";
    int generate_html = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--html") == 0) {
            generate_html = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        }
    }
    
    CoverageReport* report = create_coverage_report();
    
    printf("Generating code coverage report...\n");
    
    generate_coverage_for_wyn_files(report);
    calculate_overall_coverage(report);
    
    if (report->file_count == 0) {
        printf("No source files found for coverage analysis.\n");
        return 1;
    }
    
    print_coverage_report(report);
    
    if (generate_html) {
        generate_html_report(report, output_dir);
        printf("\nHTML coverage report generated in: %s/\n", output_dir);
        printf("Open %s/index.html in your browser to view the report.\n", output_dir);
    }
    
    // Clean up
    for (int i = 0; i < report->file_count; i++) {
        free(report->files[i].line_coverage);
    }
    free(report->files);
    free(report);
    
    return report->overall_coverage >= 80.0 ? 0 : 1;
}