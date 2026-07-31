#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#endif
#include "common.h"
#include "ast.h"
#include "wyn_interface.h"
#include "toml.h"

// ===========================================================================
// Native application packaging - `wyn build --app`
// ===========================================================================
//
// WHY THIS EXISTS: a GUI program (WynCanvas, anything on the SDL3 toolkit) built
// with a plain `wyn build` is a console executable. Double-clicking it on macOS
// opens Terminal and *then* the window; the Dock shows the terminal's icon and
// the process name; on Windows a console flashes up behind the window. That is
// the "doesn't look good for the end consumer" complaint. Fixing it is per
// platform and is entirely a matter of how the artifact is PACKAGED and LINKED -
// no language or codegen change is involved, which is why it all lives here.
//
//   macOS   - a real .app bundle. A bundle (not a bare Mach-O) is what gives an
//             icon, a Dock name, and a double-click that does not open Terminal.
//             NSHighResolutionCapable in Info.plist is load-bearing: without it
//             AppKit runs the app in 1x-upscaled mode and every window is blurry
//             on a Retina display.
//   Windows - `-mwindows`, i.e. PE subsystem WINDOWS instead of CONSOLE. That
//             single flag is the whole difference between a console window
//             appearing behind the GUI and not.
//   Linux   - no bundle format exists; the equivalent affordance is a .desktop
//             entry (with Terminal=false) plus an icon, so a file manager or
//             launcher shows a named, clickable application.
//
// SPELLING: BOTH a flag and a manifest section, because they carry different
// things. `--app` is the trigger (a build-time choice, and never inferred - see
// wyn_app_begin). `[app]` in wyn.toml carries the metadata, because a bundle
// identifier, an icon path and a category have nowhere sensible to live on a
// command line and must be committed with the project.
//
// STAGING: the C compiler is invoked through system(), so its `-o` argument is
// shell text. A bundle is routinely named "My Great App.app" - a path with
// spaces - so the binary is always linked to a space-free staging path under
// TMPDIR and MOVED into the bundle afterwards. One uniform path, instead of a
// quoted variant that only some link lines would use.

typedef enum { WYN_APP_OFF, WYN_APP_MACOS, WYN_APP_WINDOWS, WYN_APP_LINUX } WynAppKind;

// Field sizes are deliberate, not decorative: every path below is composed with
// snprintf, and GCC's -Wformat-truncation (on in CI, stricter than the clang
// used here) warns whenever a destination provably cannot hold its bounded
// inputs. Each derived buffer in this file is sized to the sum of its sources.
static struct {
    int         on;
    WynAppKind  kind;
    char artifact[512];    // what the user gets: Foo.app / Foo.exe / Foo
    char link_path[512];   // where the C compiler must write the binary
    char stage_dir[400];   // per-build staging dir holding exactly link_path
    char exec_name[64];    // CFBundleExecutable / file name inside MacOS/
    char display[128];     // CFBundleName - may contain spaces
    char identifier[128];
    char version[64];
    char icon[512];
    char category[128];
    char min_system[64];
    char resources[512];
    char cwd_mode[32];     // "", "resources" or "bundle" (macOS launcher)
    int  icon_missing;     // configured icon did not exist - warn, never fatal
} A;

static void app_copy(char* dst, size_t n, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    snprintf(dst, n, "%s", src);
}

// A file name safe to place in a shell-free path and in CFBundleExecutable.
// Spaces are dropped rather than substituted, so `name = "My Great App"` yields
// the executable `MyGreatApp` while CFBundleName keeps the spaces - which is
// where the Dock actually reads the app's name from.
static void app_sanitize(char* out, size_t n, const char* in) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < n; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '-' || c == '.') out[j++] = (char)c;
    }
    if (j == 0 && n > 3) { out[j++] = 'a'; out[j++] = 'p'; out[j++] = 'p'; }
    out[j] = '\0';
}

// Reverse-DNS-ish default identifier. Only [A-Za-z0-9.-] is legal in a
// CFBundleIdentifier, and LaunchServices treats two apps sharing an identifier
// as the same app - hence the warning in wyn_app_begin when this default is used.
static void app_default_identifier(char* out, size_t n, const char* name) {
    char safe[64];   // <= A.exec_name; "com.wyn." + 64 fits A.identifier[128]
    app_sanitize(safe, sizeof(safe), name);
    for (char* c = safe; *c; c++) *c = (char)tolower((unsigned char)*c);
    snprintf(out, n, "com.wyn.%s", safe);
}

static int app_mkdir(const char* path) {
#ifdef _WIN32
    if (mkdir(path) == 0) return 0;
#else
    if (mkdir(path, 0755) == 0) return 0;
#endif
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : -1;   // already there
}

