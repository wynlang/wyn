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
// GPU dispatch is OFF by default: it turns on only when BOTH keys below are
// present and true. Absent section / absent wyn.toml => off. `enabled = false`
// is the kill-switch (turns everything off regardless of `float32`).
//
// The ONLY GPU path implemented today is a float32 `[float].map`, which is
// LOSSY relative to Wyn's float64 CPU semantics (Metal has no double). Because
// it changes results, it is not covered by `enabled` alone: it demands the
// separate, explicit `float32 = true` opt-in. See main.c's
// wyn_gpu_flag_from_toml and docs/GPU_DESIGN.md for the precision contract.
typedef struct {
    int enabled;         // [gpu] enabled = true - master switch (default off)
    int float_enabled;   // [gpu] float32 = true - opt in to lossy f32 [float].map
} WynGpu;

// [app] section - metadata for a NATIVE, double-clickable application
// (`wyn build --app`): a macOS .app bundle, a Windows GUI-subsystem .exe, or a
// Linux binary + .desktop entry.
//
// This section only ever supplies METADATA; it does not by itself change what
// `wyn build` produces. The one exception is `bundle = true`, which is an
// explicit in-manifest opt-in equivalent to passing --app - see cmd_compile.c's
// wyn_app_begin for why the trigger is opt-in and never inferred.
//
// An identifier and an icon have nowhere sensible to live on a command line,
// which is the whole reason this section exists alongside the flag.
typedef struct {
    char* name;        // user-visible name ("My Great App"); may contain spaces
    char* identifier;  // CFBundleIdentifier, e.g. com.example.myapp
    char* version;     // CFBundleShortVersionString; falls back to [project] version
    char* icon;        // path to .icns (macOS) / .png (Linux), relative to cwd
    char* category;    // LSApplicationCategoryType / .desktop Categories
    char* min_system;  // LSMinimumSystemVersion (macOS only)
    char* resources;   // directory copied into Contents/Resources (macOS)
    char* cwd;         // "resources"|"bundle": chdir launcher for a .app (macOS)
    int bundle;        // `bundle = true` => `wyn build` packages without --app
} WynApp;

typedef struct {
    WynProject project;
    WynDependency* dependencies;
    int dependency_count;
    WynFfi ffi;
    WynGpu gpu;
    WynApp app;
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
