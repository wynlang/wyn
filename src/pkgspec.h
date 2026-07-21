// Package-spec parsing for the git-URL dependency model.
//
// A dependency is identified by a git URL - there is no central registry. A user
// types one of:
//   args                      → github.com/wynlang/args        (bare = "official")
//   github.com/bob/cool-lib   → that repo                       (third-party)
//   github.com/bob/cool@v1.2  → that repo pinned to ref v1.2
//   https://gitlab.com/x/y    → any host, any scheme
//   name = "github.com/…@ref" → the value stored in wyn.toml [dependencies]
//
// This module turns any of those into a normalized PkgSpec and computes the
// global on-disk cache location for it.
#ifndef PKGSPEC_H
#define PKGSPEC_H

#include <stddef.h>

typedef struct {
    char name[128];   // import name / cache subdir (repo tail, sans .git)
    char url[512];    // clone URL, https:// prepended if scheme was omitted
    char ref[128];    // git ref (tag/branch/sha); "" means default branch
    char host[128];   // e.g. github.com  (for the cache path)
    char owner[128];  // e.g. wynlang     (for the cache path)
    char repo[128];   // e.g. args        (for the cache path)
} PkgSpec;

// Default owner a bare name expands under: `args` → github.com/wynlang/args.
#define WYN_OFFICIAL_HOST  "github.com"
#define WYN_OFFICIAL_OWNER "wynlang"

// Parse `input` (any of the forms above) into `spec`. `override_name`, if
// non-NULL/non-empty, sets spec->name instead of deriving it from the repo tail
// (used by `wyn add <url> --as <name>`). Returns 0 on success, nonzero if the
// input can't be understood as a package spec.
int pkgspec_parse(const char* input, const char* override_name, PkgSpec* spec);

// Root of the global package cache: $WYN_PKG_CACHE, else $HOME/.wyn/pkg.
// Writes into `buf`; returns `buf`.
char* pkgspec_cache_root(char* buf, size_t size);

// Absolute cache directory for a spec:
//   <cache-root>/<host>/<owner>/<repo>@<ref>   (ref "" → "@HEAD" bucket)
// Writes into `buf`; returns `buf`.
char* pkgspec_cache_dir(const PkgSpec* spec, char* buf, size_t size);

#endif // PKGSPEC_H
