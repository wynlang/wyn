// Simple TOML parser for wyn.toml
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "toml.h"

static char* trim(char* str) {
    while (isspace(*str)) str++;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    *(end + 1) = '\0';
    return str;
}

static char* parse_value(char* value) {
    value = trim(value);
    if (*value == '"') {
        value++;
        char* end = strchr(value, '"');
        if (end) *end = '\0';
    }
    return strdup(value);
}

WynConfig* wyn_config_parse(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;

    WynConfig* config = calloc(1, sizeof(WynConfig));
    char line[1024];
    char current_section[64] = "";
    
    // Allocate initial dependency array
    config->dependencies = malloc(sizeof(WynDependency) * 16);
    config->dependency_count = 0;
    int dep_capacity = 16;

    int lineno = 0;
    int warned = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char* trimmed = trim(line);

        // Skip empty lines and comments
        if (*trimmed == '\0' || *trimmed == '#') continue;

        // Section header
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
            } else if (!warned) {
                fprintf(stderr, "\033[33mWarning:\033[0m wyn.toml line %d: unterminated section header (missing ']')\n", lineno);
                warned = 1;
            }
            continue;
        }

        // Key = value. Anything else is malformed — warn ONCE instead of
        // silently ignoring it (a typo'd manifest line was never surfaced).
        char* eq = strchr(trimmed, '=');
        if (!eq) {
            if (!warned) {
                fprintf(stderr, "\033[33mWarning:\033[0m wyn.toml line %d: ignoring malformed line (expected `key = value` or `[section]`): %.60s\n", lineno, trimmed);
                warned = 1;
            }
            continue;
        }
        
        *eq = '\0';
        char* key = trim(trimmed);
        char* value = parse_value(eq + 1);
        
        if (strcmp(current_section, "project") == 0) {
            if (strcmp(key, "name") == 0) config->project.name = value;
            else if (strcmp(key, "version") == 0) config->project.version = value;
            else if (strcmp(key, "entry") == 0) config->project.entry = value;
            else if (strcmp(key, "author") == 0) config->project.author = value;
            else if (strcmp(key, "description") == 0) config->project.description = value;
            else free(value);
        } else if (strcmp(current_section, "dependencies") == 0) {
            // Add dependency
            if (config->dependency_count >= dep_capacity) {
                dep_capacity *= 2;
                config->dependencies = realloc(config->dependencies, 
                                              sizeof(WynDependency) * dep_capacity);
            }
            config->dependencies[config->dependency_count].name = strdup(key);
            config->dependencies[config->dependency_count].version = value;
            config->dependency_count++;
        } else if (strcmp(current_section, "ffi") == 0) {
            // ACCUMULATE across repeated [ffi] sections — `wyn add` appends one
            // block per C package, so replacing meant only the LAST package's
            // libs/dirs survived and every earlier package failed to link.
            char** slot = NULL;
            if      (strcmp(key, "libs") == 0)         slot = &config->ffi.libs;
            else if (strcmp(key, "lib_dirs") == 0)     slot = &config->ffi.lib_dirs;
            else if (strcmp(key, "include_dirs") == 0) slot = &config->ffi.include_dirs;
            if (slot) {
                if (*slot && **slot) {
                    size_t need = strlen(*slot) + strlen(value) + 3;
                    char* merged = malloc(need);
                    snprintf(merged, need, "%s, %s", *slot, value);
                    free(*slot); free(value);
                    *slot = merged;
                } else {
                    free(*slot);
                    *slot = value;
                }
            } else free(value);
        } else {
            free(value);
        }
    }

    fclose(f);
    return config;
}

// Append `-<flag><token>` for each comma/space-separated token in `list` to buf.
static void append_flag_list(char* buf, int buf_size, const char* flag, const char* list) {
    if (!list || !*list) return;
    char tmp[4096];   // accumulated multi-package lists can be long
    strncpy(tmp, list, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    // Reject shell metacharacters — an FFI list should be bare lib/dir names,
    // never a command. This keeps a malicious wyn.toml from injecting shell.
    for (char* c = tmp; *c; c++) {
        if (*c == ';' || *c == '&' || *c == '|' || *c == '`' || *c == '$' ||
            *c == '(' || *c == ')' || *c == '\n' || *c == '<' || *c == '>') return;
    }
    char* tok = strtok(tmp, ", \t");
    while (tok) {
        int len = strlen(buf);
        snprintf(buf + len, buf_size - len, " -%s%s", flag, tok);
        tok = strtok(NULL, ", \t");
    }
}

char* wyn_config_ffi_flags(WynConfig* config, char* out, int out_size) {
    if (out_size > 0) out[0] = '\0';
    if (!config) return out;
    // Include dirs first (compile), then lib dirs and libs (link).
    append_flag_list(out, out_size, "I", config->ffi.include_dirs);
    append_flag_list(out, out_size, "L", config->ffi.lib_dirs);
    append_flag_list(out, out_size, "l", config->ffi.libs);
    return out;
}

void wyn_config_free(WynConfig* config) {
    if (!config) return;
    free(config->project.name);
    free(config->project.version);
    free(config->project.entry);
    free(config->project.author);
    free(config->project.description);
    for (int i = 0; i < config->dependency_count; i++) {
        free(config->dependencies[i].name);
        free(config->dependencies[i].version);
    }
    free(config->dependencies);
    free(config->ffi.libs);
    free(config->ffi.lib_dirs);
    free(config->ffi.include_dirs);
    free(config);
}

const char* wyn_config_get_name(WynConfig* config) {
    return config ? config->project.name : NULL;
}

const char* wyn_config_get_version(WynConfig* config) {
    return config ? config->project.version : NULL;
}
