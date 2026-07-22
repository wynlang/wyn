// wyn ui - interactive command browser (full-screen terminal UI).
//
// Design notes:
//   * The command table below is DATA and the single source of truth for the
//     TUI. When a new CLI command is added to main.c's dispatch, add one row
//     here - tests/errors/run_ui_coverage_test.sh diffs main.c's
//     strcmp(command, "...") dispatch against `wyn ui --list-commands` and
//     fails the build if they drift.
//   * Zero dependencies: raw ANSI escape codes + termios. No ncurses.
//   * Alternate screen buffer (1049h/1049l) so the user's scrollback is
//     untouched; cursor hidden while navigating; termios restored on every
//     exit path (normal quit, SIGINT/SIGTERM/SIGHUP, atexit).
//   * Frames are composed into one buffer and flushed with a single write()
//     to avoid flicker.
//   * Windows: the command compiles (and --list-commands works) but the
//     interactive UI prints a friendly message - raw Win32 console support
//     is not wired yet.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "cmd_ui.h"

// ─────────────────────────── command table ───────────────────────────────

enum { ARG_NONE = 0, ARG_FILE, ARG_PATH, ARG_TEXT };

typedef struct {
    const char* name;   // "--release"; NULL terminates the list
    const char* value;  // NULL = boolean toggle; else placeholder ("N", "name")
    const char* desc;
} UiFlag;

typedef struct {
    const char* name;        // dispatch name; may contain a space ("pkg add")
    const char* alias;       // secondary dispatch name or NULL
    const char* group;       // Develop | Build | Packages | Tools
    const char* desc;        // one-line description (list + detail)
    int arg;                 // ARG_* for the positional argument
    const char* arg_label;   // placeholder shown in the form ("file.wyn")
    const char* arg_ext;     // picker extension filter (".wyn", ".h") or NULL
    int arg_required;        // 1 = Run refuses until the argument is set
    const char* choices;     // "a|b|c" radio list or NULL
    const char* choice_flag; // "--template"; NULL = positional BEFORE the arg
    int takeover;            // long-running: exec() and never return to the TUI
    int hidden;              // in --list-commands but not in the browser
    UiFlag flags[7];
} UiCmd;

#define NOFLAGS {{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}

