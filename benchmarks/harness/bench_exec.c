// fork/exec/wait4 timing driver.
//
// Times a child process N times and reports the median, min and max wall clock
// plus the child's own user+sys CPU. A shell loop cannot do this honestly: it
// folds its own fork and the `time` builtin's resolution into every sample, and
// on Apple Silicon a shell-spawned child is also more likely to be placed on an
// efficiency core.
//
// Usage: bench_exec <runs> <discard> -- cmd [args...]
//   runs    how many timed samples to take
//   discard how many leading runs to throw away (macOS first-exec malware scan
//           adds 1-3s to ANY new binary, so this must be >= 1 for a fresh one)
//
// Output is one line of JSON so the orchestrator never has to parse prose.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <runs> <discard> -- cmd [args...]\n", argv[0]);
        return 2;
    }
    int runs = atoi(argv[1]);
    int discard = atoi(argv[2]);
    if (strcmp(argv[3], "--") != 0) { fprintf(stderr, "expected --\n"); return 2; }
    char **cmd = &argv[4];

    int total = runs + discard;
    double *wall = calloc(total, sizeof(double));
    double *cpu  = calloc(total, sizeof(double));
    long maxrss = 0;
    int bad_exit = 0;

    for (int i = 0; i < total; i++) {
        struct timeval t0, t1;
        gettimeofday(&t0, NULL);
        pid_t pid = fork();
        if (pid == 0) {
            // Silence the child: printing to a pipe/tty is real work we are not
            // trying to measure, and it varies with terminal state.
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            execvp(cmd[0], cmd);
            _exit(127);
        }
        int status = 0;
        struct rusage ru;
        memset(&ru, 0, sizeof ru);
        wait4(pid, &status, 0, &ru);
        gettimeofday(&t1, NULL);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) bad_exit++;
        wall[i] = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_usec - t0.tv_usec) / 1000.0;
        cpu[i]  = (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000.0
                + (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000.0;
        if (ru.ru_maxrss > maxrss) maxrss = ru.ru_maxrss;
    }

    // Report only the kept samples; the discarded ones exist to absorb
    // first-exec scanning and cold caches, not to be averaged in.
    double *kw = wall + discard, *kc = cpu + discard;
    double *sw = malloc(runs * sizeof(double));
    memcpy(sw, kw, runs * sizeof(double));
    qsort(sw, runs, sizeof(double), cmp_double);
    double *sc = malloc(runs * sizeof(double));
    memcpy(sc, kc, runs * sizeof(double));
    qsort(sc, runs, sizeof(double), cmp_double);

    printf("{\"runs\":%d,\"median_ms\":%.2f,\"min_ms\":%.2f,\"max_ms\":%.2f,"
           "\"cpu_median_ms\":%.2f,\"maxrss_kb\":%ld,\"nonzero_exits\":%d}\n",
           runs, sw[runs / 2], sw[0], sw[runs - 1], sc[runs / 2],
           maxrss / 1024, bad_exit);
    return bad_exit ? 1 : 0;
}