static int app_file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// rename() first: within one filesystem it is atomic and cheap. The copy is the
// fallback for the cross-device case, which TMPDIR staging makes the common one
// (a bundle written to /Volumes/... or a container mount).
static int app_move_file(const char* from, const char* to) {
    remove(to);
    if (rename(from, to) == 0) return 0;
    FILE* in = fopen(from, "rb");
    if (!in) return -1;
    FILE* out = fopen(to, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) { remove(to); return -1; }
    remove(from);
#ifndef _WIN32
    chmod(to, 0755);   // the linker's mode is not preserved by a byte copy
#endif
    return 0;
}

static int app_copy_file(const char* from, const char* to) {
    FILE* in = fopen(from, "rb");
    if (!in) return -1;
    FILE* out = fopen(to, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) { remove(to); return -1; }
    return 0;
}

static const char* app_basename(const char* p) {
    const char* s = strrchr(p, '/');
#ifdef _WIN32
    const char* b = strrchr(p, '\\');
    if (b && (!s || b > s)) s = b;
#endif
    return s ? s + 1 : p;
}

// Write one <key>/<string> pair, XML-escaping the value. Escaping straight to
// the stream (rather than into a fixed buffer) is what makes an `&` in a project
// name safe without a size calculation: a name of N chars can expand to 6N, and
// a truncated expansion would emit a half-entity that plutil rejects.
static void app_plist_pair(FILE* f, const char* key, const char* value) {
    fprintf(f, "\t<key>%s</key>\n\t<string>", key);
    for (const char* p = value; *p; p++) {
        switch (*p) {
            case '&':  fputs("&amp;",  f); break;
            case '<':  fputs("&lt;",   f); break;
            case '>':  fputs("&gt;",   f); break;
            case '"':  fputs("&quot;", f); break;
            case '\'': fputs("&apos;", f); break;
            default:   fputc(*p, f);       break;
        }
    }
    fputs("</string>\n", f);
}

// wyn.toml lives at the project root, which is the cwd for every documented
// invocation - but `wyn build sub/proj/src/ui.wyn --app` is legal too, so also
// look beside the entry file and one level up from it (the src/ layout).
//
// `root_out` receives the manifest's directory ("" for the cwd), because the
// bundle belongs next to the wyn.toml that named it, NOT next to the entry file.
// `wyn build src/ui.wyn --app` in a project must produce ./MyApp.app, not
// ./src/MyApp.app - a .app inside src/ looks like a source file and is not where
// anyone goes looking for the thing they just built.
static WynConfig* app_load_config(const char* entry, char* root_out, size_t root_n) {
    if (root_n) root_out[0] = '\0';
    WynConfig* c = wyn_config_parse("wyn.toml");
    if (c) return c;
    if (!entry) return NULL;
    char dir[300];
    snprintf(dir, sizeof(dir), "%s", entry);
    char* slash = strrchr(dir, '/');
    if (!slash) return NULL;
    *slash = '\0';
    char path[512];
    snprintf(path, sizeof(path), "%s/wyn.toml", dir);
    c = wyn_config_parse(path);
    if (c) { snprintf(root_out, root_n, "%s", dir); return c; }
    slash = strrchr(dir, '/');
    if (!slash) return NULL;
    *slash = '\0';
    snprintf(path, sizeof(path), "%s/wyn.toml", dir);
    c = wyn_config_parse(path);
    if (c) snprintf(root_out, root_n, "%s", dir);
    return c;
}

