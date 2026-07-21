// Package-spec parsing - see pkgspec.h for the contract.
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pkgspec.h"

// Strip a trailing ".git" from `s` in place.
static void strip_dot_git(char* s) {
    size_t n = strlen(s);
    if (n >= 4 && strcmp(s + n - 4, ".git") == 0) s[n - 4] = '\0';
}

// Copy `src` into `dst[cap]`, NUL-terminating.
static void copy_field(char* dst, size_t cap, const char* src) {
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

int pkgspec_parse(const char* input, const char* override_name, PkgSpec* spec) {
    if (!input || !*input || !spec) return 1;
    // A dependency is a git repo, not a filesystem path - reject local-path forms
    // so they fail cleanly instead of being mangled into a bogus host.
    if (input[0] == '.' || input[0] == '/' || input[0] == '~') return 1;
    memset(spec, 0, sizeof(*spec));

    // Work on a mutable copy so we can split off the @ref.
    char work[640];
    copy_field(work, sizeof(work), input);

    // Split off an "@ref" suffix - but not the "@" in a scp-style git@host URL.
    // Only an '@' after the last '/' is unambiguously a ref.
    char* last_slash = strrchr(work, '/');
    char* at = strrchr(work, '@');
    if (at && (!last_slash || at > last_slash)) {
        *at = '\0';
        copy_field(spec->ref, sizeof(spec->ref), at + 1);
    }

    // Determine scheme / bare-name.
    int has_scheme  = (strstr(work, "://") != NULL);
    int is_scp      = (!has_scheme && strncmp(work, "git@", 4) == 0);   // git@host:owner/repo
    int has_slash   = (strchr(work, '/') != NULL);
    int looks_hosted = has_scheme || is_scp ||
                       // host/owner/repo style: a dot before the first slash
                       (has_slash && strchr(work, '.') && strchr(work, '.') < strchr(work, '/'));

    char skeleton[640];   // host[/owner]/repo  (no scheme, no user@)
    if (is_scp) {
        // git@github.com:owner/repo → github.com/owner/repo for the skeleton;
        // clone URL stays in scp form (with the @ref already stripped from work).
        const char* after = work + 4;            // skip "git@"
        char tmp[640]; copy_field(tmp, sizeof(tmp), after);
        char* colon = strchr(tmp, ':');
        if (colon) *colon = '/';
        copy_field(skeleton, sizeof(skeleton), tmp);
        snprintf(spec->url, sizeof(spec->url), "git@%s", after);
    } else if (has_scheme) {
        const char* p = strstr(work, "://") + 3;
        // Drop an optional "user@" authority prefix.
        const char* authat = strchr(p, '@');
        const char* slashp = strchr(p, '/');
        if (authat && (!slashp || authat < slashp)) p = authat + 1;
        copy_field(skeleton, sizeof(skeleton), p);
        copy_field(spec->url, sizeof(spec->url), work);  // already has scheme, ref stripped
    } else if (looks_hosted) {
        copy_field(skeleton, sizeof(skeleton), work);
        snprintf(spec->url, sizeof(spec->url), "https://%s", work);
    } else {
        // Bare name → official org. Reject anything with a slash or empty.
        if (has_slash || !*work) return 1;
        snprintf(skeleton, sizeof(skeleton), "%s/%s/%s",
                 WYN_OFFICIAL_HOST, WYN_OFFICIAL_OWNER, work);
        snprintf(spec->url, sizeof(spec->url), "https://%s/%s/%s",
                 WYN_OFFICIAL_HOST, WYN_OFFICIAL_OWNER, work);
    }

    // Split skeleton into host / owner… / repo.
    char sk[640]; copy_field(sk, sizeof(sk), skeleton);
    char* first = strchr(sk, '/');
    if (!first) return 1;               // need at least host/repo
    *first = '\0';
    copy_field(spec->host, sizeof(spec->host), sk);
    char* rest = first + 1;
    char* lastsl = strrchr(rest, '/');
    if (lastsl) {
        *lastsl = '\0';
        copy_field(spec->owner, sizeof(spec->owner), rest);
        copy_field(spec->repo, sizeof(spec->repo), lastsl + 1);
    } else {
        spec->owner[0] = '\0';
        copy_field(spec->repo, sizeof(spec->repo), rest);
    }
    strip_dot_git(spec->repo);
    strip_dot_git(spec->url);
    if (!*spec->repo) return 1;

    // Import name: override, else the repo tail.
    if (override_name && *override_name)
        copy_field(spec->name, sizeof(spec->name), override_name);
    else
        copy_field(spec->name, sizeof(spec->name), spec->repo);

    return 0;
}

char* pkgspec_cache_root(char* buf, size_t size) {
    const char* env = getenv("WYN_PKG_CACHE");
    if (env && *env) { snprintf(buf, size, "%s", env); return buf; }
    const char* home = getenv("HOME");
    if (!home || !*home) home = ".";
    snprintf(buf, size, "%s/.wyn/pkg", home);
    return buf;
}

char* pkgspec_cache_dir(const PkgSpec* spec, char* buf, size_t size) {
    char root[512]; pkgspec_cache_root(root, sizeof(root));
    const char* ref = (spec->ref[0]) ? spec->ref : "HEAD";
    if (spec->owner[0])
        snprintf(buf, size, "%s/%s/%s/%s@%s", root, spec->host, spec->owner, spec->repo, ref);
    else
        snprintf(buf, size, "%s/%s/%s@%s", root, spec->host, spec->repo, ref);
    return buf;
}
