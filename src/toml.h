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

// [gpu] section - transparent GPU dispatch for builtin array methods.
// `enabled` is a global kill-switch: it defaults to TRUE (also when the
// section or the whole wyn.toml is absent); `enabled = false` turns every
// GPU path off. Exact-result int dispatch is automatic under the switch.
// Float dispatch (float32 on Metal, a real precision contract) additionally
// needs an explicit opt-in - see main.c's wyn_gpu_flags_from_toml.
typedef struct {
    int enabled;         // kill-switch; parser defaults this to 1
    int enabled_set;     // 1 when the `enabled` key appeared in the file
    int float_enabled;   // [gpu] float = true - opt in to f32 float dispatch
} WynGpu;

typedef struct {
    WynProject project;
    WynDependency* dependencies;
    int dependency_count;
    WynFfi ffi;
    WynGpu gpu;
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