// Decide whether to package and where the binary must be linked.
//
// `app_flag` is `--app` on the command line; `[app] bundle = true` in wyn.toml
// is the equivalent in-manifest opt-in. Packaging is NEVER inferred from "this
// looks like a GUI program": guessing would change the artifact a user's scripts
// already depend on, and a mis-guess is silent. Returns 1 when packaging is on
// (and fills `link_path`), 0 when off, -1 on error.
int wyn_app_begin(int app_flag, const char* app_target, const char* entry,
                  const char* output_name, char* link_path, size_t link_n,
                  int plan_only) {
    memset(&A, 0, sizeof(A));

    char proj_root[300] = "";
    WynConfig* c = app_load_config(entry, proj_root, sizeof(proj_root));
    int want = app_flag || (c && c->app.bundle);
    if (!want) { if (c) wyn_config_free(c); return 0; }

    // Target: --app-target overrides the host, and selects which packaging is
    // produced. The C compiler this command drives is the HOST compiler, so a
    // non-host target can only PLAN (--app-plan): it emits the metadata and the
    // link flags for inspection. Actually linking one would be a lie - the
    // artifact would be a host binary wearing another platform's name, and on
    // Windows the host clang rejects -mwindows outright ("unsupported option
    // '-mwindows' for target 'arm64-apple-darwin'"), which is how this was found.
    const char* host =
#ifdef __APPLE__
        "macos";
#elif defined(_WIN32)
        "windows";
#else
        "linux";
#endif
    const char* t = (app_target && *app_target) ? app_target : host;
    if (strcmp(t, "macos") == 0 || strcmp(t, "mac") == 0 || strcmp(t, "darwin") == 0)
        A.kind = WYN_APP_MACOS;
    else if (strcmp(t, "windows") == 0 || strcmp(t, "win") == 0 || strcmp(t, "win64") == 0)
        A.kind = WYN_APP_WINDOWS;
    else if (strcmp(t, "linux") == 0)
        A.kind = WYN_APP_LINUX;
    else {
        fprintf(stderr, "\033[31m✗\033[0m --app-target: unknown target '%s' (macos | windows | linux)\n", t);
        if (c) wyn_config_free(c);
        return -1;
    }
    if (!plan_only && strcmp(t, host) != 0) {
        fprintf(stderr, "\033[31m✗\033[0m --app-target %s cannot be LINKED on a %s host "
                        "(`wyn build` drives the host C compiler).\n", t, host);
        fprintf(stderr, "  Use --app-plan --app-target %s to emit and inspect its packaging,\n", t);
        fprintf(stderr, "  or run `wyn build --app` on a %s machine (that is what CI does).\n", t);
        if (c) wyn_config_free(c);
        return -1;
    }

    // Display name: [app] name, else -o's stem, else the entry file's stem.
    char stem[128] = "";
    if (output_name && *output_name) {
        app_copy(stem, sizeof(stem), app_basename(output_name));
        char* dot = strrchr(stem, '.');
        if (dot && (strcmp(dot, ".app") == 0 || strcmp(dot, ".exe") == 0)) *dot = '\0';
    } else if (entry) {
        app_copy(stem, sizeof(stem), app_basename(entry));
        char* dot = strrchr(stem, '.');
        if (dot) *dot = '\0';
    }
    if (c && c->app.name && *c->app.name) app_copy(A.display, sizeof(A.display), c->app.name);
    else if (stem[0])                     app_copy(A.display, sizeof(A.display), stem);
    else                                  app_copy(A.display, sizeof(A.display), "App");

    // The on-disk stem follows -o when given (the user named the artifact), and
    // the display name otherwise - so `[app] name = "My Great App"` with no -o
    // produces "My Great App.app", which is what a macOS user expects to see.
    char disk[128];   // fits A.artifact with dir[300] + "/" + ".app"
    app_copy(disk, sizeof(disk), (output_name && *output_name) ? stem : A.display);
    app_sanitize(A.exec_name, sizeof(A.exec_name), disk);

    if (c && c->app.identifier && *c->app.identifier) {
        app_copy(A.identifier, sizeof(A.identifier), c->app.identifier);
    } else {
        app_default_identifier(A.identifier, sizeof(A.identifier), A.display);
        if (A.kind == WYN_APP_MACOS)
            fprintf(stderr, "\033[33mnote:\033[0m no [app] identifier in wyn.toml - using %s. "
                            "Two apps sharing an identifier are ONE app to macOS; set your own before shipping.\n",
                    A.identifier);
    }
    if (c && c->app.version && *c->app.version)          app_copy(A.version, sizeof(A.version), c->app.version);
    else if (c && c->project.version && *c->project.version) app_copy(A.version, sizeof(A.version), c->project.version);
    else                                                  app_copy(A.version, sizeof(A.version), "1.0.0");
    if (c && c->app.category)   app_copy(A.category, sizeof(A.category), c->app.category);
    if (c && c->app.min_system && *c->app.min_system) app_copy(A.min_system, sizeof(A.min_system), c->app.min_system);
    else                                              app_copy(A.min_system, sizeof(A.min_system), "11.0");
    if (c && c->app.resources)  app_copy(A.resources, sizeof(A.resources), c->app.resources);
    if (c && c->app.icon && *c->app.icon) {
        app_copy(A.icon, sizeof(A.icon), c->app.icon);
        if (!app_file_exists(A.icon)) {
            // NOT fatal: an icon is cosmetic, and failing a build over a missing
            // .icns would be a worse experience than shipping the default one.
            fprintf(stderr, "\033[33mwarning:\033[0m [app] icon '%s' not found - building without an icon\n", A.icon);
            A.icon_missing = 1;
            A.icon[0] = '\0';
        }
    }
    if (c && c->app.cwd && *c->app.cwd) {
        if (strcmp(c->app.cwd, "resources") == 0 || strcmp(c->app.cwd, "bundle") == 0)
            app_copy(A.cwd_mode, sizeof(A.cwd_mode), c->app.cwd);
        else
            fprintf(stderr, "\033[33mwarning:\033[0m [app] cwd = \"%s\" ignored "
                            "(expected \"resources\" or \"bundle\")\n", c->app.cwd);
    }
    int have_manifest = (c != NULL);
    if (c) wyn_config_free(c);

    // Where the artifact goes. With -o the user's path wins (the platform
    // extension is appended when missing); otherwise it sits beside the entry
    // file, which is where a plain `wyn build` already puts its binary.
    // With a manifest, "beside the wyn.toml" ("" = cwd). Without one, beside the
    // entry file, matching where a plain `wyn build` puts its binary.
    char dir[300] = "";
    if (!(output_name && *output_name)) {
        if (have_manifest) {
            app_copy(dir, sizeof(dir), proj_root);
        } else if (entry) {
            app_copy(dir, sizeof(dir), entry);
            char* slash = strrchr(dir, '/');
            if (slash) *slash = '\0'; else dir[0] = '\0';
        }
    }
    const char* ext = (A.kind == WYN_APP_MACOS) ? ".app" : (A.kind == WYN_APP_WINDOWS) ? ".exe" : "";
    if (output_name && *output_name) {
        const char* have = strrchr(app_basename(output_name), '.');
        int has_ext = have && *ext && strcmp(have, ext) == 0;
        snprintf(A.artifact, sizeof(A.artifact), "%s%s", output_name, has_ext ? "" : ext);
    } else if (dir[0]) {
        snprintf(A.artifact, sizeof(A.artifact), "%s/%s%s", dir, disk, ext);
    } else {
        snprintf(A.artifact, sizeof(A.artifact), "%s%s", disk, ext);
    }

    // ALWAYS stage, on every platform. The link line is shell text handed to
    // system(), so a `-o My Great App.exe` is split into two arguments and clang
    // reports `no such file or directory: 'App'` - measured, on the Linux/Windows
    // path that originally linked straight to the artifact on the theory that
    // only a bundle needed staging. A name with a space is the norm for an app,
    // so one uniform space-free staging path it is.
    //
    // The pid goes in the DIRECTORY, never in the file name: on Apple platforms
    // ld writes an ad-hoc "linker-signed" signature whose identifier is derived
    // from the output file's name, so staging to `wyn_app_1234_Foo.bin` shipped
    // an app whose `codesign -dv` identity read `wyn_app_1234_Foo.bin`. The file
    // has to already be called what the app is called.
    const char* tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    size_t tl = strlen(tmp);
    snprintf(A.stage_dir, sizeof(A.stage_dir), "%s%swyn_app_%ld",
             tmp, (tl && (tmp[tl-1] == '/')) ? "" : "/", (long)getpid());
    if (app_mkdir(A.stage_dir) != 0) {
        fprintf(stderr, "\033[31m✗\033[0m could not create staging directory %s\n", A.stage_dir);
        return -1;
    }
    snprintf(A.link_path, sizeof(A.link_path), "%s/%s", A.stage_dir, A.exec_name);
    A.on = 1;
    snprintf(link_path, link_n, "%s", A.link_path);
    return 1;
}

