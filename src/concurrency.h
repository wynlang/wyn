#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <pthread.h>
#include <stdint.h>

// Thread handle
typedef struct {
    pthread_t thread;
    void* (*fn)(void*);
    void* arg;
    int result;
} WynThread;

// Spawn a new thread
WynThread* wyn_spawn(void* (*fn)(void*), void* arg);

// Join a thread (wait for completion)
int wyn_join(WynThread* thread);

// Detach a thread
void wyn_detach(WynThread* thread);

// Get current thread ID
uint64_t wyn_thread_id(void);

#endif
