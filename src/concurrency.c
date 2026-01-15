#include "concurrency.h"
#include <stdlib.h>
#include <unistd.h>

// Spawn a new thread
WynThread* wyn_spawn(void* (*fn)(void*), void* arg) {
    WynThread* thread = malloc(sizeof(WynThread));
    thread->fn = fn;
    thread->arg = arg;
    thread->result = 0;
    
    pthread_create(&thread->thread, NULL, fn, arg);
    
    return thread;
}

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
