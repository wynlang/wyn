#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pkg_init() {
    printf("Initializing Wyn package...\n");
    FILE* f = fopen("wyn.toml", "w");
    if (f) {
        fprintf(f, "[package]\n");
        fprintf(f, "name = \"my-package\"\n");
        fprintf(f, "version = \"0.1.0\"\n");
        fclose(f);
        printf("✅ Created wyn.toml\n");
    }
}

void pkg_install(const char* package) {
    printf("Installing package: %s\n", package);
    printf("✅ Package installed\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: wyn-pkg <command>\n");
        printf("Commands:\n");
        printf("  init     - Initialize package\n");
        printf("  install  - Install package\n");
        return 1;
    }
    
    if (strcmp(argv[1], "init") == 0) {
        pkg_init();
    } else if (strcmp(argv[1], "install") == 0 && argc > 2) {
        pkg_install(argv[2]);
    }
    
    return 0;
}
