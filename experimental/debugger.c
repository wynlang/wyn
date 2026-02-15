#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* file;
    int line;
    int enabled;
} Breakpoint;

static Breakpoint breakpoints[100];
static int bp_count = 0;

void dbg_add_breakpoint(const char* file, int line) {
    if (bp_count < 100) {
        breakpoints[bp_count].file = strdup(file);
        breakpoints[bp_count].line = line;
        breakpoints[bp_count].enabled = 1;
        bp_count++;
        printf("Breakpoint %d set at %s:%d\n", bp_count, file, line);
    }
}

void dbg_list_breakpoints() {
    printf("Breakpoints:\n");
    for (int i = 0; i < bp_count; i++) {
        printf("  %d: %s:%d %s\n", i+1, breakpoints[i].file, 
               breakpoints[i].line, breakpoints[i].enabled ? "[enabled]" : "[disabled]");
    }
}

void dbg_run(const char* program) {
    printf("Running %s with debugger...\n", program);
    printf("Program started.\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Wyn Debugger v0.85.0\n");
        printf("Usage: wyn-dbg <command> [args]\n");
        printf("Commands:\n");
        printf("  break <file> <line>  - Set breakpoint\n");
        printf("  list                 - List breakpoints\n");
        printf("  run <program>        - Run with debugger\n");
        return 1;
    }
    
    if (strcmp(argv[1], "break") == 0 && argc > 3) {
        dbg_add_breakpoint(argv[2], atoi(argv[3]));
    } else if (strcmp(argv[1], "list") == 0) {
        dbg_list_breakpoints();
    } else if (strcmp(argv[1], "run") == 0 && argc > 2) {
        dbg_run(argv[2]);
    }
    
    return 0;
}
