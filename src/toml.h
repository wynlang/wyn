// Simple TOML parser for wyn.toml
// Supports basic key=value and [sections]

#ifndef TOML_H
#define TOML_H

typedef struct {
    char* name;
    char* version;
    char* entry;
    char* author;
    char* description;
} WynProject;

typedef struct {
    char* name;
    char* version;
} WynDependency;

// [ffi] section - how to link C libraries an `extern fn` calls into.
// Each is a comma/space-separated list, e.g. libs = "curl, z".
typedef struct {
    char* libs;          // -l<name> for each
    char* lib_dirs;      // -L<dir> for each
    char* include_dirs;  // -I<dir> for each
} WynFfi;

typedef struct {
    WynProject project;
    WynDependency* dependencies;
    int dependency_count;
    WynFfi ffi;
} WynConfig;

// Parse wyn.toml file
WynConfig* wyn_config_parse(const char* filename);

// Free config
void wyn_config_free(WynConfig* config);

// Get project name
const char* wyn_config_get_name(WynConfig* config);

// Get project version
const char* wyn_config_get_version(WynConfig* config);

// Build a C-compiler flag string from the [ffi] section (`-l`, `-L`, `-I`),
// leading with a space, into `out`. Empty string if no [ffi] fields. Safe to
// call with config == NULL. Returns `out`.
char* wyn_config_ffi_flags(WynConfig* config, char* out, int out_size);

#endif // TOML_H
