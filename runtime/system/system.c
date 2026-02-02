// System Module Implementation
#include "../wyn_runtime.h"
#include <stdlib.h>
#include <string.h>

// Global argc/argv storage (set by main wrapper)
static int g_argc = 0;
static char** g_argv = NULL;

// Initialize argc/argv (called by wyn_wrapper.c)
void wyn_system_init(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

int wyn_system_argc(void) {
    return g_argc;
}

const char* wyn_system_argv(int index) {
    if (index < 0 || index >= g_argc) {
        return NULL;
    }
    return g_argv[index];
}

const char* wyn_system_env(const char* name) {
    return getenv(name);
}

int wyn_system_set_env(const char* name, const char* value) {
    return setenv(name, value, 1);
}

int wyn_system_exec(const char* command) {
    return system(command);
}
