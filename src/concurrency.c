// Concurrency primitives (threads, mutexes, etc.)
// Note: wyn_spawn is now in spawn.c
#include "concurrency.h"
#include <stdlib.h>
#include <unistd.h>

// Join a thread (wait for completion)
int wyn_join(WynThread* thread) {
    void* result;
    pthread_join(thread->thread, &result);
    thread->result = (int)(intptr_t)result;
    
    int ret = thread->result;
    free(thread);
    return ret;
}

// Detach a thread
void wyn_detach(WynThread* thread) {
    pthread_detach(thread->thread);
    free(thread);
}

// Get current thread ID
uint64_t wyn_thread_id(void) {
    return (uint64_t)pthread_self();
}
