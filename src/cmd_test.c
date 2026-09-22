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
    // FLUSH BEFORE FORKING, and this is not hygiene - it was a visible defect.
    //
    // fork() gives the child a COPY of this process's stdio buffers. The child below
    // then calls freopen() on stdout, and freopen CLOSES the stream first, which
    // FLUSHES that copied buffer to the inherited fd. So everything this process had
    // printed and not yet flushed - the "🧪 Wyn Test Runner / Scanning: tests/"
    // banner - was written a SECOND time, by the child, before the redirect took
    // effect. `wyn test` printed its banner twice and its per-file lines twice.
    //
    // It only happened when stdout was NOT a tty: on a terminal stdout is
    // line-buffered and the banner was already gone before the fork. Redirected - a
    // file, a pipe, every CI log, every agent capturing output - it is fully
    // buffered, so the duplication appeared exactly where nobody was watching
    // interactively. Guarded by tests/errors/run_test_summary_test.sh, which captures
    // to a file for that reason.
    fflush(NULL);
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

// Strip ANSI SGR sequences in place. The compiled test binary colours its own
// summary ("\033[32m%d tests passed\033[0m"), so the tally cannot be matched without
// removing them first.
static void strip_ansi(char* s)
{
    char* w = s;
    for (char* r = s; *r; ) {
        if (r[0] == '\033' && r[1] == '[') {
            r += 2;
            while (*r && !((*r >= '@' && *r <= '~'))) r++;
            if (*r) r++;
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

// Is this line the test binary's OWN summary? codegen_program.c emits exactly two
// shapes: "%d tests passed" when nothing failed, and "%d passed, %d failed" when
// something did. Returns 1 and fills the counts when it matches.
// Matched EXACTLY and anchored to end-of-line. The first version used
//     sscanf(plain, "%d tests passed", &p) == 1
// which is wrong in a way that looks right: sscanf's return counts ASSIGNED
// conversions, and %d is assigned BEFORE the literal " tests passed" is compared -
// so any line beginning with a number matched. Running `wyn test` over this repo's
// own tests/ reported "227705 tests passed", because the programs there print
// numbers. Anchoring also stops a program's own output from being swallowed as a
// summary, which matters: a swallowed line would vanish from the report entirely.
static int parse_test_tally(const char* plain, int* passed, int* failed)
{
    while (*plain == ' ' || *plain == '\t') plain++;
    if (*plain < '0' || *plain > '9') return 0;
    char* end = NULL;
    long a = strtol(plain, &end, 10);
    if (!end || end == plain) return 0;
    while (*end == ' ') end++;
    if (strcmp(end, "tests passed") == 0 || strcmp(end, "test passed") == 0) {
        *passed += (int)a; return 1;
    }
    if (strncmp(end, "passed, ", 8) == 0) {
        const char* q = end + 8;
        char* e2 = NULL;
        long b = strtol(q, &e2, 10);
        if (!e2 || e2 == q) return 0;
        while (*e2 == ' ') e2++;
        if (strcmp(e2, "failed") == 0) { *passed += (int)a; *failed += (int)b; return 1; }
    }
    return 0;
}

// Run the compiled test binary, streaming its output through unchanged EXCEPT for its
// own summary line, whose numbers are added to the caller's running totals instead.
//
// WHY CAPTURE AT ALL. The binary prints the per-test ✓/✗ lines and then its own
// summary; cmd_test then printed a SECOND summary counting FILES. So `wyn test`
// ended with two numbers measuring different things and said which neither:
//     4 tests passed                 <- test BLOCKS, from the binary
//     Results: 1 passed, 0 failed    <- FILES, from cmd_test
// On a 40-file suite those diverge wildly and a reader has no way to tell them
// apart. Reading the binary's tally here is what makes ONE truthful summary
// possible: the per-test lines still stream through, and the total is printed once
// at the end, in units of test blocks - which is what a user means by "a test".
//
// *tally_seen stays 0 when no summary line was found (a crash before the end, or the
// Windows path below). The caller then says it is counting FILES rather than quietly
// reporting a wrong number of tests.
static int run_binary_tally(const char* path, int* blocks_passed, int* blocks_failed,
                            int* tally_seen)
{
#ifdef _WIN32
    // No capture on Windows: this file deliberately avoids a shell, and a pipe here
    // would need the CreatePipe/handle-inheritance dance. The caller falls back to a
    // file-level summary and SAYS so, which is honest; it is not silently wrong.
    char* argv[] = {(char*)path, NULL};
    return run_process(path, argv);
#else
    int p[2];
    if (pipe(p) != 0) return 1;
    fflush(NULL);                       // see run_process()
    pid_t pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); return 1; }
    if (pid == 0) {
        close(p[0]);
        dup2(p[1], 1);
        dup2(p[1], 2);
        close(p[1]);
        execl(path, path, NULL);
        _exit(127);
    }
    close(p[1]);
    FILE* in = fdopen(p[0], "r");
    if (!in) { close(p[0]); }
    // A single blank line precedes the binary's summary (codegen emits printf("\n")).
    // Hold it back one line: if the next line IS the summary the blank goes with it,
    // otherwise it is passed through. Without this, swallowing the summary would
    // leave a stray blank line in the middle of the report.
    int pending_blank = 0;
    if (in) {
        char line[4096];
        while (fgets(line, sizeof(line), in)) {
            char plain[4096];
            snprintf(plain, sizeof(plain), "%s", line);
            strip_ansi(plain);
            char* nl = strpbrk(plain, "\r\n"); if (nl) *nl = '\0';
            int only_ws = 1;
            for (const char* q = plain; *q; q++) if (*q != ' ' && *q != '\t') { only_ws = 0; break; }
            if (only_ws) {
                if (pending_blank) fputs("\n", stdout);   // two blanks: keep the first
                pending_blank = 1;
                continue;
            }
            if (parse_test_tally(plain, blocks_passed, blocks_failed)) {
                *tally_seen = 1;
                pending_blank = 0;       // the blank belonged to the summary
                continue;
            }
            if (pending_blank) { fputs("\n", stdout); pending_blank = 0; }
            fputs(line, stdout);
        }
        fclose(in);
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
    // Block-level totals, read out of each test binary's own summary. These are what
    // the ONE summary reports: "4 tests passed" means four `test` blocks, which is
    // what a user means by a test. r.passed/r.failed stay file-level and are reported
    // as files, so the two can no longer be mistaken for each other.
    int blocks_passed = 0, blocks_failed = 0, tally_seen = 0;
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
            // Re-run so the user sees the actual diagnostics - "(compile error)"
            // alone was a dead end.
            //
            // Re-run `build`, not `check`. When the two DISAGREE - check passes
            // and build fails - re-running `check` prints "✓ no errors" and tells
            // you nothing, which is exactly the dead end this was meant to avoid.
            // That happened on the ubuntu CI runner with the api/web templates:
            // `wyn check` was clean and the C compilation was what failed, so the
            // log showed a green check next to a red test and no cause at all.
            // `build` reproduces the real failure, including the C compiler's own
            // errors, and it also covers the check-passes-then-codegen-fails class
            // this release has been fixing.
#ifndef _WIN32
            fflush(NULL);               // see run_process(): fork copies our buffers
            pid_t dp = fork();
            if (dp == 0) {
                execl(wyn_exe, wyn_exe, "build", files[i], (char*)NULL);
                _exit(127);
            }
            if (dp > 0) { int ds; waitpid(dp, &ds, 0); }
#endif
            continue;
        }

        // Run compiled binary, taking its per-test tally as we pass its output through
        int rc = run_binary_tally(bin, &blocks_passed, &blocks_failed, &tally_seen);
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

    // ─── ONE SUMMARY, AND IT SAYS WHAT IT COUNTS ─────────────────────────────
    //
    // There used to be two, from different places, in different units:
    //     4 tests passed                 <- the test binary: test BLOCKS
    //     Results: 1 passed, 0 failed    <- here: FILES
    // Nothing said which was which, and on a multi-file suite they diverge. The
    // binary's summary line is now swallowed by run_binary_tally() and its numbers
    // added up here, so there is exactly one summary and it is in blocks - the unit a
    // user means by "a test". The file count is still shown, labelled as files,
    // because it is the only way to see that a file failed to COMPILE (no blocks ran,
    // so a block count alone would hide it).
    int files_run = r.passed + r.failed;
    if (tally_seen) {
        // BOTH units, on one line, each labelled. A block count alone would hide a
        // file that failed to COMPILE (no blocks ran, so it contributes nothing to the
        // block tally), and a file count alone is the number that was being mistaken
        // for a test count in the first place.
        printf("\n\033[1mResults:\033[0m %d test%s passed, %d failed"
               "  (%d test file%s: %d passed, %d failed, %.1fs)\n",
               blocks_passed, blocks_passed == 1 ? "" : "s", blocks_failed,
               files_run, files_run == 1 ? "" : "s", r.passed, r.failed, r.total_time);
    } else {
        // No binary reported a tally. That is NOT an error: `wyn test` supports two
        // styles, and only one of them counts blocks.
        //   (a) `test "name" { ... }` blocks  -> the binary prints per-test lines and
        //       a tally, which the branch above aggregates.
        //   (b) `Test.init / Test.assert_* / Test.summary()` in a plain `fn main`
        //       -> no tally line at all; pass/fail arrives purely as the exit code
        //       (the wyn_test_exit_code hook). tests/stdlib/ and the fixtures in
        //       run_user_test_runner_test.sh are all style (b).
        // Windows also lands here, because the output is not captured there.
        //
        // So the units are named instead of guessed at. A block count of 0 would read
        // as "no tests exist", which is the same class of lie this change is removing.
        printf("\n\033[1mResults:\033[0m %d test file%s: %d passed, %d failed  (%.1fs)"
               "  \033[2m[no per-test counts: these files report via Test.summary()]\033[0m\n",
               files_run, files_run == 1 ? "" : "s", r.passed, r.failed, r.total_time);
    }

    // Zero tests run is a FAILURE, not a pass: a typo'd filter in CI used to
    // print "All tests passed!" with exit 0 while running nothing.
    if (files_run == 0) {
        fprintf(stderr, "✗ No tests matched. Check the filter or add tests under tests/.\n");
        return 1;
    }
    // NOT FIXED HERE, and deliberately so: a test file with ZERO assertions still
    // passes. A .wyn file named test_*.wyn whose `test` blocks were renamed away
    // compiles, exits 0, and reads as green. It cannot be caught from this side,
    // because style (b) above is INDISTINGUISHABLE from it on stdout and on the exit
    // code - a passing Test.summary() run and an empty program both print nothing a
    // runner can count and both exit 0. Refusing it here breaks every style-(b)
    // suite in the tree (measured: run_user_test_runner_test.sh's three fixtures).
    // The fix belongs in src/test_runtime.c, which knows how many assertions actually
    // ran; logged for that lane rather than guessed at here.

    // `+ blocks_failed` is DELIBERATELY REDUNDANT today and is called out rather than
    // left looking covered: a binary that reports failures also exits nonzero (the
    // wyn_test_exit_code hook in src/test_runtime.c), so r.failed already counts that
    // file. Mutating this term away changes no test. It is kept because the two facts
    // have different sources - one is the child's exit status, one is the child's own
    // report - and a regression in the exit-code hook would otherwise turn a suite
    // with visible ✗ lines into exit 0.
    int failed = r.failed + blocks_failed;
    if (failed == 0) printf("🎉 All tests passed!\n");

    return failed > 0 ? 1 : 0;
}
