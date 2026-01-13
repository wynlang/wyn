#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Basic LSP server for Wyn
void lsp_initialize() {
    printf("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{}}}\n");
}

void lsp_hover(const char* file, int line, int col) {
    printf("{\"jsonrpc\":\"2.0\",\"result\":{\"contents\":\"Wyn symbol\"}}\n");
}

void lsp_completion(const char* file, int line, int col) {
    printf("{\"jsonrpc\":\"2.0\",\"result\":[");
    printf("{\"label\":\"fn\",\"kind\":3},");
    printf("{\"label\":\"let\",\"kind\":3},");
    printf("{\"label\":\"struct\",\"kind\":3}");
    printf("]}\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Wyn LSP Server v0.85.0\n");
        printf("Usage: wyn-lsp <command>\n");
        return 1;
    }
    
    if (strcmp(argv[1], "init") == 0) {
        lsp_initialize();
    } else if (strcmp(argv[1], "hover") == 0) {
        lsp_hover("", 0, 0);
    } else if (strcmp(argv[1], "complete") == 0) {
        lsp_completion("", 0, 0);
    }
    
    return 0;
}
