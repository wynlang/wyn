// wyn_regex_escapes.h - the ONE place Wyn expands regex shorthand classes.
//
// WHY THIS FILE EXISTS
// Wyn has two regex engines: <regex.h> (POSIX ERE) everywhere but Windows, and
// the bundled NFA in wyn_regex.h on Windows. The NFA implemented \d/\w/\s; POSIX
// ERE has no such escape and regcomp() does NOT reject an unknown one - it
// quietly drops the backslash and matches the LETTER. So
//
//     Regex.replace("2026-04-23 ERROR: disk full", "\\d+", "N")
//
// returned "2026-04-23 ERROR: Nisk full": it rewrote the "d" of "disk" and left
// every digit alone, at exit 0, while the same program on Windows was correct.
//
// The fix is not "teach the POSIX path about \d". It is to expand the shorthands
// ONCE, upstream of both engines, at the single point where a pattern is
// compiled. Two engines that each carry their own copy of "what \d means" is how
// they came to disagree; one expansion pass is why they cannot disagree again.
//
// Expansion targets plain POSIX ERE (explicit byte sets, never named classes),
// so the output is accepted unchanged by both engines.

#ifndef WYN_REGEX_ESCAPES_H
#define WYN_REGEX_ESCAPES_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The shorthand byte sets, written so they can be pasted straight between the
// brackets of a bracket expression.
//
// WYN_RX_SET_S deliberately holds REAL control bytes rather than the two-char
// sequences "\\t", "\\n": inside a POSIX bracket expression a backslash is an
// ordinary member, so "[ \\t]" is the set {space, backslash, t} - the very class
// of mistake this file removes. C string escapes here produce the actual bytes.
#define WYN_RX_SET_D "0-9"
#define WYN_RX_SET_W "0-9A-Za-z_"
#define WYN_RX_SET_S " \t\n\r\f\v"

// Longest expansion is \W -> "[^0-9A-Za-z_]" : 13 bytes out for 2 bytes in.
#define WYN_RX_MAX_GROWTH 8

static inline const char* wyn_rx_shorthand_set(char c) {
    switch (c) {
        case 'd': case 'D': return WYN_RX_SET_D;
        case 'w': case 'W': return WYN_RX_SET_W;
        case 's': case 'S': return WYN_RX_SET_S;
        default:            return NULL;
    }
}

// A shorthand is negated iff it is spelled with a capital: \D \W \S.
static inline bool wyn_rx_shorthand_negated(char c) { return c >= 'A' && c <= 'Z'; }

// A printable spelling of the equivalent negated class, for the diagnostic.
static inline const char* wyn_rx_negated_hint(char c) {
    switch (c) {
        case 'D': return "[^0-9]";
        case 'W': return "[^0-9A-Za-z_]";
        case 'S': return "[^[:space:]]";
        default:  return "a negated bracket expression";
    }
}

// Report an unrepresentable pattern and stop.
//
// This does NOT honour lenient mode, unlike the data-error panics in
// wyn_runtime.h. Lenient mode exists to continue past a bad *value* with a
// defensible default; a pattern that cannot be translated has none - carrying on
// means answering "no match", which is precisely the silent wrong answer this
// file was written to remove.
static inline void wyn_rx_reject(const char* pattern, char bad) {
    fprintf(stderr,
            "panic: regex: \\%c is not supported inside a character class "
            "(pattern \"%s\")\n"
            "  A POSIX character class has no complement operator, so there is "
            "no translation that is not a guess.\n"
            "  Write the negated class directly instead: %s\n",
            bad, pattern, wyn_rx_negated_hint(bad));
    exit(1);
}

// Expand \d \D \w \W \s \S into bracket expressions POSIX ERE can represent.
//
// Returns a newly malloc()'d pattern the caller must free(), or NULL when the
// pattern is not representable - a NEGATED shorthand inside a bracket
// expression, e.g. "[a\D]", which is {a} union complement({0-9}) and needs a set
// complement ERE does not have. On NULL, *bad_escape receives that shorthand
// letter so the caller can name the class to use instead.
//
// Everything else is copied through byte for byte. In particular:
//   - an escaped backslash "\\" is consumed as a pair, so "\\d" stays a literal
//     backslash followed by a literal d and is never read as a shorthand;
//   - POSIX [:class:] / [.coll.] / [=equiv=] items are copied to their closer;
//   - a non-shorthand escape ("\." , "\^" ...) keeps both of its bytes, so ERE
//     quoting behaves exactly as it did before this pass existed.
static inline char* wyn_regex_expand_escapes(const char* pattern, char* bad_escape) {
    if (bad_escape) *bad_escape = 0;
    if (!pattern) return NULL;

    size_t n = strlen(pattern);
    char* out = (char*)malloc(n * WYN_RX_MAX_GROWTH + 16);
    if (!out) return NULL;

    size_t i = 0, o = 0;
    bool in_class = false;   // inside a [ ... ] bracket expression

    while (i < n) {
        char c = pattern[i];

        if (!in_class) {
            if (c == '\\' && i + 1 < n) {
                const char* set = wyn_rx_shorthand_set(pattern[i + 1]);
                if (set) {
                    out[o++] = '[';
                    if (wyn_rx_shorthand_negated(pattern[i + 1])) out[o++] = '^';
                    size_t sl = strlen(set);
                    memcpy(out + o, set, sl); o += sl;
                    out[o++] = ']';
                } else {
                    out[o++] = c;
                    out[o++] = pattern[i + 1];
                }
                i += 2;
                continue;
            }
            if (c == '[') {
                in_class = true;
                out[o++] = pattern[i++];
                // A '^' here negates; a ']' immediately after either is a
                // literal member, not the closer. Both must be copied before
                // the member loop, or the class is mis-terminated.
                if (i < n && pattern[i] == '^') out[o++] = pattern[i++];
                if (i < n && pattern[i] == ']') out[o++] = pattern[i++];
                continue;
            }
            out[o++] = pattern[i++];
            continue;
        }

        // --- inside a bracket expression ---
        if (c == '[' && i + 1 < n &&
            (pattern[i + 1] == ':' || pattern[i + 1] == '.' || pattern[i + 1] == '=')) {
            char kind = pattern[i + 1];
            out[o++] = pattern[i++];
            out[o++] = pattern[i++];
            while (i < n && !(pattern[i] == kind && i + 1 < n && pattern[i + 1] == ']'))
                out[o++] = pattern[i++];
            if (i < n) out[o++] = pattern[i++];   // the kind byte
            if (i < n) out[o++] = pattern[i++];   // the ']'
            continue;
        }
        if (c == '\\' && i + 1 < n) {
            const char* set = wyn_rx_shorthand_set(pattern[i + 1]);
            if (set) {
                if (wyn_rx_shorthand_negated(pattern[i + 1])) {
                    if (bad_escape) *bad_escape = pattern[i + 1];
                    free(out);
                    return NULL;
                }
                size_t sl = strlen(set);
                memcpy(out + o, set, sl); o += sl;
            } else {
                // POSIX brackets treat a backslash as an ordinary member. Keep
                // both bytes so patterns that relied on that keep working.
                out[o++] = c;
                out[o++] = pattern[i + 1];
            }
            i += 2;
            continue;
        }
        if (c == ']') in_class = false;
        out[o++] = pattern[i++];
    }

    out[o] = '\0';
    return out;
}

#endif // WYN_REGEX_ESCAPES_H
