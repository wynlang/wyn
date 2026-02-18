// tcc_backend.c — Embedded TCC compilation backend for Wyn
// Zero external dependencies. Full runtime. No feature loss.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    
    // Use pre-compiled TCC runtime library for speed
    char rt_tcc[512];
    snprintf(rt_tcc, sizeof(rt_tcc), "%s/vendor/tcc/lib/libwyn_rt_tcc.a", wyn_root);

    char cmd[4096];
    if (access(rt_tcc, R_OK) == 0) {
        // Fast path: pre-compiled runtime + wrapper source (wrapper defines main())
        snprintf(cmd, sizeof(cmd),
            "%s -o %s -I %s/src -I %s/vendor/tcc/tcc_include -I %s/vendor/sqlite -w %s "
            "%s %s/src/wyn_wrapper.c %s/src/wyn_interface.c %s -lpthread -lm 2>/tmp/wyn_tcc_err.txt",
            tcc_bin, output_path, wyn_root, wyn_root, wyn_root, extra_flags, c_path, wyn_root, wyn_root, rt_tcc);
    } else {
        // Fallback: compile runtime from source
        snprintf(cmd, sizeof(cmd),
            "%s -o %s -I %s/src -I %s/vendor/tcc/tcc_include -w -D__TINYC__ "
            "%s %s/src/wyn_wrapper.c %s/src/wyn_interface.c "
            "%s/src/hashmap.c %s/src/hashset.c %s/src/json.c "
            "%s/src/test_runtime.c %s/src/spawn.c %s/src/spawn_fast.c "
            "%s/src/future.c %s/src/net.c %s/src/net_runtime.c "
            "%s/src/io.c %s/src/optional.c %s/src/result.c "
            "%s/src/arc_runtime.c %s/src/safe_memory.c %s/src/error.c "
            "%s/src/string_runtime.c %s/src/concurrency.c "
            "%s/src/stdlib_runtime.c %s/src/stdlib_string.c "
            "%s/src/stdlib_array.c %s/src/stdlib_time.c "
            "%s/src/stdlib_math.c %s/src/stdlib_crypto.c "
            "%s/src/stdlib_enhanced.c %s/src/file_io_simple.c "
            "%s/src/net_advanced.c %s/src/hashmap_runtime.c "
            "-lpthread -lm 2>/tmp/wyn_tcc_err.txt",
            tcc_bin, output_path, wyn_root, wyn_root,
            c_path, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root, wyn_root,
            wyn_root, wyn_root,
            wyn_root, wyn_root,
            wyn_root, wyn_root,
            wyn_root, wyn_root,
            wyn_root, wyn_root,
            wyn_root, wyn_root);
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
