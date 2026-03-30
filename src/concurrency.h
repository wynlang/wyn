#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef struct {
    HANDLE thread;
    void* (*fn)(void*);
    void* arg;
    int result;
} WynThread;
#else
#include <pthread.h>
typedef struct {
    pthread_t thread;
    void* (*fn)(void*);
    void* arg;
    int result;
} WynThread;
#endif

WynThread* wyn_spawn(void* (*fn)(void*), void* arg);
int wyn_join(WynThread* thread);
void wyn_detach(WynThread* thread);
uint64_t wyn_thread_id(void);

#endif
