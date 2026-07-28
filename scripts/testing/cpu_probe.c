// cpu_probe.c - portable "how much CPU did this child burn?" harness.
//
// Runs a command with a wall-clock limit, kills it, and reports the CPU time
// (user+sys) it actually consumed, in milliseconds, on stdout.
//
//   usage: cpu_probe <wall_ms> <cmd> [args...]
//   stdout: "<cpu_ms>\n"
//
// Why not `ps -o time=` or %CPU: %CPU is a DECAYING AVERAGE (it has produced
// wrong diagnoses on this codebase twice) and `ps -o time=` has 1-second
// granularity and non-portable formatting. This measures CUMULATIVE CPU:
// getrusage(RUSAGE_CHILDREN) on POSIX, GetProcessTimes on Windows.
//
// Used by scripts/integration_gates.sh idle-cpu gate: a program that is merely
// waiting (sleep / awaited spawn) must burn ~no CPU. Before the designated-poller
// change, the scheduler's park/spin sites burned 1-3 CPU-seconds per 2s of wall.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: cpu_probe <wall_ms> <cmd> [args...]\n"); return 2; }
    int wall_ms = atoi(argv[1]);

    // Rebuild a single command line for CreateProcess.
    char cmdline[8192]; cmdline[0] = '\0';
    for (int i = 2; i < argc; i++) {
        if (i > 2) strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, "\"", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, argv[i], sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, "\"", sizeof(cmdline) - strlen(cmdline) - 1);
    }

    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "cpu_probe: CreateProcess failed (%lu)\n", GetLastError());
        return 2;
    }
    WaitForSingleObject(pi.hProcess, (DWORD)wall_ms);
    FILETIME ct, et, kt, ut;
    long long cpu_ms = 0;
    if (GetProcessTimes(pi.hProcess, &ct, &et, &kt, &ut)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
        u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
        cpu_ms = (long long)((k.QuadPart + u.QuadPart) / 10000ULL);  // 100ns ticks
    }
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 2000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    printf("%lld\n", cpu_ms);
    return 0;
}

#else
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <errno.h>

static long long rusage_children_ms(void) {
    struct rusage ru;
    if (getrusage(RUSAGE_CHILDREN, &ru) != 0) return -1;
    return (long long)ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000 +
           (long long)ru.ru_stime.tv_sec * 1000 + ru.ru_stime.tv_usec / 1000;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: cpu_probe <wall_ms> <cmd> [args...]\n"); return 2; }
    int wall_ms = atoi(argv[1]);

    long long before = rusage_children_ms();

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 2; }
    if (pid == 0) {
        // Own process group so we can signal the whole tree.
        setpgid(0, 0);
        // Silence the child's stdout: this tool reports the measurement on stdout,
        // and a chatty program would otherwise be indistinguishable from it.
        // stderr is left alone so real failures stay visible.
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); close(devnull); }
        execvp(argv[2], &argv[2]);
        _exit(127);
    }
    setpgid(pid, pid);

    // Poll for the wall-clock budget, then kill. RUSAGE_CHILDREN only accounts
    // for REAPED children, so the wait AND the reap must both happen here.
    struct timespec slice = {0, 5 * 1000 * 1000};  // 5ms
    int elapsed = 0, done = 0, status = 0;
    while (elapsed < wall_ms) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) { done = 1; break; }
        nanosleep(&slice, NULL);
        elapsed += 5;
    }
    if (!done) {
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }

    long long after = rusage_children_ms();
    if (before < 0 || after < 0) { fprintf(stderr, "cpu_probe: getrusage failed\n"); return 2; }
    printf("%lld\n", after - before);
    return 0;
}
#endif
