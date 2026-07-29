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
// _chsize is declared in mingw's <io.h>, which mingw's own unistd.h includes at its
// line 10. THE REAL CAUSE IS INCLUDE-PATH SHADOWING, not -std=c11: we compile with
// `-I src` and we ship our own `src/io.h`. gcc searches -I directories before system
// directories even for angle-bracket includes, so mingw's `#include <io.h>` resolves
// to OUR src/io.h (guard WYN_IO_H), mingw's real io.h is never read, and _chsize is
// never declared. Verified with mingw gcc 14:
//
//     -std=c11  -O2  (no -I src)  -> 0 errors   <- so c11 is NOT the trigger
//     -std=gnu11 -O2 -I src       -> 2 errors   <- so gnu11 does not save you
//     gcc -H shows: .. src/io.h                 <- our header wins
//
// (An earlier version of this comment blamed -std=c11 hiding underscore CRT
// extensions, citing the _setmode/_fileno precedent in src/lsp.c. That was wrong -
// mingw's io.h declares _chsize under `#ifndef _IO_H_` and nothing else - and the
// lsp.c precedent is very likely the same misdiagnosis. Kept here because the wrong
// story predicts the wrong fix: it makes per-file include ORDER look like the
// problem, when in fact mingw's <io.h> is unreachable from anywhere while -I src
// shadows it.)
//
// gcc 14+ treats the resulting implicit declaration as a hard error rather than a
// warning, and the runner ships mingw 16.1.0:
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
// THE DURABLE FIX WE HAVE NOT DONE: rename src/io.h to src/wyn_io.h. That unshadows
// mingw's real <io.h> for every translation unit at once and stops the next file
// that includes <unistd.h> from re-breaking the Windows release build. This -include
// shim only patches the one symbol we tripped over; the shadowing is still there and
// can bite again with a different symbol (_setmode, _fileno, _mkdir, ...). Tracked in
// internal-docs/PLAN_v1.21.md §8. Only 4 files include our "io.h", so the rename is
// small - it was deliberately not done during the release to keep the diff minimal.
//
// Harmless everywhere else: the whole thing is behind _WIN32, and the declaration
// matches the CRT's real signature, so it either agrees with <io.h> or supplies
// what <io.h> was hiding.

#ifdef _WIN32
// Matches the CRT: int _chsize(int fd, long size).
extern int _chsize(int fd, long size);
#endif

#endif // WYN_MINGW_UNISTD_FIX_H
