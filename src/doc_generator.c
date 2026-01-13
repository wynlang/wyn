#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void generate_docs_for_file(const char* filename) {
    printf("# Documentation for %s\n\n", filename);
    printf("## Functions\n\n");
    printf("## Structs\n\n");
    printf("## Enums\n\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: wyn-doc <file.wyn>\n");
        return 1;
    }
    
    generate_docs_for_file(argv[1]);
    return 0;
}
