#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#ifdef _WIN32

#include <ctype.h>

// --- File path helpers ---

static inline char* _wyn_basename(const char* path) {
    const char* base = strrchr(path, '\\');
    if (!base) base = strrchr(path, '/');
    return base ? (char*)(base + 1) : (char*)path;
}

static inline char* _dirname(const char* path) {
    static char dir[_MAX_PATH];
    strcpy(dir, path);
    char* last = strrchr(dir, '\\');
    if (!last) last = strrchr(dir, '/');
    if (last) *last = '\0';
    return dir;
}

// --- POSIX access() via Win32 API (no _access dependency) ---

#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 0  // Windows has no execute bit; maps to existence check
#endif

// Uses GetFileAttributesA (always available via windows.h).
// Avoids _access which doesn't exist on all MinGW distributions.
static inline int access(const char* path, int mode) {
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return -1;  // doesn't exist
    if ((mode & W_OK) && (attrs & FILE_ATTRIBUTE_READONLY)) return -1;
    return 0;
}

// --- POSIX getpid() ---

#define getpid() ((int)GetCurrentProcessId())

// --- strcasestr (not in any Windows CRT) ---

static inline char* _wyn_strcasestr(const char* haystack, const char* needle) {
    if (!needle[0]) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n))) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}
#define strcasestr _wyn_strcasestr

// --- Process/signal stubs (no fork/pipe/waitpid on Windows) ---

#define fork() (-1)
#define pipe(fds) (-1)
#define dup2(oldfd, newfd) (-1)
#define waitpid(pid, status, options) (-1)
#define kill(pid, sig) (-1)
#define WIFEXITED(status) (0)
#define WEXITSTATUS(status) (0)
#define WIFSIGNALED(status) (0)
#define WTERMSIG(status) (0)
#define SIGTERM 15
#define SIGKILL 9
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// --- Signal handling stubs ---

struct sigaction { void* sa_handler; void* sa_mask; int sa_flags; };
#define sigemptyset(set) (0)
#define sigaction(sig, act, oldact) (-1)
#define SIG_IGN ((void*)1)
#define SIG_DFL ((void*)0)

// --- System info stubs ---

struct utsname { char sysname[65]; char nodename[65]; char release[65]; char version[65]; char machine[65]; };
#define uname(buf) (-1)
#define sysconf(name) (-1)
#define _SC_NPROCESSORS_ONLN 84

// --- Directory stubs ---

typedef struct { int dummy; } DIR;
#define opendir(path) NULL
#define closedir(dir) (-1)
#define readdir(dir) NULL

#endif // _WIN32

#endif // WINDOWS_COMPAT_H
