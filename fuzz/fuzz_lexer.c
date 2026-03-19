// Fuzz harness for Wyn lexer
// Build: clang -g -fsanitize=fuzzer,address -I src -o fuzz/fuzz_lexer fuzz/fuzz_lexer.c src/lexer.c -DWYN_PLATFORM_MACOS -w
// Run:   ./fuzz/fuzz_lexer fuzz/corpus/ -max_len=4096 -timeout=5

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern void init_lexer(const char* source);
typedef struct { int type; const char* start; int length; int line; } Token;
extern Token next_token(void);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192) return 0;

    char* source = (char*)malloc(size + 1);
    if (!source) return 0;
    memcpy(source, data, size);
    source[size] = '\0';

    init_lexer(source);
    for (int i = 0; i < 100000; i++) {
        Token tok = next_token();
        if (tok.type == 0) break;
    }

    free(source);
    return 0;
}
