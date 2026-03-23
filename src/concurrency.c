// Concurrency primitives (threads, mutexes, etc.)
#include "concurrency.h"
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32

int wyn_join(WynThread* thread) {
    WaitForSingleObject(thread->thread, INFINITE);
    int ret = thread->result;
    CloseHandle(thread->thread);
    free(thread);
    return ret;
}

void wyn_detach(WynThread* thread) {
    CloseHandle(thread->thread);
    free(thread);
}

uint64_t wyn_thread_id(void) {
    return (uint64_t)GetCurrentThreadId();
}

#else

#include <unistd.h>

int wyn_join(WynThread* thread) {
    void* result;
    pthread_join(thread->thread, &result);
    thread->result = (int)(intptr_t)result;
    int ret = thread->result;
    free(thread);
    return ret;
}

void wyn_detach(WynThread* thread) {
    pthread_detach(thread->thread);
    free(thread);
}

uint64_t wyn_thread_id(void) {
    return (uint64_t)pthread_self();
}

#endif