static const UiCmd CMDS[] = {
    // ── Develop ──
    {"run", NULL, "Develop", "Compile and run", ARG_FILE, "file.wyn", ".wyn", 1,
     NULL, NULL, 0, 0,
     {{"--release", NULL, "Full optimizations (-O3)"},
      {"--fast", NULL, "Skip optimizations (fastest compile)"},
      {"--debug", NULL, "Keep .c and .out artifacts"},
      {"--mem-stats", NULL, "Print memory statistics"}, {0,0,0},{0,0,0},{0,0,0}}},
    {"check", NULL, "Develop", "Type-check without compiling", ARG_FILE, "file.wyn", ".wyn", 1,
     NULL, NULL, 0, 0, NOFLAGS},
    {"fmt", NULL, "Develop", "Format source file or directory", ARG_PATH, "file.wyn or dir", ".wyn", 1,
     NULL, NULL, 0, 0,
     {{"--check", NULL, "Report differences, do not rewrite"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"fix", NULL, "Develop", "Migrate removed syntax", ARG_PATH, "file.wyn or dir", ".wyn", 1,
     NULL, NULL, 0, 0,
     {{"--check", NULL, "Report changes, do not rewrite"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"test", NULL, "Develop", "Run project tests", ARG_TEXT, "filter or dir (optional)", NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"watch", NULL, "Develop", "Watch and auto-rebuild", ARG_FILE, "file.wyn", ".wyn", 1,
     NULL, NULL, 1, 0, NOFLAGS},
    {"repl", NULL, "Develop", "Interactive REPL", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 1, 0, NOFLAGS},
    {"bench", NULL, "Develop", "Benchmark with timing", ARG_FILE, "file.wyn", ".wyn", 1,
     NULL, NULL, 0, 0,
     {{"--iterations", "N", "Number of runs (default 10)"},
      {"--compare", NULL, "Show previous results only"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"doc", NULL, "Develop", "Generate documentation", ARG_FILE, "file.wyn (optional)", ".wyn", 0,
     NULL, NULL, 0, 0,
     {{"--html", NULL, "Emit HTML instead of markdown"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"debug", NULL, "Develop", "Debug a program", ARG_FILE, "file.wyn", ".wyn", 1,
     NULL, NULL, 0, 0, NOFLAGS},

    // ── Build ──
    {"build", NULL, "Build", "Build a binary or library", ARG_PATH, "file or dir (optional)", ".wyn", 0,
     NULL, NULL, 0, 0,
     {{"--release", NULL, "Full optimizations (-O3), stripped"},
      {"--shared", NULL, "Build a C shared library"},
      {"--python", NULL, "Build a Python extension module"},
      {"--pgo", NULL, "Profile-guided optimization (with --release)"},
      {"-o", "name", "Output binary name"}, {0,0,0},{0,0,0}}},
    {"cross", NULL, "Build", "Cross-compile for another platform", ARG_FILE, "file.wyn", ".wyn", 1,
     "linux|linux-arm64|macos|macos-x64|macos-arm64|windows|ios|android", NULL, 0, 0, NOFLAGS},
    {"build-runtime", NULL, "Build", "Precompile runtime for fast builds", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"clean", NULL, "Build", "Remove build artifacts", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},

    // ── Packages ──
    {"init", "new", "Packages", "Create a new project", ARG_TEXT, "project name", NULL, 1,
     "default|cli|api|web|lib", "--template", 0, 0, NOFLAGS},
    {"add", NULL, "Packages", "Add a dependency (C lib or git repo)", ARG_TEXT, "name or git url", NULL, 1,
     NULL, NULL, 0, 0, NOFLAGS},
    {"remove", "rm", "Packages", "Remove a dependency", ARG_TEXT, "name", NULL, 1,
     NULL, NULL, 0, 0, NOFLAGS},
    {"list", NULL, "Packages", "List declared dependencies", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"restore", NULL, "Packages", "Reinstall deps from lock file", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"pkg add", NULL, "Packages", "pkg: add a git dependency", ARG_TEXT, "name or url[@ref]", NULL, 1,
     NULL, NULL, 0, 0, NOFLAGS},
    {"pkg install", NULL, "Packages", "pkg: install all declared deps", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"pkg remove", NULL, "Packages", "pkg: remove a dependency", ARG_TEXT, "name", NULL, 1,
     NULL, NULL, 0, 0, NOFLAGS},
    {"pkg list", NULL, "Packages", "pkg: list declared dependencies", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"pkg audit", NULL, "Packages", "pkg: verify lock vs remotes", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0,
     {{"--offline", NULL, "Skip network checks"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"pkg search", NULL, "Packages", "pkg: discover packages on GitHub", ARG_TEXT, "query (optional)", NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},

    // ── Tools ──
    {"lsp", NULL, "Tools", "Start language server (for editors)", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 1, 0, NOFLAGS},
    {"doctor", NULL, "Tools", "Check your setup", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"explain", NULL, "Tools", "Explain an error code", ARG_TEXT, "error code (E001..E005)", NULL, 1,
     NULL, NULL, 0, 0, NOFLAGS},
    {"bind", NULL, "Tools", "Generate FFI bindings from a C header", ARG_FILE, "header.h", ".h", 1,
     NULL, NULL, 0, 0,
     {{"-o", "out.wyn", "Write bindings to a file"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"deploy", NULL, "Tools", "Deploy to a server via SSH", ARG_TEXT, "target (from wyn.toml)", NULL, 1,
     NULL, NULL, 0, 0,
     {{"--dry-run", NULL, "Show the plan, change nothing"}, {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
    {"logs", NULL, "Tools", "Tail remote service logs", ARG_TEXT, "target (from wyn.toml)", NULL, 1,
     NULL, NULL, 1, 0, NOFLAGS},
    {"ssh", NULL, "Tools", "Open a shell on a deploy target", ARG_TEXT, "target (from wyn.toml)", NULL, 1,
     NULL, NULL, 1, 0, NOFLAGS},
    {"install", NULL, "Tools", "Install wyn to system PATH", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"uninstall", NULL, "Tools", "Remove wyn from system PATH", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"upgrade", "update", "Tools", "Update wyn to a release", ARG_TEXT, "version (optional)", NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"wisdom", NULL, "Tools", "Flight rules from Wynter", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"version", NULL, "Tools", "Show version", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"help", NULL, "Tools", "Show CLI help", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 0, NOFLAGS},
    {"ui", "tui", "Tools", "This command browser", ARG_NONE, NULL, NULL, 0,
     NULL, NULL, 0, 1, NOFLAGS},
};
static const int NCMD = (int)(sizeof(CMDS) / sizeof(CMDS[0]));
static const char* GROUPS[] = {"Develop", "Build", "Packages", "Tools"};

// ───────────────────── portable part: --list-commands ─────────────────────

static int ui_list_commands(void) {
    for (int i = 0; i < NCMD; i++) {
        if (CMDS[i].desc[0] == 0 && CMDS[i].hidden) continue; // internal filler
        // Print the first token of the name (the dispatch word) + alias.
        char head[64];
        snprintf(head, sizeof(head), "%s", CMDS[i].name);
        char* sp = strchr(head, ' ');
        if (sp) *sp = 0;
        printf("%s\n", head);
        if (CMDS[i].alias) printf("%s\n", CMDS[i].alias);
    }
    return 0;
}

#ifdef _WIN32

int cmd_ui(int argc, char** argv, const char* version) {
    (void)version;
    if (argc > 2 && strcmp(argv[2], "--list-commands") == 0) return ui_list_commands();
    fprintf(stderr, "wyn ui requires a POSIX terminal (Windows support coming).\n");
    fprintf(stderr, "Run 'wyn help' for the command reference.\n");
    return 1;
}

#else  // ───────────────────────── POSIX implementation ────────────────────

#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/stat.h>

// ── terminal state ──
static struct termios g_saved_tio;
static volatile sig_atomic_t g_tui_active = 0;
static volatile sig_atomic_t g_resized = 0;

static void tui_restore(void) {
    if (!g_tui_active) return;
    g_tui_active = 0;
    // rmcup + show cursor + reset attrs. write() is async-signal-safe.
    const char* s = "\033[0m\033[?25h\033[?1049l";
    ssize_t r = write(STDOUT_FILENO, s, strlen(s));
    (void)r;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_tio);
}

static void on_fatal_signal(int sig) {
    tui_restore();
    _exit(128 + sig);
}

static void on_winch(int sig) { (void)sig; g_resized = 1; }

static void tui_enter(void) {
    struct termios raw = g_saved_tio;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag |= OPOST;  // keep \n -> \r\n so child output later is sane
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    const char* s = "\033[?1049h\033[?25l\033[2J\033[H";
    ssize_t r = write(STDOUT_FILENO, s, strlen(s));
    (void)r;
    g_tui_active = 1;
}

// ── frame buffer: compose everything, one write() per frame ──
static char g_fb[1 << 19];
static size_t g_fblen;

static void fbf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_fb + g_fblen, sizeof(g_fb) - g_fblen, fmt, ap);
    va_end(ap);
    if (n > 0) {
        g_fblen += (size_t)n;
        if (g_fblen >= sizeof(g_fb)) g_fblen = sizeof(g_fb) - 1;
    }
}

static void fb_flush(void) {
    ssize_t r = write(STDOUT_FILENO, g_fb, g_fblen);
    (void)r;
    g_fblen = 0;
}

// Append `s` clipped to `width` visible columns, then pad with spaces.
// Skips ANSI escape sequences when counting; UTF-8 continuation bytes are
// zero-width (our box-drawing chars are 3 bytes, 1 column).
static void fb_pad(const char* s, int width) {
    int cols = 0;
    const unsigned char* p = (const unsigned char*)s;
    while (*p && cols < width) {
        if (*p == 0x1b) {  // escape sequence: copy until final letter
            while (*p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) {
                if (g_fblen < sizeof(g_fb) - 1) g_fb[g_fblen++] = (char)*p;
                p++;
            }
            if (*p && g_fblen < sizeof(g_fb) - 1) g_fb[g_fblen++] = (char)*p++;
            continue;
        }
        if ((*p & 0xC0) != 0x80) cols++;           // count only lead bytes
        if (g_fblen < sizeof(g_fb) - 1) g_fb[g_fblen++] = (char)*p;
        p++;
        while ((*p & 0xC0) == 0x80) {              // copy continuation bytes
            if (g_fblen < sizeof(g_fb) - 1) g_fb[g_fblen++] = (char)*p;
            p++;
        }
    }
    fbf("\033[0m");
    for (; cols < width; cols++)
        if (g_fblen < sizeof(g_fb) - 1) g_fb[g_fblen++] = ' ';
    g_fb[g_fblen] = 0;
}

// Horizontal border segment: "─ Title ────" padded to width.
static void fb_hline_titled(const char* title, int width) {
    char buf[256];
    int used = 0;
    if (title && title[0]) {
        snprintf(buf, sizeof(buf), "\033[2m\033[0m\033[1m %s \033[0m", title);
        used = 2 + (int)strlen(title);  // visible cols of " title "
        fbf("─");
        fb_pad(buf, used);
        used += 1;
    }
    for (int i = used; i < width; i++) fbf("─");
}

// ── input ──
enum {
    K_UP = 1001, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESC, K_TAB, K_BACKSPACE,
    K_HOME, K_END, K_RESIZE, K_NONE, K_EOF
};

static int read_byte(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) return c;
    if (n == 0) return -2;  // EOF: terminal went away / stdin closed
    return -1;
}

static int read_key(void) {
    if (g_resized) { g_resized = 0; return K_RESIZE; }
    int c = read_byte();
    if (c == -2) return K_EOF;
    if (c < 0) {
        if (errno == EINTR && g_resized) { g_resized = 0; return K_RESIZE; }
        return K_NONE;
    }
    if (c == '\r' || c == '\n') return K_ENTER;
    if (c == '\t') return K_TAB;
    if (c == 127 || c == 8) return K_BACKSPACE;
    if (c == 3) return 'q';  // ctrl-c quits like q (terminal is restored)
    if (c != 0x1b) return c;
    // ESC: peek for a CSI sequence with a short timeout.
    fd_set fds;
    struct timeval tv = {0, 30000};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return K_ESC;
    int c1 = read_byte();
    if (c1 != '[' && c1 != 'O') return K_ESC;
    int c2 = read_byte();
    switch (c2) {
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        case 'H': return K_HOME;
        case 'F': return K_END;
        default:
            // swallow the rest of unknown sequences (e.g. "3~")
            while (c2 >= '0' && c2 <= '9') c2 = read_byte();
            return K_NONE;
    }
}

// ── app state ──
#define MAXCMD 64
#define MAXFLAG 7
static int st_flag_on[MAXCMD][MAXFLAG];
static char st_flag_val[MAXCMD][MAXFLAG][128];
static char st_arg[MAXCMD][512];
static int st_choice[MAXCMD];

static int g_rows = 24, g_cols = 80;
static int g_focus = 0;        // 0 = list, 1 = detail
static int g_sel = 0;          // selected command (index into CMDS)
static int g_field = 0;        // selected field in the detail form
static int g_scroll = 0;       // list scroll offset (in item rows)
static char g_filter[64] = "";
static int g_filter_mode = 0;
static char g_status[160] = "";
static const char* g_version = "";
static char g_argv0[512] = "wyn";

static void term_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        g_cols = ws.ws_col;
        g_rows = ws.ws_row;
    } else {
        g_cols = 80;
        g_rows = 24;
    }
}

// case-insensitive subsequence match ("fuzzy")
static int fuzzy_match(const char* needle, const char* hay) {
    if (!needle[0]) return 1;
    const char* n = needle;
    for (const char* h = hay; *h && *n; h++) {
        char a = *h, b = *n;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a == b) n++;
    }
    return *n == 0;
}

// visible list items: group headers (-1 - group idx) and command indexes
static int g_items[MAXCMD + 8];
static int g_nitems;

static void build_items(void) {
    g_nitems = 0;
    int filtered = g_filter[0] != 0;
    for (int gi = 0; gi < 4; gi++) {
        int header_at = -1;
        for (int ci = 0; ci < NCMD; ci++) {
            if (CMDS[ci].hidden) continue;
            if (strcmp(CMDS[ci].group, GROUPS[gi]) != 0) continue;
            if (filtered && !fuzzy_match(g_filter, CMDS[ci].name) &&
                !fuzzy_match(g_filter, CMDS[ci].desc)) continue;
            if (header_at < 0 && !filtered) {
                header_at = g_nitems;
                g_items[g_nitems++] = -1 - gi;
            }
            g_items[g_nitems++] = ci;
        }
    }
}

static int item_of_sel(void) {
    for (int i = 0; i < g_nitems; i++)
        if (g_items[i] == g_sel) return i;
    return -1;
}

static void select_first_cmd(void) {
    for (int i = 0; i < g_nitems; i++)
        if (g_items[i] >= 0) { g_sel = g_items[i]; return; }
}

static void move_sel(int dir) {
    int at = item_of_sel();
    if (at < 0) { select_first_cmd(); return; }
    int i = at + dir;
    while (i >= 0 && i < g_nitems && g_items[i] < 0) i += dir;
    if (i >= 0 && i < g_nitems) g_sel = g_items[i];
}

// ── choices helpers ──
static int choice_count(const UiCmd* c) {
    if (!c->choices) return 0;
    int n = 1;
    for (const char* p = c->choices; *p; p++)
        if (*p == '|') n++;
    return n;
}

static void choice_name(const UiCmd* c, int idx, char* out, size_t cap) {
    const char* p = c->choices;
    for (int i = 0; i < idx && p; i++) {
        p = strchr(p, '|');
        if (p) p++;
    }
    if (!p) { out[0] = 0; return; }
    const char* e = strchr(p, '|');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len >= cap) len = cap - 1;
    memcpy(out, p, len);
    out[len] = 0;
}

// ── detail form fields ──
enum { F_ARG, F_CHOICE, F_FLAG, F_RUN };
typedef struct { int kind; int idx; } Field;
static Field g_fields[MAXFLAG + 3];
static int g_nfields;

static void build_fields(void) {
    const UiCmd* c = &CMDS[g_sel];
    g_nfields = 0;
    if (c->arg != ARG_NONE) g_fields[g_nfields++] = (Field){F_ARG, 0};
    if (c->choices) g_fields[g_nfields++] = (Field){F_CHOICE, 0};
    for (int i = 0; i < MAXFLAG && c->flags[i].name; i++)
        g_fields[g_nfields++] = (Field){F_FLAG, i};
    g_fields[g_nfields++] = (Field){F_RUN, 0};
    if (g_field >= g_nfields) g_field = g_nfields - 1;
    if (g_field < 0) g_field = 0;
}

// ── command line assembly ──
static int build_argv(char** out, int cap, char* store, size_t store_cap) {
    const UiCmd* c = &CMDS[g_sel];
    int n = 0;
    size_t sp = 0;
#define PUSH(str) do { \
        size_t _l = strlen(str) + 1; \
        if (n < cap - 1 && sp + _l < store_cap) { \
            memcpy(store + sp, (str), _l); \
            out[n++] = store + sp; \
            sp += _l; \
        } \
    } while (0)
    PUSH(g_argv0);
    // command name tokens ("pkg add" -> "pkg", "add")
    {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s", c->name);
        char* save = NULL;
        for (char* t = strtok_r(tmp, " ", &save); t; t = strtok_r(NULL, " ", &save))
            PUSH(t);
    }
    char ch[64] = "";
    if (c->choices) choice_name(c, st_choice[g_sel], ch, sizeof(ch));
    if (c->choices && !c->choice_flag) PUSH(ch);       // positional (cross <target>)
    if (c->arg != ARG_NONE && st_arg[g_sel][0]) PUSH(st_arg[g_sel]);
    if (c->choices && c->choice_flag && strcmp(ch, "default") != 0) {
        PUSH(c->choice_flag);
        PUSH(ch);
    }
    for (int i = 0; i < MAXFLAG && c->flags[i].name; i++) {
        if (c->flags[i].value) {
            if (st_flag_val[g_sel][i][0]) {
                PUSH(c->flags[i].name);
                PUSH(st_flag_val[g_sel][i]);
            }
        } else if (st_flag_on[g_sel][i]) {
            PUSH(c->flags[i].name);
        }
    }
#undef PUSH
    out[n] = NULL;
    return n;
}

static void build_cmdline(char* out, size_t cap) {
    char* av[24];
    char store[2048];
    int n = build_argv(av, 24, store, sizeof(store));
    size_t p = 0;
    out[0] = 0;
    for (int i = 0; i < n && p < cap; i++) {
        const char* w = (i == 0) ? "wyn" : av[i];
        int wrote = snprintf(out + p, cap - p, "%s%s", i ? " " : "", w);
        if (wrote < 0) break;
        p += (size_t)wrote;
    }
}

// ── header helpers ──
static void git_branch(char* out, size_t cap) {
    out[0] = 0;
    char head_path[600] = ".git/HEAD";
    FILE* f = fopen(".git", "r");  // worktrees: .git is a file "gitdir: <path>"
    if (f) {
        char line[512] = "";
        if (fgets(line, sizeof(line), f) && strncmp(line, "gitdir: ", 8) == 0) {
            line[strcspn(line, "\n")] = 0;
            snprintf(head_path, sizeof(head_path), "%s/HEAD", line + 8);
        }
        fclose(f);
    }
    f = fopen(head_path, "r");
    if (!f) return;
    char line[256] = "";
    if (fgets(line, sizeof(line), f)) {
        char* p = strstr(line, "refs/heads/");
        if (p) {
            p += 11;
            p[strcspn(p, "\n")] = 0;
            snprintf(out, cap, "%s", p);
        }
    }
    fclose(f);
}

static void short_cwd(char* out, size_t cap) {
    char cwd[512] = "";
    if (!getcwd(cwd, sizeof(cwd))) { snprintf(out, cap, "?"); return; }
    const char* home = getenv("HOME");
    if (home && home[0] && strncmp(cwd, home, strlen(home)) == 0)
        snprintf(out, cap, "~%s", cwd + strlen(home));
    else
        snprintf(out, cap, "%s", cwd);
}

// ── file picker overlay ──
static int g_picker = 0;
static char g_picker_dir[512] = ".";
static char g_picker_names[128][160];
static int g_picker_isdir[128];
static int g_picker_n = 0, g_picker_sel = 0, g_picker_scroll = 0;

static void picker_load(const char* ext) {
    g_picker_n = 0;
    g_picker_sel = 0;
    g_picker_scroll = 0;
    snprintf(g_picker_names[g_picker_n], sizeof(g_picker_names[0]), "..");
    g_picker_isdir[g_picker_n++] = 1;
    DIR* d = opendir(g_picker_dir);
    if (!d) return;
    struct dirent* e;
    // two passes: directories first, then matching files - both alphabetical
    for (int pass = 0; pass < 2; pass++) {
        rewinddir(d);
        int start = g_picker_n;
        while ((e = readdir(d)) && g_picker_n < 128) {
            if (e->d_name[0] == '.') continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", g_picker_dir, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;
            int isdir = S_ISDIR(st.st_mode);
            if (pass == 0 && !isdir) continue;
            if (pass == 1) {
                if (isdir) continue;
                if (ext) {
                    const char* dot = strrchr(e->d_name, '.');
                    if (!dot || strcmp(dot, ext) != 0) continue;
                }
            }
            snprintf(g_picker_names[g_picker_n], sizeof(g_picker_names[0]), "%s", e->d_name);
            g_picker_isdir[g_picker_n++] = isdir;
        }
        // insertion sort this pass's slice
        for (int i = start + 1; i < g_picker_n; i++) {
            char tmp[160];
            memcpy(tmp, g_picker_names[i], sizeof(tmp));
            int td = g_picker_isdir[i];
            int j = i - 1;
            while (j >= start && strcmp(g_picker_names[j], tmp) > 0) {
                memcpy(g_picker_names[j + 1], g_picker_names[j], sizeof(tmp));
                g_picker_isdir[j + 1] = g_picker_isdir[j];
                j--;
            }
            memcpy(g_picker_names[j + 1], tmp, sizeof(tmp));
            g_picker_isdir[j + 1] = td;
        }
    }
    closedir(d);
}

static void picker_path(char* out, size_t cap, const char* name) {
    if (strcmp(g_picker_dir, ".") == 0)
        snprintf(out, cap, "%s", name);
    else if (strncmp(g_picker_dir, "./", 2) == 0)
        snprintf(out, cap, "%s/%s", g_picker_dir + 2, name);
    else
        snprintf(out, cap, "%s/%s", g_picker_dir, name);
}

// ── text input (modal, single line, drawn in the status row) ──
static int edit_line(const char* label, char* buf, size_t cap) {
    char work[512];
    snprintf(work, sizeof(work), "%s", buf);
    size_t len = strlen(work);
    for (;;) {
        g_fblen = 0;
        fbf("\033[%d;1H", g_rows);
        char line[640];
        snprintf(line, sizeof(line), "\033[1m %s:\033[0m %s\033[7m \033[0m", label, work);
        fb_pad(line, g_cols);
        fbf("\033[?25l");
        fb_flush();
        int k = read_key();
        if (k == K_ENTER) {
            snprintf(buf, cap, "%s", work);
            return 1;
        }
        if (k == K_ESC || k == K_EOF) return 0;
        if (k == K_RESIZE) { term_size(); return 0; }
        if (k == K_BACKSPACE) {
            if (len > 0) work[--len] = 0;
        } else if (k >= 32 && k < 127 && len < sizeof(work) - 1) {
            work[len++] = (char)k;
            work[len] = 0;
        }
    }
}

// ─────────────────────────────── drawing ──────────────────────────────────

static void draw_header(void) {
    char cwd[256], br[128], line[640];
    short_cwd(cwd, sizeof(cwd));
    git_branch(br, sizeof(br));
    if (br[0])
        snprintf(line, sizeof(line), " wyn v%s   %s   (%s)", g_version, cwd, br);
    else
        snprintf(line, sizeof(line), " wyn v%s   %s", g_version, cwd);
    fbf("\033[1;1H\033[7m");
    fb_pad(line, g_cols);
}

static void draw_status(void) {
    char line[512];
    if (g_filter_mode)
        snprintf(line, sizeof(line), " /%s\xE2\x96\x88   enter accept   esc clear", g_filter);
    else if (g_status[0])
        snprintf(line, sizeof(line), " %s", g_status);
    else if (g_picker)
        snprintf(line, sizeof(line), " j/k move   enter open/select   esc cancel");
    else if (g_focus == 0)
        snprintf(line, sizeof(line), " j/k move   enter select   tab pane   / filter   q quit");
    else
        snprintf(line, sizeof(line), " j/k field   space toggle   enter edit/run   esc back   q quit");
    fbf("\033[%d;1H\033[2m", g_rows);
    fb_pad(line, g_cols);
    g_status[0] = 0;
}

// list pane rows into rows [top..top+h-1], width w, at column col
static void draw_list(int top, int h, int col, int w) {
    int at = item_of_sel();
    if (at < g_scroll) g_scroll = at < 0 ? 0 : at;
    if (at >= g_scroll + h) g_scroll = at - h + 1;
    if (g_scroll < 0) g_scroll = 0;
    for (int r = 0; r < h; r++) {
        int idx = g_scroll + r;
        fbf("\033[%d;%dH", top + r, col);
        char line[320];
        if (idx >= g_nitems) {
            fb_pad("", w);
            continue;
        }
        int it = g_items[idx];
        if (it < 0) {
            snprintf(line, sizeof(line), "\033[1m %s\033[0m", GROUPS[-1 - it]);
            fb_pad(line, w);
        } else {
            const UiCmd* c = &CMDS[it];
            int selected = (it == g_sel);
            if (selected && g_focus == 0)
                snprintf(line, sizeof(line), "\033[7m  %-14s%s\033[0m", c->name, c->takeover ? ">" : " ");
            else if (selected)
                snprintf(line, sizeof(line), "\033[7m\033[2m  %-14s%s\033[0m", c->name, c->takeover ? ">" : " ");
            else
                snprintf(line, sizeof(line), "  \033[32m%-14s\033[0m\033[2m%s\033[0m", c->name, c->takeover ? ">" : " ");
            fb_pad(line, w);
        }
    }
}

static void draw_detail(int top, int h, int col, int w) {
    const UiCmd* c = &CMDS[g_sel];
    build_fields();
    char line[640];
    int r = 0;
#define ROW(...) do { \
        if (r < h) { \
            fbf("\033[%d;%dH", top + r, col); \
            snprintf(line, sizeof(line), __VA_ARGS__); \
            fb_pad(line, w); \
            r++; \
        } \
    } while (0)
    ROW(" \033[1m%s\033[0m%s%s  \033[2m%s\033[0m", c->name,
        c->alias ? "  alias: " : "", c->alias ? c->alias : "", c->desc);
    if (c->takeover)
        ROW(" \033[2mtakes over the terminal; use ctrl-c / exit to leave\033[0m");
    ROW("%s", "");
    int fi = 0;
    for (int f = 0; f < g_nfields; f++) {
        Field* fd = &g_fields[f];
        int hot = (g_focus == 1 && g_field == f);
        const char* cur = hot ? "\033[7m" : "";
        const char* end = hot ? "\033[0m" : "";
        if (fd->kind == F_ARG) {
            ROW(" \033[1mArgument\033[0m");
            const char* v = st_arg[g_sel][0] ? st_arg[g_sel] : "(not set)";
            const char* how = (c->arg == ARG_FILE || c->arg == ARG_PATH)
                                  ? "enter: pick file" : "enter: type value";
            ROW("  %s \033[33m%-24s\033[0m%s %s  \033[2m%s%s\033[0m",
                cur, c->arg_label, end, v, how, c->arg_required ? ", required" : "");
            ROW("%s", "");
        } else if (fd->kind == F_CHOICE) {
            int n = choice_count(c);
            char opts[256] = "";
            size_t p = 0;
            for (int i = 0; i < n; i++) {
                char nm[64];
                choice_name(c, i, nm, sizeof(nm));
                int wrote = snprintf(opts + p, sizeof(opts) - p, "%s(%s) %s",
                                     i ? "  " : "", i == st_choice[g_sel] ? "\xE2\x80\xA2" : " ", nm);
                if (wrote > 0) p += (size_t)wrote;
            }
            ROW(" \033[1m%s\033[0m", c->choice_flag ? "Template" : "Target");
            ROW("  %s %s %s \033[2mh/l or space to change\033[0m", cur, opts, end);
            ROW("%s", "");
        } else if (fd->kind == F_FLAG) {
            if (fi == 0) ROW(" \033[1mOptions\033[0m");
            fi++;
            const UiFlag* fl = &c->flags[fd->idx];
            if (fl->value) {
                const char* v = st_flag_val[g_sel][fd->idx][0] ? st_flag_val[g_sel][fd->idx] : "(unset)";
                ROW("  %s \033[33m%s %s\033[0m = %s %s \033[2m%s\033[0m",
                    cur, fl->name, fl->value, v, end, fl->desc);
            } else {
                ROW("  %s [%s] \033[33m%s\033[0m %s \033[2m%s\033[0m",
                    cur, st_flag_on[g_sel][fd->idx] ? "x" : " ", fl->name, end, fl->desc);
            }
        } else {  // F_RUN
            char cl[512];
            build_cmdline(cl, sizeof(cl));
            ROW("%s", "");
            ROW("  %s \xE2\x96\xB6 Run %s  \033[2m$ %s\033[0m", cur, end, cl);
        }
    }
    while (r < h) ROW("%s", "");
#undef ROW
}

static void draw_picker(void) {
    int bw = g_cols - 12;
    if (bw > 64) bw = 64;
    if (bw < 30) bw = 30;
    int bh = g_rows - 8;
    if (bh > 16) bh = 16;
    if (bh < 5) bh = 5;
    int x = (g_cols - bw) / 2 + 1;
    int y = (g_rows - bh) / 2;
    char line[512];
    fbf("\033[%d;%dH\xE2\x94\x8C", y, x);
    fb_hline_titled("Select file", bw - 2);
    fbf("\xE2\x94\x90");
    snprintf(line, sizeof(line), " \033[2m%s/\033[0m", g_picker_dir);
    fbf("\033[%d;%dH\xE2\x94\x82", y + 1, x);
    fb_pad(line, bw - 2);
    fbf("\xE2\x94\x82");
    int lh = bh - 3;
    if (g_picker_sel < g_picker_scroll) g_picker_scroll = g_picker_sel;
    if (g_picker_sel >= g_picker_scroll + lh) g_picker_scroll = g_picker_sel - lh + 1;
    for (int i = 0; i < lh; i++) {
        int idx = g_picker_scroll + i;
        fbf("\033[%d;%dH\xE2\x94\x82", y + 2 + i, x);
        if (idx < g_picker_n) {
            const char* mark = g_picker_isdir[idx] ? "/" : "";
            if (idx == g_picker_sel)
                snprintf(line, sizeof(line), "\033[7m  %s%s\033[0m", g_picker_names[idx], mark);
            else if (g_picker_isdir[idx])
                snprintf(line, sizeof(line), "  \033[1m%s/\033[0m", g_picker_names[idx]);
            else
                snprintf(line, sizeof(line), "  %s", g_picker_names[idx]);
            fb_pad(line, bw - 2);
        } else {
            fb_pad("", bw - 2);
        }
        fbf("\xE2\x94\x82");
    }
    fbf("\033[%d;%dH\xE2\x94\x94", y + bh - 1, x);
    for (int i = 0; i < bw - 2; i++) fbf("\xE2\x94\x80");
    fbf("\xE2\x94\x98");
}

static void draw(void) {
    g_fblen = 0;
    fbf("\033[H");
    if (g_cols < 80 || g_rows < 24) {
        fbf("\033[2J\033[H");
        char msg[128];
        snprintf(msg, sizeof(msg), "wyn ui needs at least an 80x24 terminal (now %dx%d)", g_cols, g_rows);
        int y = g_rows / 2;
        int x = (g_cols - (int)strlen(msg)) / 2;
        if (x < 1) x = 1;
        fbf("\033[%d;%dH%s", y, x, msg);
        fbf("\033[%d;%dH\033[2mresize the window, or press q to quit\033[0m", y + 1, x);
        fb_flush();
        return;
    }
    build_items();
    if (item_of_sel() < 0) select_first_cmd();
    draw_header();
    int top = 2, bot = g_rows - 1;             // frame rows
    char title[128];
    snprintf(title, sizeof(title), "%s", CMDS[g_sel].name);
    if (g_cols >= 90) {
        int lw = 26;                            // list interior width
        int dw = g_cols - lw - 3;               // detail interior width
        // top border
        fbf("\033[%d;1H\xE2\x94\x8C", top);
        fb_hline_titled(g_filter[0] ? "Commands (filtered)" : "Commands", lw);
        fbf("\xE2\x94\xAC");
        fb_hline_titled(title, dw);
        fbf("\xE2\x94\x90");
        int inner_h = bot - top - 1;
        for (int i = 0; i < inner_h; i++) {
            fbf("\033[%d;1H\xE2\x94\x82", top + 1 + i);
            fbf("\033[%d;%dH\xE2\x94\x82", top + 1 + i, lw + 2);
            fbf("\033[%d;%dH\xE2\x94\x82", top + 1 + i, g_cols);
        }
        draw_list(top + 1, inner_h, 2, lw);
        draw_detail(top + 1, inner_h, lw + 3, dw);
        fbf("\033[%d;1H\xE2\x94\x94", bot);
        for (int i = 0; i < lw; i++) fbf("\xE2\x94\x80");
        fbf("\xE2\x94\xB4");
        for (int i = 0; i < g_cols - lw - 3; i++) fbf("\xE2\x94\x80");
        fbf("\xE2\x94\x98");
    } else {
        // narrow: stack panes vertically
        int iw = g_cols - 2;
        int lh = (bot - top - 2) * 2 / 5;
        if (lh < 4) lh = 4;
        int dh = bot - top - 2 - lh;
        fbf("\033[%d;1H\xE2\x94\x8C", top);
        fb_hline_titled("Commands", iw);
        fbf("\xE2\x94\x90");
        for (int i = 0; i < lh; i++) {
            fbf("\033[%d;1H\xE2\x94\x82", top + 1 + i);
            fbf("\033[%d;%dH\xE2\x94\x82", top + 1 + i, g_cols);
        }
        draw_list(top + 1, lh, 2, iw);
        fbf("\033[%d;1H\xE2\x94\x9C", top + 1 + lh);
        fb_hline_titled(title, iw);
        fbf("\xE2\x94\xA4");
        for (int i = 0; i < dh; i++) {
            fbf("\033[%d;1H\xE2\x94\x82", top + 2 + lh + i);
            fbf("\033[%d;%dH\xE2\x94\x82", top + 2 + lh + i, g_cols);
        }
        draw_detail(top + 2 + lh, dh, 2, iw);
        fbf("\033[%d;1H\xE2\x94\x94", bot);
        for (int i = 0; i < iw; i++) fbf("\xE2\x94\x80");
        fbf("\xE2\x94\x98");
    }
    if (g_picker) draw_picker();
    draw_status();
    fb_flush();
}

// ─────────────────────────── run a command ────────────────────────────────

// Returns 0 to keep browsing, 1 to quit the TUI (with *exit_code set).
static int run_selected(int* exit_code) {
    const UiCmd* c = &CMDS[g_sel];
    if (c->arg != ARG_NONE && c->arg_required && !st_arg[g_sel][0]) {
        snprintf(g_status, sizeof(g_status),
                 "%s needs %s - set the argument first", c->name, c->arg_label);
        return 0;
    }
    char* av[24];
    char store[2048];
    build_argv(av, 24, store, sizeof(store));
    char cl[512];
    build_cmdline(cl, sizeof(cl));

    tui_restore();
    printf("\033[1m$ %s\033[0m\n", cl);
    fflush(stdout);

    if (c->takeover) {
        // Hand the terminal over entirely (watch/repl/lsp/ssh/logs).
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        execvp(av[0], av);
        fprintf(stderr, "wyn ui: failed to exec %s: %s\n", av[0], strerror(errno));
        *exit_code = 1;
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        execvp(av[0], av);
        fprintf(stderr, "wyn ui: failed to exec %s: %s\n", av[0], strerror(errno));
        _exit(127);
    }
    int status = 0;
    if (pid > 0) {
        // ctrl-c should go to the child, not kill the browser
        void (*old_int)(int) = signal(SIGINT, SIG_IGN);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        signal(SIGINT, old_int);
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status)
                                 : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
    printf("\n\033[2m[exit %d] press enter to return to wyn ui, q to quit\033[0m ", code);
    fflush(stdout);
    // one-key read without echo
    struct termios raw = g_saved_tio;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    unsigned char ch = '\n';
    ssize_t rd = read(STDIN_FILENO, &ch, 1);
    (void)rd;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_tio);
    printf("\n");
    if (ch == 'q' || ch == 'Q') {
        *exit_code = code;
        return 1;
    }
    tui_enter();
    term_size();
    return 0;
}

// ─────────────────────────────── main loop ────────────────────────────────

int cmd_ui(int argc, char** argv, const char* version) {
    if (argc > 2 && strcmp(argv[2], "--list-commands") == 0) return ui_list_commands();
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "wyn ui needs an interactive terminal.\n");
        return 1;
    }
    g_version = version;
    snprintf(g_argv0, sizeof(g_argv0), "%s", argv[0]);
    if (tcgetattr(STDIN_FILENO, &g_saved_tio) != 0) {
        fprintf(stderr, "wyn ui: cannot read terminal attributes.\n");
        return 1;
    }
    memset(st_flag_on, 0, sizeof(st_flag_on));
    memset(st_flag_val, 0, sizeof(st_flag_val));
    memset(st_arg, 0, sizeof(st_arg));
    memset(st_choice, 0, sizeof(st_choice));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_fatal_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_winch;  // no SA_RESTART: read() must wake with EINTR
    sigaction(SIGWINCH, &sa, NULL);
    atexit(tui_restore);

    tui_enter();
    term_size();
    build_items();
    select_first_cmd();

    int exit_code = 0;
    for (;;) {
        draw();
        int k = read_key();
        if (k == K_NONE) continue;
        if (k == K_EOF) { exit_code = 0; break; }  // stdin closed: never spin
        if (k == K_RESIZE) { term_size(); continue; }

        if (g_picker) {
            const UiCmd* c = &CMDS[g_sel];
            if (k == 'j' || k == K_DOWN) { if (g_picker_sel < g_picker_n - 1) g_picker_sel++; }
            else if (k == 'k' || k == K_UP) { if (g_picker_sel > 0) g_picker_sel--; }
            else if (k == K_ESC || k == 'q') g_picker = 0;
            else if (k == K_ENTER || k == 'l' || k == K_RIGHT) {
                if (g_picker_sel < g_picker_n) {
                    if (g_picker_isdir[g_picker_sel]) {
                        if (strcmp(g_picker_names[g_picker_sel], "..") == 0) {
                            char* s = strrchr(g_picker_dir, '/');
                            if (s) *s = 0; else snprintf(g_picker_dir, sizeof(g_picker_dir), ".");
                            if (!g_picker_dir[0]) snprintf(g_picker_dir, sizeof(g_picker_dir), ".");
                        } else {
                            char next[512];
                            picker_path(next, sizeof(next), g_picker_names[g_picker_sel]);
                            snprintf(g_picker_dir, sizeof(g_picker_dir), "%s", next);
                        }
                        picker_load(c->arg_ext);
                    } else {
                        picker_path(st_arg[g_sel], sizeof(st_arg[0]), g_picker_names[g_picker_sel]);
                        g_picker = 0;
                    }
                }
            }
            else if (k == 'h' || k == K_LEFT) {
                char* s = strrchr(g_picker_dir, '/');
                if (s) *s = 0; else snprintf(g_picker_dir, sizeof(g_picker_dir), ".");
                if (!g_picker_dir[0]) snprintf(g_picker_dir, sizeof(g_picker_dir), ".");
                picker_load(c->arg_ext);
            }
            continue;
        }

        if (g_filter_mode) {
            if (k == K_ENTER) g_filter_mode = 0;
            else if (k == K_ESC) { g_filter_mode = 0; g_filter[0] = 0; }
            else if (k == K_BACKSPACE) {
                size_t l = strlen(g_filter);
                if (l > 0) g_filter[l - 1] = 0;
            } else if (k >= 32 && k < 127 && strlen(g_filter) < sizeof(g_filter) - 1) {
                size_t l = strlen(g_filter);
                g_filter[l] = (char)k;
                g_filter[l + 1] = 0;
            }
            build_items();
            if (item_of_sel() < 0) select_first_cmd();
            continue;
        }

        if (k == 'q') { exit_code = 0; break; }
        if (k == '/') { g_filter_mode = 1; g_focus = 0; continue; }
        if (k == K_TAB) { g_focus = !g_focus; g_field = 0; continue; }

        if (g_cols < 80 || g_rows < 24) continue;  // tiny mode: only q/resize

        if (g_focus == 0) {  // list pane
            if (k == 'j' || k == K_DOWN) move_sel(1);
            else if (k == 'k' || k == K_UP) move_sel(-1);
            else if (k == 'g' || k == K_HOME) select_first_cmd();
            else if (k == 'G' || k == K_END) {
                for (int i = g_nitems - 1; i >= 0; i--)
                    if (g_items[i] >= 0) { g_sel = g_items[i]; break; }
            }
            else if (k == K_ENTER || k == 'l' || k == K_RIGHT) { g_focus = 1; g_field = 0; }
            else if (k == K_ESC && g_filter[0]) { g_filter[0] = 0; }
        } else {  // detail pane
            build_fields();
            Field* fd = &g_fields[g_field];
            const UiCmd* c = &CMDS[g_sel];
            if (k == 'j' || k == K_DOWN) { if (g_field < g_nfields - 1) g_field++; }
            else if (k == 'k' || k == K_UP) { if (g_field > 0) g_field--; }
            else if (k == K_ESC) { g_focus = 0; }
            else if (k == 'r') {
                if (run_selected(&exit_code)) break;
            }
            else if ((k == 'h' || k == K_LEFT) && fd->kind == F_CHOICE) {
                int n = choice_count(c);
                st_choice[g_sel] = (st_choice[g_sel] + n - 1) % n;
            }
            else if ((k == 'l' || k == K_RIGHT || k == ' ') && fd->kind == F_CHOICE) {
                int n = choice_count(c);
                st_choice[g_sel] = (st_choice[g_sel] + 1) % n;
            }
            else if (k == K_LEFT || k == 'h') { g_focus = 0; }
            else if (k == ' ' && fd->kind == F_FLAG && !c->flags[fd->idx].value) {
                st_flag_on[g_sel][fd->idx] = !st_flag_on[g_sel][fd->idx];
            }
            else if (k == K_ENTER || k == ' ') {
                if (fd->kind == F_RUN) {
                    if (run_selected(&exit_code)) break;
                } else if (fd->kind == F_FLAG) {
                    const UiFlag* fl = &c->flags[fd->idx];
                    if (fl->value) {
                        char label[96];
                        snprintf(label, sizeof(label), "%s %s", fl->name, fl->value);
                        edit_line(label, st_flag_val[g_sel][fd->idx], sizeof(st_flag_val[0][0]));
                    } else {
                        st_flag_on[g_sel][fd->idx] = !st_flag_on[g_sel][fd->idx];
                    }
                } else if (fd->kind == F_ARG) {
                    if (c->arg == ARG_FILE || c->arg == ARG_PATH) {
                        snprintf(g_picker_dir, sizeof(g_picker_dir), ".");
                        picker_load(c->arg_ext);
                        g_picker = 1;
                    } else {
                        edit_line(c->arg_label, st_arg[g_sel], sizeof(st_arg[0]));
                    }
                }
            }
        }
    }
    tui_restore();
    return exit_code;
}

#endif  // _WIN32