// Extra flags the C compiler needs for a windowed application. Only Windows
// needs one, and it is the entire point of --app there.
//
// ENTRY POINT: the generated C emits a plain `int main(...)`, and it stays that
// way. mingw-w64's crtexe.c defines BOTH mainCRTStartup and WinMainCRTStartup
// and funnels both into __tmainCRTStartup, which calls `main`; -mwindows only
// selects the PE subsystem (and pulls in the GUI import libs), it does not
// require the program to define WinMain. So no shim is emitted. This is asserted
// at the flag level by tests/errors/run_app_bundle_test.sh and NOT executed on
// the macOS dev box - there is no mingw here to link with.
const char* wyn_app_link_flags(void) {
    if (!A.on) return "";
    return (A.kind == WYN_APP_WINDOWS) ? " -mwindows" : "";
}

const char* wyn_app_artifact(void) { return A.on ? A.artifact : ""; }

static int app_write_info_plist(const char* bundle) {
    char path[640];
    snprintf(path, sizeof(path), "%s/Contents/Info.plist", bundle);
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "\033[31m✗\033[0m could not write %s\n", path); return -1; }

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
               "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
               "<plist version=\"1.0\">\n<dict>\n");
    app_plist_pair(f, "CFBundleExecutable", A.exec_name);
    app_plist_pair(f, "CFBundleIdentifier", A.identifier);
    app_plist_pair(f, "CFBundleName", A.display);
    app_plist_pair(f, "CFBundleDisplayName", A.display);
    app_plist_pair(f, "CFBundlePackageType", "APPL");
    app_plist_pair(f, "CFBundleInfoDictionaryVersion", "6.0");
    app_plist_pair(f, "CFBundleShortVersionString", A.version);
    app_plist_pair(f, "CFBundleVersion", A.version);
    // Without this the window is rendered at 1x and upscaled - blurry on every
    // Retina display. It is the single most visible item in this file.
    fprintf(f, "\t<key>NSHighResolutionCapable</key>\n\t<true/>\n");
    app_plist_pair(f, "LSMinimumSystemVersion", A.min_system);
    if (A.icon[0]) app_plist_pair(f, "CFBundleIconFile", app_basename(A.icon));
    if (A.category[0]) app_plist_pair(f, "LSApplicationCategoryType", A.category);
    fprintf(f, "</dict>\n</plist>\n");
    fclose(f);
    return 0;
}

