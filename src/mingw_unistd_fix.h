#ifndef WYN_MINGW_UNISTD_FIX_H
#define WYN_MINGW_UNISTD_FIX_H

// Include this BEFORE <unistd.h> in any translation unit that includes it.
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
// WHY ONLY THE RELEASE BUILD SAW IT: -O2 is what causes __CRT_INLINE bodies to be
// emitted at all. The CI build uses -g, never emits ftruncate's body, and so never
// references _chsize - which is why four platforms plus every CI run were green
// while the optimized Windows build failed on src/package.c, src/lsp.c and
// src/bindgen.c. Including <io.h> first is not a reliable fix on its own: these
// files pull <unistd.h> in ahead of <io.h> through their own headers, and reordering
// across headers is fragile. Declaring the symbol is order-independent.
//
// Harmless everywhere else: the whole thing is behind _WIN32, and the declaration
// matches the CRT's real signature, so it either agrees with <io.h> or supplies
// what <io.h> was hiding.

#ifdef _WIN32
// Matches the CRT: int _chsize(int fd, long size).
extern int _chsize(int fd, long size);
#endif

#endif // WYN_MINGW_UNISTD_FIX_H
