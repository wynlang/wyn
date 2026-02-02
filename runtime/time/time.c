// Time Module Implementation
#include "../wyn_runtime.h"
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

int64_t wyn_time_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void wyn_time_sleep(int64_t ms) {
    usleep(ms * 1000);
}