// Map [app] category to a freedesktop Categories value. An Apple UTI
// ("public.app-category.graphics-design") is recognised and translated, because
// one manifest key has to serve both platforms; anything already ending in ';'
// is taken as a literal freedesktop list and passed through.
static const char* app_desktop_categories(void) {
    if (!A.category[0]) return "Utility;";
    const char* c = A.category;
    if (strncmp(c, "public.app-category.", 20) != 0) {
        // Already freedesktop-shaped. Add the trailing ';' the spec wants if the
        // user left it off, which is the common slip.
        if (strchr(c, ';')) return c;
        static char withsemi[130];
        snprintf(withsemi, sizeof(withsemi), "%s;", c);
        return withsemi;
    }
    const char* uti = c + 20;
    if (strstr(uti, "developer") || strstr(uti, "utilities")) return "Development;";
    if (strstr(uti, "graphics") || strstr(uti, "photo") || strstr(uti, "design")) return "Graphics;";
    if (strstr(uti, "games"))       return "Game;";
    if (strstr(uti, "music") || strstr(uti, "video") || strstr(uti, "entertainment")) return "AudioVideo;";
    if (strstr(uti, "education"))   return "Education;";
    if (strstr(uti, "productivity")) return "Office;";
    if (strstr(uti, "network") || strstr(uti, "social")) return "Network;";
    return "Utility;";
}

static int app_write_desktop_entry(void) {
    // Exec must be absolute: a launcher does not run from the build directory.
    char abs[1100];   // getcwd(512) + "/" + A.artifact(512)
    if (A.artifact[0] == '/') {
        snprintf(abs, sizeof(abs), "%s", A.artifact);
    } else {
        char cwd[512];
        if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");
        snprintf(abs, sizeof(abs), "%s/%s", cwd, A.artifact);
    }
    char dir[512];
    app_copy(dir, sizeof(dir), A.artifact);
    char* slash = strrchr(dir, '/');
    if (slash) *slash = '\0'; else app_copy(dir, sizeof(dir), ".");
    char path[700];
    snprintf(path, sizeof(path), "%s/%s.desktop", dir, A.exec_name);
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "\033[31m✗\033[0m could not write %s\n", path); return -1; }
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Name=%s\n", A.display);
    // Exec is parsed per the Desktop Entry Spec, not by a shell: an unquoted
    // path with a space is read as `program + argument`, so `[app] name = "My
    // Great App"` would launch nothing at all. The spec's quoting is a
    // double-quoted string with `\` escaping `"` and `\`.
    fputs("Exec=", f);
    if (strchr(abs, ' ')) {
        fputc('"', f);
        for (const char* p = abs; *p; p++) {
            if (*p == '"' || *p == '\\') fputc('\\', f);
            fputc(*p, f);
        }
        fputc('"', f);
    } else {
        fputs(abs, f);
    }
    fputc('\n', f);
    // The Linux half of "no terminal window": a launcher honours this key, and
    // without it some desktops open the app inside a terminal emulator.
    fprintf(f, "Terminal=false\n");
    // Categories are freedesktop main-category tokens ("Graphics;"), NOT the
    // Apple UTI that LSApplicationCategoryType wants. A single [app] category
    // cannot be both, so an Apple-style value is mapped to its freedesktop
    // counterpart rather than written verbatim into a file where a launcher
    // would silently ignore it.
    fprintf(f, "Categories=%s\n", app_desktop_categories());
    if (A.icon[0]) {
        char icon_abs[1100];   // getcwd(512) + "/" + A.icon(512)
        if (A.icon[0] == '/') snprintf(icon_abs, sizeof(icon_abs), "%s", A.icon);
        else {
            char cwd[512];
            if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");
            snprintf(icon_abs, sizeof(icon_abs), "%s/%s", cwd, A.icon);
        }
        fprintf(f, "Icon=%s\n", icon_abs);
    }
    fprintf(f, "X-Wyn-Version=%s\n", A.version);
    fclose(f);
    printf("  desktop entry: %s\n", path);
    return 0;
}

