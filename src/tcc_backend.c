// tcc_backend.c — Embedded TCC compilation backend for Wyn
// Zero external dependencies. Full runtime. No feature loss.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include "windows_compat.h"
#else
#include <unistd.h>
#endif

int wyn_tcc_compile_to_exe(const char* c_source, const char* output_path,
                           const char* wyn_root, const char* include_path) {
    (void)include_path;
    
    char tcc_bin[512];
    snprintf(tcc_bin, sizeof(tcc_bin), "%s/vendor/tcc/bin/tcc", wyn_root);
    if (access(tcc_bin, X_OK) != 0) return -1;

    // Write C source to temp file
    char c_path[512];
    snprintf(c_path, sizeof(c_path), "%s.tcc.c", output_path);
    FILE* f = fopen(c_path, "w");
    if (!f) return -1;
    fputs(c_source, f);
    fclose(f);

    const char* extra_flags = (include_path && include_path[0]) ? include_path : "";
    
    // Detect bundled system headers/CRT (Lambda deployment)
    // TCC is configured with correct --sysincludepaths/--crtprefix at build time,
    // so no extra flags needed for system headers/CRT.

    // Use pre-compiled TCC runtime library for speed
    char rt_tcc[512];
    snprintf(rt_tcc, sizeof(rt_tcc), "%s/vendor/tcc/lib/libwyn_rt_tcc.a", wyn_root);

    char cmd[4096];
    if (access(rt_tcc, R_OK) == 0) {
        // Fast path: pre-compiled runtime + wrapper source
        // Check if project has sqlite package installed
        char sqlite_src[512];
        snprintf(sqlite_src, sizeof(sqlite_src), "./packages/sqlite/src/sqlite3.c");
        const char* sqlite_inc = "";
        const char* sqlite_file = "";
        char sqlite_inc_buf[256] = "";
        if (access(sqlite_src, R_OK) == 0) {
            snprintf(sqlite_inc_buf, sizeof(sqlite_inc_buf), "-I ./packages/sqlite/src -DWYN_USE_SQLITE");
            sqlite_inc = sqlite_inc_buf;
            sqlite_file = sqlite_src;
        }
        
        snprintf(cmd, sizeof(cmd),
            "%s -o %s -I %s/src -I %s/vendor/tcc/tcc_include -I %s/vendor/minicoro -L %s/vendor/tcc/lib -w -DMCO_NO_MULTITHREAD -DMCO_USE_UCONTEXT -D_XOPEN_SOURCE=600 %s %s "
            "%s %s/src/wyn_arena.c %s/src/stdlib_string.c %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s/src/coroutine.c %s/src/spawn_fast.c %s/src/io_loop.c %s/src/future.c %s %s -lpthread -lm 2>/tmp/wyn_tcc_err.txt",
            tcc_bin, output_path, wyn_root, wyn_root, wyn_root, wyn_root, extra_flags, sqlite_inc,
            c_path, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, wyn_root, rt_tcc, sqlite_file);
    } else {
        // Fallback: compile runtime from source using unified source list
        extern const char* wyn_runtime_sources[];
        extern void build_source_list(char* buf, int bufsize, const char* prefix);
        char src_list[4096];
        build_source_list(src_list, sizeof(src_list), wyn_root);
        snprintf(cmd, sizeof(cmd),
            "%s -o %s -I %s/src -I %s/vendor/tcc/tcc_include -I %s/vendor/minicoro -L %s/vendor/tcc/lib -w -D__TINYC__ -DMCO_NO_MULTITHREAD -DMCO_USE_UCONTEXT -D_XOPEN_SOURCE=600 "
            "%s %s -lpthread -lm 2>/tmp/wyn_tcc_err.txt",
            tcc_bin, output_path, wyn_root, wyn_root, wyn_root, wyn_root,
            c_path, src_list);
    }

    int result = system(cmd);
    unlink(c_path);
    
    if (result != 0 && getenv("WYN_DEBUG")) {
        FILE* err = fopen("/tmp/wyn_tcc_err.txt", "r");
        if (err) {
            char line[512];
            while (fgets(line, sizeof(line), err)) fprintf(stderr, "  tcc: %s", line);
            fclose(err);
        }
    }
    unlink("/tmp/wyn_tcc_err.txt");
    return result == 0 ? 0 : -1;
}

int wyn_tcc_available(void) { return 1; }
