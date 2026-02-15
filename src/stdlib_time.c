// Stdlib Time API Implementation
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

// Low-level time functions called by stdlib_runtime.c wrappers
long long wyn_time_now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec;
}

long long wyn_time_now_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

void wyn_time_sleep(int milliseconds) {
    usleep(milliseconds * 1000);
}
