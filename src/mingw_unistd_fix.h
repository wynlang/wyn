#ifndef WYN_MINGW_UNISTD_FIX_H
#define WYN_MINGW_UNISTD_FIX_H

// Force-included into EVERY translation unit on Windows via
// `PLATFORM_CFLAGS := ... -include src/mingw_unistd_fix.h` in the Makefile. Do not
// #include it by hand - the point of -include is that it lands before any header
// can pull in <unistd.h>, so no per-file include ORDER can get it wrong and a newly
// added .c file cannot miss it.
//
// THE BUG: mingw's <unistd.h> defines ftruncate as a __CRT_INLINE body that calls
// _chsize:
//
//     __CRT_INLINE int ftruncate(int __fd, off32_t __length)
//     { return _chsize (__fd, __length); }
//
// _chsize is an underscore-prefixed CRT extension declared in <io.h>. Under
// -std=c11 (strict ANSI) mingw hides those extensions, exactly as this codebase
// already documents for _setmode/_fileno in src/lsp.c - so at the point that
// inline body is compiled, _chsize has no declaration in scope. gcc 14+ treats an
// implicit declaration as a hard error rather than a warning, and the runner ships
// mingw 16.1.0:
//
//     unistd.h:67:10: error: implicit declaration of function '_chsize';
//                     did you mean '_msize'? [-Wimplicit-function-declaration]
//
// WHY ONLY THE RELEASE BUILD SAW IT - it is PREPROCESSOR exclusion, not codegen:
//   1. gcc defines __NO_INLINE__ at -O0/-g but NOT at -O1/-O2/-Og.
//   2. _mingw.h: `#ifdef __NO_INLINE__` -> `#define __CRT__NO_INLINE 1`.
//   3. unistd.h: `#ifndef __CRT__NO_INLINE` wraps the ftruncate body.
// So at -g the body is #ifdef'd OUT of the translation unit entirely and _chsize is
// never referenced; at -O2 it is present. Measured on the preprocessed output:
// `-g` yields 2 _chsize hits (both declarations from io.h), `-O2` yields 3, the
// extra being `return _chsize (__fd, __length);`.
//
// Note the corollary: when the body IS present gcc diagnoses the implicit _chsize at
// EVERY optimization level. Optimization level is irrelevant to diagnosing the body -
// the only thing protecting the -g CI build is __NO_INLINE__. That is why four
// platforms and every CI run were green while the optimized Windows build failed.
//
// Including <io.h> first is not a reliable fix on its own: these files pull
// <unistd.h> in ahead of <io.h> through their own headers, and reordering across
// headers is fragile. Declaring the symbol is order-independent.
//
// WHY -include RATHER THAN PER-FILE: 21 of the .c files in CORE_SRCS include
// <unistd.h>. The failing release log named only three, because make stops at the
// first failures - a fourth (src/cpkg.c) was found only by enumerating CORE_SRCS
// against the Makefile. Patching them one at a time is whack-a-mole with a
// ~9-minute CI round-trip per miss.
//
// Harmless everywhere else: the whole thing is behind _WIN32, and the declaration
// matches the CRT's real signature, so it either agrees with <io.h> or supplies
// what <io.h> was hiding.

#ifdef _WIN32
// Matches the CRT: int _chsize(int fd, long size).
extern int _chsize(int fd, long size);
#endif

#endif // WYN_MINGW_UNISTD_FIX_H
