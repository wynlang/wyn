// Fuzz harness for Wyn lexer and parser
// Build: clang -g -fsanitize=fuzzer,address -I src -o fuzz_parser fuzz/fuzz_parser.c src/*.c vendor/tcc/lib/libtcc.a -lpthread -lm -DWYN_PLATFORM_MACOS
// Run:   ./fuzz_parser fuzz/corpus/ -max_len=4096 -timeout=5

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Wyn compiler API
extern void init_lexer(const char* source);
extern void init_parser(void);

typedef struct Token { int type; const char* start; int length; int line; } Token;
extern Token next_token(void);

typedef struct { void** stmts; int count; } Program;
extern Program* parse_program(void);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192) return 0;  // Skip very large inputs

    char* source = malloc(size + 1);
    if (!source) return 0;
    memcpy(source, data, size);
    source[size] = '\0';

    // Suppress stderr
    FILE* saved = stderr;
    stderr = fopen("/dev/null", "w");

    // Fuzz lexer
    init_lexer(source);
    for (int i = 0; i < 50000; i++) {
        Token tok = next_token();
        if (tok.type == 0) break;  // EOF
    }

    // Fuzz parser
    init_lexer(source);
    init_parser();
    parse_program();

    fclose(stderr);
    stderr = saved;
    free(source);
    return 0;
}
