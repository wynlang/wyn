#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

extern int cmd_compile(const char* target, int argc, char** argv);

typedef struct {
    int total, passed, failed;
    double total_time;
} TestResults;

// Cross-platform process execution (no system() / shell)
static int run_binary(const char* path) {
#ifdef _WIN32
    STARTUPINFO si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s", path);
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
        execl(path, path, NULL);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

// Scan directory for test files
static int collect_tests(const char* dir, char files[][512], int max) {
    int count = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\test_*.wyn", dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (count < max) {
            snprintf(files[count], 512, "%s\\%s", dir, fd.cFileName);
            count++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && count < max) {
        if (strncmp(e->d_name, "test_", 5) != 0) continue;
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 4, ".wyn") != 0) continue;
        snprintf(files[count], 512, "%s/%s", dir, e->d_name);
        count++;
    }
    closedir(d);
#endif
    return count;
}

int cmd_test(const char* test_dir, int argc, char** argv) {
    (void)argc; (void)argv;
    if (!test_dir) test_dir = "tests";

    printf("\033[1m🧪 Wyn Test Runner\033[0m\n");
    printf("Scanning: %s/\n\n", test_dir);

    static char files[256][512];
    int count = collect_tests(test_dir, files, 256);
    if (count == 0) {
        fprintf(stderr, "No test files found in %s/\n", test_dir);
        return 1;
    }

    TestResults r = {0};
    clock_t t0 = clock();

    for (int i = 0; i < count; i++) {
        r.total++;
        clock_t ts = clock();

        // Compile
        if (cmd_compile(files[i], 0, NULL) != 0) {
            r.failed++;
            printf("  \033[31m✗\033[0m %s (compile error)\n", files[i]);
            continue;
        }

        // Run compiled binary
        char bin[512];
        snprintf(bin, sizeof(bin), "%s.out", files[i]);
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
    if (r.failed == 0) printf("🎉 All tests passed!\n");

    return r.failed > 0 ? 1 : 0;
}
