#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

typedef struct {
    int total, passed, failed;
    double total_time;
} TestResults;

// Get path to the wyn executable itself
static char wyn_exe[1024] = "";
static void find_wyn_exe(void) {
    if (wyn_exe[0]) return;
#ifdef __APPLE__
    uint32_t sz = sizeof(wyn_exe);
    if (_NSGetExecutablePath(wyn_exe, &sz) != 0) strcpy(wyn_exe, "wyn");
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", wyn_exe, sizeof(wyn_exe) - 1);
    if (len > 0) wyn_exe[len] = '\0'; else strcpy(wyn_exe, "wyn");
#elif defined(_WIN32)
    GetModuleFileNameA(NULL, wyn_exe, sizeof(wyn_exe));
#else
    strcpy(wyn_exe, "wyn");
#endif
}

// Cross-platform process execution (no system() / shell)
static int run_process(const char* path, char* const argv[]) {
#ifdef _WIN32
    STARTUPINFO si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi;
    // Build command line from argv
    char cmd[4096] = "";
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        // Suppress stdout/stderr from build
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execv(path, argv);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

static int run_binary(const char* path) {
#ifdef _WIN32
    char* argv[] = {(char*)path, NULL};
    return run_process(path, argv);
#else
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        execl(path, path, NULL);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

// A test file is test_*.wyn or *_test.wyn
static int is_test_name(const char* name) {
    size_t len = strlen(name);
    if (len < 5 || strcmp(name + len - 4, ".wyn") != 0) return 0;
    if (strncmp(name, "test_", 5) == 0) return 1;
    if (len >= 9 && strncmp(name + len - 9, "_test.wyn", 9) == 0) return 1;
    return 0;
}

// Scan directory (one level of subdirectories too) for test files
static int collect_tests(const char* dir, char files[][512], int max) {
    int count = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (count >= max) break;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!is_test_name(fd.cFileName)) continue;
        snprintf(files[count], 512, "%s\\%s", dir, fd.cFileName);
        count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && count < max) {
        if (e->d_name[0] == '.') continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        // stat() instead of d_type: DT_DIR needs _DEFAULT_SOURCE on glibc and
        // d_type is unsupported on some filesystems anyway.
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            // one level deep: tests/unit/test_x.wyn
            DIR* sub = opendir(path);
            if (!sub) continue;
            struct dirent* se;
            while ((se = readdir(sub)) != NULL && count < max) {
                if (!is_test_name(se->d_name)) continue;
                snprintf(files[count], 512, "%s/%s", path, se->d_name);
                count++;
            }
            closedir(sub);
            continue;
        }
        if (!is_test_name(e->d_name)) continue;
        snprintf(files[count], 512, "%s", path);
        count++;
    }
    closedir(d);
#endif
    return count;
}

int cmd_test(const char* test_dir, int argc, char** argv) {
    if (!test_dir) test_dir = "tests";
    find_wyn_exe();

    // Optional name filter: `wyn test math` runs only files whose path
    // contains "math".
    const char* filter = NULL;
    for (int i = 0; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-') { filter = argv[i]; break; }
    }

    printf("\033[1m🧪 Wyn Test Runner\033[0m\n");
    printf("Scanning: %s/%s%s\n\n", test_dir,
           filter ? "  filter: " : "", filter ? filter : "");

    static char files[256][512];
    int count = collect_tests(test_dir, files, 256);
    if (count == 0) {
        fprintf(stderr, "No test files found in %s/\n", test_dir);
        fprintf(stderr, "  Tests are .wyn files named test_*.wyn or *_test.wyn.\n");
        fprintf(stderr, "  Example tests/test_math.wyn:\n\n");
        fprintf(stderr, "    test \"addition\" {\n");
        fprintf(stderr, "        assert_eq(2 + 3, 5)\n");
        fprintf(stderr, "    }\n");
        return 1;
    }

    TestResults r = {0};
    clock_t t0 = clock();

    for (int i = 0; i < count; i++) {
        if (filter && !strstr(files[i], filter)) continue;
        r.total++;
        clock_t ts = clock();

        // Compile using `wyn build <file>` (handles WYN_ROOT correctly)
        char bin[512];
        snprintf(bin, sizeof(bin), "%.*s", (int)(strlen(files[i]) - 4), files[i]);
        char* build_argv[] = {wyn_exe, "build", files[i], NULL};
        int compile_rc = run_process(wyn_exe, build_argv);

        if (compile_rc != 0) {
            r.failed++;
            printf("  \033[31m✗\033[0m %s (compile error)\n", files[i]);
            // Re-run `wyn check` WITHOUT output suppression so the user sees
            // the actual diagnostics - "(compile error)" alone was a dead end.
#ifndef _WIN32
            pid_t dp = fork();
            if (dp == 0) {
                execl(wyn_exe, wyn_exe, "check", files[i], (char*)NULL);
                _exit(127);
            }
            if (dp > 0) { int ds; waitpid(dp, &ds, 0); }
#endif
            continue;
        }

        // Run compiled binary
        int rc = run_binary(bin);
        double dt = (double)(clock() - ts) / CLOCKS_PER_SEC;

        if (rc == 0) {
            r.passed++;
            printf("  \033[32m✓\033[0m %s (%.1fs)\n", files[i], dt);
        } else {
            r.failed++;
            printf("  \033[31m✗\033[0m %s (exit %d, %.1fs)\n", files[i], rc, dt);
        }

        // Cleanup artifacts
        remove(bin);
        char csrc[512];
        snprintf(csrc, sizeof(csrc), "%s.c", files[i]);
        remove(csrc);
    }

    r.total_time = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("\n\033[1mResults:\033[0m %d passed, %d failed (%.1fs)\n",
           r.passed, r.failed, r.total_time);
    // Zero tests run is a FAILURE, not a pass: a typo'd filter in CI used to
    // print "All tests passed!" with exit 0 while running nothing.
    if (r.passed == 0 && r.failed == 0) {
        fprintf(stderr, "✗ No tests matched. Check the filter or add tests under tests/.\n");
        return 1;
    }
    if (r.failed == 0) printf("🎉 All tests passed!\n");

    return r.failed > 0 ? 1 : 0;
}
