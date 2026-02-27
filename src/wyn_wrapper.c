// Wrapper program to initialize arguments for Wyn-compiled programs
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "wyn_interface.h"

// External main function from the Wyn-compiled program
extern int wyn_main(void);

// Global argc/argv for System::args()
extern int __wyn_argc;
extern char** __wyn_argv;

// Spawn origin query (defined in spawn_fast.c)
extern const char* wyn_spawn_origin_file(void);
extern int wyn_spawn_origin_line(void);
extern long wyn_spawn_origin_id(void);

static char wyn_sigstack_buf[SIGSTKSZ];

static void wyn_crash_handler(int sig) {
    const char* spawn_file = wyn_spawn_origin_file();
    int spawn_line = wyn_spawn_origin_line();
    long spawn_id = wyn_spawn_origin_id();
    
    if (sig == SIGSEGV || sig == SIGBUS) {
        const char msg[] = "\n\033[31m✗ Segmentation fault\033[0m\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        if (spawn_file && spawn_line > 0) {
            char buf[512];
            int n = snprintf(buf, sizeof(buf),
                "  Inside spawn #%ld, created at %s:%d\n"
                "  Likely cause: stack overflow or null pointer in coroutine.\n"
                "  Try: set WYN_CORO_STACK=262144 for larger stacks.\n",
                spawn_id, spawn_file, spawn_line);
            write(STDERR_FILENO, buf, n);
        } else {
            const char hint[] = "  Likely causes: stack overflow or null pointer.\n";
            write(STDERR_FILENO, hint, sizeof(hint) - 1);
        }
    } else if (sig == SIGFPE) {
        const char msg[] = "\npanic: arithmetic error (division by zero?)\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        if (spawn_file && spawn_line > 0) {
            char buf[256];
            int n = snprintf(buf, sizeof(buf), "  Inside spawn #%ld, created at %s:%d\n", spawn_id, spawn_file, spawn_line);
            write(STDERR_FILENO, buf, n);
        }
    } else if (sig == SIGABRT) {
        const char msg[] = "\npanic: abort\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
    } else {
        const char msg[] = "\npanic: unexpected signal\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
    _exit(128 + sig);
}

__attribute__((flatten))
int main(int argc, char** argv) {
    // Alternate signal stack so handler works even on stack overflow
    stack_t ss;
    ss.ss_sp = wyn_sigstack_buf;
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    sa.sa_handler = wyn_crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    
    // Initialize arguments for Wyn interface
    wyn_init_args(argc, argv);
    
    // Set global argc/argv for System::args()
    __wyn_argc = argc;
    __wyn_argv = argv;
    
    // Call the Wyn-compiled main function
    return wyn_main();
}
