#ifndef WYN_BANNER_H
#define WYN_BANNER_H

// Clean text identity — no ASCII art. The visual identity (the wyvern-W
// emblem) lives on wynlang.com; the CLI prints one informative line.
// (The old dragon/cat drawings are gone by owner decision, 2026-07-21.)
static void print_banner_to(FILE* out, const char* version) {
    fprintf(out, "\033[1mWyn\033[0m v%s \033[2m— compiled language: Python feel, C speed, one binary\033[0m\n\n", version);
}

// No-args usage banner: stderr (stdout stays clean for piping).
static void print_banner(const char* version) { print_banner_to(stderr, version); }

#endif