// The macOS working-directory problem, and the only fix available without a
// runtime change: a .app launched from Finder gets cwd "/", so a program that
// opens "assets/foo.png" relative to cwd works from the terminal and breaks the
// moment it is double-clicked.
//
// Opt in with `[app] cwd = "resources"` (or "bundle") and the real binary is
// installed as <exec>-bin with a two-line /bin/sh launcher in its place, which
// chdirs and execs it. The launcher also exports WYN_APP_RESOURCES, so a program
// can locate bundled assets explicitly via System.env() instead of relying on
// cwd at all.
//
// Left OFF by default on purpose: with no `cwd` key the bundle contains a plain
// Mach-O executable, which is what codesign/notarization and `ps` expect. The
// limitation is real, opt-in is the honest default, and it is documented here
// and in the test.
static int app_write_launcher(const char* bundle) {
    char real[900], script[900];
    snprintf(real,   sizeof(real),   "%s/Contents/MacOS/%s-bin", bundle, A.exec_name);
    snprintf(script, sizeof(script), "%s/Contents/MacOS/%s", bundle, A.exec_name);
    if (app_move_file(script, real) != 0) {
        fprintf(stderr, "\033[31m✗\033[0m could not stage %s\n", real);
        return -1;
    }
    FILE* f = fopen(script, "w");
    if (!f) { fprintf(stderr, "\033[31m✗\033[0m could not write %s\n", script); return -1; }
    const char* sub = (strcmp(A.cwd_mode, "bundle") == 0) ? "../.." : "../Resources";
    fprintf(f, "#!/bin/sh\n"
               "# Generated by `wyn build --app` for [app] cwd = \"%s\".\n"
               "# A Finder launch gives cwd \"/\"; relative asset paths need this.\n"
               "d=$(cd \"$(dirname \"$0\")\" && pwd)\n"
               "WYN_APP_RESOURCES=\"$d/../Resources\"; export WYN_APP_RESOURCES\n"
               "cd \"$d/%s\" || exit 1\n"
               "exec \"$d/%s-bin\" \"$@\"\n",
            A.cwd_mode, sub, A.exec_name);
    fclose(f);
#ifndef _WIN32
    chmod(script, 0755);
#endif
    return 0;
}

// Create the bundle skeleton and write every metadata file. Split out of
// finalize so `--app-plan` can emit and show the metadata without compiling -
// which is also how the Windows and Linux generators get tested on a Mac.
int wyn_app_emit_metadata(void) {
    if (!A.on) return 0;
    if (A.kind == WYN_APP_LINUX) return app_write_desktop_entry();
    if (A.kind == WYN_APP_WINDOWS) {
        if (A.icon[0])
            fprintf(stderr, "\033[33mnote:\033[0m [app] icon is not embedded on Windows yet "
                            "(that needs a windres-compiled .rc); the .exe still links as a GUI app.\n");
        return 0;
    }

    char contents[600], macos[640], resources[640];
    snprintf(contents,  sizeof(contents),  "%s/Contents", A.artifact);
    snprintf(macos,     sizeof(macos),     "%s/MacOS", contents);
    snprintf(resources, sizeof(resources), "%s/Resources", contents);
    if (app_mkdir(A.artifact) != 0 || app_mkdir(contents) != 0 ||
        app_mkdir(macos) != 0 || app_mkdir(resources) != 0) {
        fprintf(stderr, "\033[31m✗\033[0m could not create bundle directories under %s\n", A.artifact);
        return -1;
    }
    if (app_write_info_plist(A.artifact) != 0) return -1;
    // "APPL" in PkgInfo alongside CFBundlePackageType: legacy, tiny, and still
    // what some Finder/LaunchServices paths sniff first.
    { char pk[640]; snprintf(pk, sizeof(pk), "%s/PkgInfo", contents);
      FILE* p = fopen(pk, "w"); if (p) { fputs("APPL????", p); fclose(p); } }
    if (A.icon[0]) {
        char dest[1400];
        snprintf(dest, sizeof(dest), "%s/%s", resources, app_basename(A.icon));
        if (app_copy_file(A.icon, dest) != 0)
            fprintf(stderr, "\033[33mwarning:\033[0m could not copy icon %s into the bundle\n", A.icon);
    }
    if (A.resources[0]) {
        // A directory copy is the one place a shell is genuinely simpler than
        // C, and both paths are ours (quoted for names with spaces).
        struct stat st;
        if (stat(A.resources, &st) == 0 && S_ISDIR(st.st_mode)) {
            char cp[1400];
            snprintf(cp, sizeof(cp), "cp -R '%s/.' '%s/' 2>/dev/null", A.resources, resources);
            if (system(cp) != 0)
                fprintf(stderr, "\033[33mwarning:\033[0m could not copy [app] resources '%s'\n", A.resources);
        } else {
            fprintf(stderr, "\033[33mwarning:\033[0m [app] resources '%s' is not a directory - skipped\n", A.resources);
        }
    }
    return 0;
}

