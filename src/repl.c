#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wyn_repl() {
    char line[1024];
    int line_num = 1;
    
    printf("Wyn REPL v0.77.0\n");
    printf("Type 'exit' to quit\n\n");
    
    while (1) {
        printf("wyn[%d]> ", line_num);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        if (strlen(line) == 0) {
            continue;
        }
        
        // Create temp file with code
        FILE* f = fopen("/tmp/repl_temp.wyn", "w");
        if (f) {
            fprintf(f, "fn main() -> int {\n");
            fprintf(f, "    %s\n", line);
            fprintf(f, "}\n");
            fclose(f);
            
            // Compile and run
            system("./wyn /tmp/repl_temp.wyn 2>&1 > /dev/null && /tmp/repl_temp.wyn.out");
        }
        
        line_num++;
    }
    
    printf("\nGoodbye!\n");
}

int main() {
    wyn_repl();
    return 0;
}