// Called after a successful link: move the staged binary into the bundle and
// write the metadata. Only macOS has anything to move.
int wyn_app_finalize(void) {
    if (!A.on) return 0;
    if (wyn_app_emit_metadata() != 0) return -1;

    char dest[1400];
    if (A.kind == WYN_APP_MACOS)
        snprintf(dest, sizeof(dest), "%s/Contents/MacOS/%s", A.artifact, A.exec_name);
    else
        snprintf(dest, sizeof(dest), "%s", A.artifact);   // Foo.exe / bare binary
    if (app_move_file(A.link_path, dest) != 0) {
        fprintf(stderr, "\033[31m✗\033[0m could not install the executable at %s\n", dest);
        return -1;
    }
#ifndef _WIN32
    chmod(dest, 0755);
#endif
    if (A.cwd_mode[0] && app_write_launcher(A.artifact) != 0) return -1;
    if (A.stage_dir[0]) remove(A.stage_dir);   // empty now; don't litter TMPDIR
    return 0;
}

// `--app-plan`: describe the packaging decision. Exists so the parts that cannot
// be RUN on this machine (the Windows subsystem flag, the Linux .desktop file)
// are still asserted on by a test, instead of being taken on trust.
void wyn_app_describe(void) {
    const char* k = A.kind == WYN_APP_MACOS ? "macos-bundle" :
                    A.kind == WYN_APP_WINDOWS ? "windows-gui" : "linux-desktop";
    printf("app-plan: %s\n", k);
    printf("  artifact:   %s\n", A.artifact);
    printf("  executable: %s\n", A.exec_name);
    printf("  name:       %s\n", A.display);
    printf("  identifier: %s\n", A.identifier);
    printf("  version:    %s\n", A.version);
    printf("  icon:       %s\n", A.icon[0] ? A.icon : (A.icon_missing ? "(missing - skipped)" : "(none)"));
    printf("  category:   %s\n", A.category[0] ? A.category : "(none)");
    printf("  link-flags:%s\n", wyn_app_link_flags()[0] ? wyn_app_link_flags() : " (none)");
    printf("  link-path:  %s\n", A.link_path);
}

// Forward declarations
extern void init_lexer(const char* source);
extern void init_parser();
extern void init_checker();
extern void init_codegen(FILE* output);
extern Program* parse_program();
extern void check_program(Program* prog);
extern bool checker_had_error();
extern void free_program(Program* prog);
extern void codegen_c_header();
extern void codegen_program(Program* prog);

// Compile a single file
static int compile_file_with_output(const char* filename, const char* output_name) {
    char* source = wyn_read_file(filename);
    
    // Generate output filename
    char output_c[512];
    snprintf(output_c, sizeof(output_c), "%s.c", filename);
    
    FILE* output = fopen(output_c, "w");
    if (!output) {
        fprintf(stderr, "Error: Could not create output file '%s'\n", output_c);
        free(source);
        return 1;
    }
    
    init_lexer(source);
    init_parser();
    init_checker();
    init_codegen(output);
    
    Program* prog = parse_program();
    if (!prog) {
        fprintf(stderr, "Error: Failed to parse program\n");
        fclose(output);
        free(source);
        return 1;
    }
    
    check_program(prog);
    if (checker_had_error()) {
        fclose(output);
        free_program(prog);
        free(source);
        return 1;
    }
    
    codegen_c_header();
    codegen_program(prog);
    
    fclose(output);
    free_program(prog);
    free(source);
    
    // Compile C to binary
    char output_bin[512];
    if (output_name) {
        snprintf(output_bin, sizeof(output_bin), "%s", output_name);
    } else {
        snprintf(output_bin, sizeof(output_bin), "%s.out", filename);
    }
    
    // Get the directory where wyn binary is located
    char wyn_dir[1024] = ".";
    char* wyn_path = getenv("WYN_ROOT");
    if (wyn_path) {
        snprintf(wyn_dir, sizeof(wyn_dir), "%s", wyn_path);
    } else {
        // Auto-detect: try common locations
        const char* search_paths[] = {
            ".",
            "./wyn",
            "../",
            "../../",
            "/usr/local/share/wyn",
            "/usr/share/wyn",
            NULL
        };
        
        for (int i = 0; search_paths[i] != NULL; i++) {
            char test_path[1024];
            snprintf(test_path, sizeof(test_path), "%s/src/wyn_wrapper.c", search_paths[i]);
            FILE* test = fopen(test_path, "r");
            if (test) {
                fclose(test);
                snprintf(wyn_dir, sizeof(wyn_dir), "%s", search_paths[i]);
                break;
            }
        }
    }
    
    char cmd[4096];

    // Read source to detect modules that need extra link flags
    char* source_content = NULL;
    FILE* src_f = fopen(output_c, "r");
    if (!src_f) src_f = fopen(filename, "r");
    if (src_f) {
        fseek(src_f, 0, SEEK_END);
        long sz = ftell(src_f);
        fseek(src_f, 0, SEEK_SET);
        source_content = malloc(sz + 1);
        fread(source_content, 1, sz, src_f);
        source_content[sz] = '\0';
        fclose(src_f);
    }

    const char* extra_flags = "";
    if (source_content) {
        if (strstr(source_content, "App.") || strstr(source_content, "App_create")) {
#ifdef __APPLE__
            extra_flags = " -framework WebKit -framework Cocoa";
            char wv_obj[512];
            snprintf(wv_obj, sizeof(wv_obj), "%s/src/wyn_webview.o", wyn_dir);
            FILE* wv_test = fopen(wv_obj, "r");
            if (wv_test) {
                fclose(wv_test);
                static char wv_flags[1024];
                snprintf(wv_flags, sizeof(wv_flags), " %s -framework WebKit -framework Cocoa", wv_obj);
                extra_flags = wv_flags;
            }
#elif defined(__linux__)
            extra_flags = " $(pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0 2>/dev/null)";
#elif defined(_WIN32)
            {
                static char wv_flags[1024];
                snprintf(wv_flags, sizeof(wv_flags), " %s/src/wyn_webview_win.c", wyn_dir);
                extra_flags = wv_flags;
            }
#endif
        }
        free(source_content);
    }

    // Try precompiled runtime first, fall back to source
    char rt_path[512];
    snprintf(rt_path, sizeof(rt_path), "%s/runtime/libwyn_rt.a", wyn_dir);
    FILE* rt_test = fopen(rt_path, "r");
    if (rt_test) {
        fclose(rt_test);
        snprintf(cmd, sizeof(cmd),
                 "gcc -O2 -w -I %s/src -o %s %s %s/runtime/libwyn_rt.a "
                 "-L%s/runtime/parser_lib -lwyn_c_parser -lpthread -lm%s 2>&1",
                 wyn_dir, output_bin, output_c, wyn_dir, wyn_dir, extra_flags);
    } else {
        snprintf(cmd, sizeof(cmd), 
                 "gcc -O2 -w -I %s/src -o %s %s %s/src/wyn_wrapper.c %s/src/wyn_interface.c "
                 "%s/src/io.c %s/src/optional.c %s/src/result.c %s/src/arc_runtime.c "
                 "%s/src/concurrency.c %s/src/async_runtime.c "
                 "%s/src/safe_memory.c %s/src/error.c %s/src/string_runtime.c "
                 "%s/src/hashmap.c %s/src/hashset.c %s/src/json.c %s/src/json_runtime.c %s/src/stdlib_runtime.c %s/src/hashmap_runtime.c "
                 "%s/src/stdlib_string.c %s/src/stdlib_array.c %s/src/stdlib_time.c %s/src/stdlib_crypto.c "
                 "%s/src/spawn.c %s/src/net.c %s/src/net_runtime.c "
                 "%s/src/test_runtime.c %s/src/net_advanced.c "
                 "-L%s/runtime/parser_lib -lwyn_c_parser -lpthread -lm%s 2>&1",
                 wyn_dir, output_bin, output_c,
                 wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir,
                 wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir,
                 wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir,
                 wyn_dir, wyn_dir, wyn_dir, wyn_dir, wyn_dir, extra_flags);
    }
    
    if (getenv("WYN_DEBUG")) fprintf(stderr, "CMD: %s\n", cmd); int result = system(cmd);
    if (result != 0) {
        return 1;
    }
    
    printf("Compiled successfully: %s\n", output_bin);
    return 0;
}

#ifndef _WIN32
// Find main.wyn in directory
static char* find_main_file(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return NULL;
    
    struct dirent* entry;
    static char path[512];
    
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, "main.wyn") == 0) {
            snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
            closedir(d);
            return path;
        }
    }
    
    closedir(d);
    return NULL;
}
#endif

// Main compile command
int cmd_compile(const char* target, int argc, char** argv) {
    // Check for -o flag
    const char* output_name = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[i + 1];
            break;
        }
    }
    
    // Check if target is a file or directory
    FILE* f = fopen(target, "r");
    if (f) {
        // It's a file
        fclose(f);
        return compile_file_with_output(target, output_name);
    }
    
#ifndef _WIN32
    // Try as directory (not supported on Windows)
    char* main_file = find_main_file(target);
    if (main_file) {
        return compile_file_with_output(main_file, output_name);
    }
#endif
    
    fprintf(stderr, "Error: Could not find '%s' or main.wyn in directory\n", target);
    return 1;
}
