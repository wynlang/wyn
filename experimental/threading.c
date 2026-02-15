#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#else
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
#define sched_yield() SwitchToThread()
#endif
#include <errno.h>

// Thread management
WynThread* wyn_thread_spawn(WynThreadFunc func, void* arg) {
    if (!func) return NULL;
    
    WynThread* thread = malloc(sizeof(WynThread));
    if (!thread) return NULL;
    
    thread->func = func;
    thread->arg = arg;
    thread->result = NULL;
    thread->status = WYN_THREAD_RUNNING;
    thread->detached = false;
    
    int result = pthread_create(&thread->handle, NULL, func, arg);
    if (result != 0) {
        free(thread);
        return NULL;
    }
    
    return thread;
}

WynThread* wyn_thread_spawn_detached(WynThreadFunc func, void* arg) {
    WynThread* thread = wyn_thread_spawn(func, arg);
    if (!thread) return NULL;
    
    if (wyn_thread_detach(thread)) {
        return thread;
    } else {
        wyn_thread_free(thread);
        return NULL;
    }
}

void* wyn_thread_join(WynThread* thread) {
    if (!thread || thread->detached || thread->status == WYN_THREAD_JOINED) {
        return NULL;
    }
    
    void* result;
    int join_result = pthread_join(thread->handle, &result);
    
    if (join_result == 0) {
        thread->result = result;
        thread->status = WYN_THREAD_JOINED;
        return result;
    } else {
        thread->status = WYN_THREAD_ERROR;
        return NULL;
    }
}

bool wyn_thread_detach(WynThread* thread) {
    if (!thread || thread->detached) return false;
    
    int result = pthread_detach(thread->handle);
    if (result == 0) {
        thread->detached = true;
        return true;
    }
    return false;
}

void wyn_thread_free(WynThread* thread) {
    if (!thread) return;
    
    // If thread is still running and not detached, try to join it
    if (thread->status == WYN_THREAD_RUNNING && !thread->detached) {
        wyn_thread_join(thread);
    }
    
    free(thread);
}

WynThreadStatus wyn_thread_status(const WynThread* thread) {
    return thread ? thread->status : WYN_THREAD_ERROR;
}

void wyn_thread_yield(void) {
    sched_yield();
}

void wyn_thread_sleep_ms(uint32_t milliseconds) {
    usleep(milliseconds * 1000);
}

// Mutex operations
WynMutex* wyn_mutex_new(void) {
    WynMutex* mutex = malloc(sizeof(WynMutex));
    if (!mutex) return NULL;
    
    int result = pthread_mutex_init(&mutex->mutex, NULL);
    if (result != 0) {
        free(mutex);
        return NULL;
    }
    
    mutex->initialized = true;
    return mutex;
}

void wyn_mutex_free(WynMutex* mutex) {
    if (!mutex) return;
    
    if (mutex->initialized) {
        pthread_mutex_destroy(&mutex->mutex);
    }
    free(mutex);
}

WynMutexGuard* wyn_mutex_lock(WynMutex* mutex) {
    if (!mutex || !mutex->initialized) return NULL;
    
    int result = pthread_mutex_lock(&mutex->mutex);
    if (result != 0) return NULL;
    
    WynMutexGuard* guard = malloc(sizeof(WynMutexGuard));
    if (!guard) {
        pthread_mutex_unlock(&mutex->mutex);
        return NULL;
    }
    
    guard->mutex = mutex;
    guard->locked = true;
    return guard;
}

bool wyn_mutex_try_lock(WynMutex* mutex, WynMutexGuard** guard) {
    if (!mutex || !mutex->initialized || !guard) return false;
    
    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result != 0) {
        *guard = NULL;
        return false;
    }
    
    *guard = malloc(sizeof(WynMutexGuard));
    if (!*guard) {
        pthread_mutex_unlock(&mutex->mutex);
        return false;
    }
    
    (*guard)->mutex = mutex;
    (*guard)->locked = true;
    return true;
}

void wyn_mutex_guard_unlock(WynMutexGuard* guard) {
    if (!guard || !guard->locked) return;
    
    pthread_mutex_unlock(&guard->mutex->mutex);
    guard->locked = false;
    free(guard);
}

// RwLock operations
WynRwLock* wyn_rwlock_new(void) {
    WynRwLock* rwlock = malloc(sizeof(WynRwLock));
    if (!rwlock) return NULL;
    
    int result = pthread_rwlock_init(&rwlock->rwlock, NULL);
    if (result != 0) {
        free(rwlock);
        return NULL;
    }
    
    rwlock->initialized = true;
    return rwlock;
}

void wyn_rwlock_free(WynRwLock* rwlock) {
    if (!rwlock) return;
    
    if (rwlock->initialized) {
        pthread_rwlock_destroy(&rwlock->rwlock);
    }
    free(rwlock);
}

WynRwLockReadGuard* wyn_rwlock_read(WynRwLock* rwlock) {
    if (!rwlock || !rwlock->initialized) return NULL;
    
    int result = pthread_rwlock_rdlock(&rwlock->rwlock);
    if (result != 0) return NULL;
    
    WynRwLockReadGuard* guard = malloc(sizeof(WynRwLockReadGuard));
    if (!guard) {
        pthread_rwlock_unlock(&rwlock->rwlock);
        return NULL;
    }
    
    guard->rwlock = rwlock;
    guard->locked = true;
    return guard;
}

WynRwLockWriteGuard* wyn_rwlock_write(WynRwLock* rwlock) {
    if (!rwlock || !rwlock->initialized) return NULL;
    
    int result = pthread_rwlock_wrlock(&rwlock->rwlock);
    if (result != 0) return NULL;
    
    WynRwLockWriteGuard* guard = malloc(sizeof(WynRwLockWriteGuard));
    if (!guard) {
        pthread_rwlock_unlock(&rwlock->rwlock);
        return NULL;
    }
    
    guard->rwlock = rwlock;
    guard->locked = true;
    return guard;
}

bool wyn_rwlock_try_read(WynRwLock* rwlock, WynRwLockReadGuard** guard) {
    if (!rwlock || !rwlock->initialized || !guard) return false;
    
    int result = pthread_rwlock_tryrdlock(&rwlock->rwlock);
    if (result != 0) {
        *guard = NULL;
        return false;
    }
    
    *guard = malloc(sizeof(WynRwLockReadGuard));
    if (!*guard) {
        pthread_rwlock_unlock(&rwlock->rwlock);
        return false;
    }
    
    (*guard)->rwlock = rwlock;
    (*guard)->locked = true;
    return true;
}

bool wyn_rwlock_try_write(WynRwLock* rwlock, WynRwLockWriteGuard** guard) {
    if (!rwlock || !rwlock->initialized || !guard) return false;
    
    int result = pthread_rwlock_trywrlock(&rwlock->rwlock);
    if (result != 0) {
        *guard = NULL;
        return false;
    }
    
    *guard = malloc(sizeof(WynRwLockWriteGuard));
    if (!*guard) {
        pthread_rwlock_unlock(&rwlock->rwlock);
        return false;
    }
    
    (*guard)->rwlock = rwlock;
    (*guard)->locked = true;
    return true;
}

void wyn_rwlock_read_guard_unlock(WynRwLockReadGuard* guard) {
    if (!guard || !guard->locked) return;
    
    pthread_rwlock_unlock(&guard->rwlock->rwlock);
    guard->locked = false;
    free(guard);
}

void wyn_rwlock_write_guard_unlock(WynRwLockWriteGuard* guard) {
    if (!guard || !guard->locked) return;
    
    pthread_rwlock_unlock(&guard->rwlock->rwlock);
    guard->locked = false;
    free(guard);
}

// Channel operations
WynChannel* wyn_channel_new(size_t capacity) {
    WynChannel* channel = malloc(sizeof(WynChannel));
    if (!channel) return NULL;
    
    channel->head = NULL;
    channel->tail = NULL;
    channel->count = 0;
    channel->capacity = capacity == 0 ? SIZE_MAX : capacity;
    channel->closed = false;
    
    // Initialize mutex and condition variables
    WynMutex* mutex = wyn_mutex_new();
    if (!mutex) {
        free(channel);
        return NULL;
    }
    channel->mutex = *mutex;
    free(mutex); // We copied the contents
    
    int result1 = pthread_cond_init(&channel->not_empty, NULL);
    int result2 = pthread_cond_init(&channel->not_full, NULL);
    
    if (result1 != 0 || result2 != 0) {
        pthread_mutex_destroy(&channel->mutex.mutex);
        free(channel);
        return NULL;
    }
    
    return channel;
}

void wyn_channel_free(WynChannel* channel) {
    if (!channel) return;
    
    // Close channel and wake up waiting threads
    wyn_channel_close(channel);
    
    // Free remaining nodes
    WynChannelNode* current = channel->head;
    while (current) {
        WynChannelNode* next = current->next;
        free(current);
        current = next;
    }
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&channel->not_empty);
    pthread_cond_destroy(&channel->not_full);
    pthread_mutex_destroy(&channel->mutex.mutex);
    
    free(channel);
}

bool wyn_channel_send(WynChannel* channel, void* data) {
    if (!channel) return false;
    
    pthread_mutex_lock(&channel->mutex.mutex);
    
    // Check if channel is closed
    if (channel->closed) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    // Wait for space if channel is full
    while (channel->count >= channel->capacity && !channel->closed) {
        pthread_cond_wait(&channel->not_full, &channel->mutex.mutex);
    }
    
    // Check again if channel was closed while waiting
    if (channel->closed) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    // Create new node
    WynChannelNode* node = malloc(sizeof(WynChannelNode));
    if (!node) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    node->data = data;
    node->next = NULL;
    
    // Add to queue
    if (channel->tail) {
        channel->tail->next = node;
    } else {
        channel->head = node;
    }
    channel->tail = node;
    channel->count++;
    
    // Signal waiting receivers
    pthread_cond_signal(&channel->not_empty);
    pthread_mutex_unlock(&channel->mutex.mutex);
    
    return true;
}

bool wyn_channel_try_send(WynChannel* channel, void* data) {
    if (!channel) return false;
    
    pthread_mutex_lock(&channel->mutex.mutex);
    
    // Check if channel is closed or full
    if (channel->closed || channel->count >= channel->capacity) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    // Create new node
    WynChannelNode* node = malloc(sizeof(WynChannelNode));
    if (!node) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    node->data = data;
    node->next = NULL;
    
    // Add to queue
    if (channel->tail) {
        channel->tail->next = node;
    } else {
        channel->head = node;
    }
    channel->tail = node;
    channel->count++;
    
    // Signal waiting receivers
    pthread_cond_signal(&channel->not_empty);
    pthread_mutex_unlock(&channel->mutex.mutex);
    
    return true;
}

bool wyn_channel_recv(WynChannel* channel, void** data) {
    if (!channel || !data) return false;
    
    pthread_mutex_lock(&channel->mutex.mutex);
    
    // Wait for data or channel closure
    while (channel->count == 0 && !channel->closed) {
        pthread_cond_wait(&channel->not_empty, &channel->mutex.mutex);
    }
    
    // Check if channel is empty and closed
    if (channel->count == 0 && channel->closed) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    // Get data from queue
    WynChannelNode* node = channel->head;
    *data = node->data;
    
    channel->head = node->next;
    if (!channel->head) {
        channel->tail = NULL;
    }
    channel->count--;
    
    free(node);
    
    // Signal waiting senders
    pthread_cond_signal(&channel->not_full);
    pthread_mutex_unlock(&channel->mutex.mutex);
    
    return true;
}

bool wyn_channel_try_recv(WynChannel* channel, void** data) {
    if (!channel || !data) return false;
    
    pthread_mutex_lock(&channel->mutex.mutex);
    
    // Check if channel is empty
    if (channel->count == 0) {
        pthread_mutex_unlock(&channel->mutex.mutex);
        return false;
    }
    
    // Get data from queue
    WynChannelNode* node = channel->head;
    *data = node->data;
    
    channel->head = node->next;
    if (!channel->head) {
        channel->tail = NULL;
    }
    channel->count--;
    
    free(node);
    
    // Signal waiting senders
    pthread_cond_signal(&channel->not_full);
    pthread_mutex_unlock(&channel->mutex.mutex);
    
    return true;
}

void wyn_channel_close(WynChannel* channel) {
    if (!channel) return;
    
    pthread_mutex_lock(&channel->mutex.mutex);
    channel->closed = true;
    
    // Wake up all waiting threads
    pthread_cond_broadcast(&channel->not_empty);
    pthread_cond_broadcast(&channel->not_full);
    
    pthread_mutex_unlock(&channel->mutex.mutex);
}

bool wyn_channel_is_closed(const WynChannel* channel) {
    return channel ? channel->closed : true;
}

size_t wyn_channel_len(const WynChannel* channel) {
    if (!channel) return 0;
    
    pthread_mutex_lock((pthread_mutex_t*)&channel->mutex.mutex);
    size_t len = channel->count;
    pthread_mutex_unlock((pthread_mutex_t*)&channel->mutex.mutex);
    
    return len;
}

size_t wyn_channel_capacity(const WynChannel* channel) {
    return channel ? channel->capacity : 0;
}

// Atomic operations
WynAtomic* wyn_atomic_new(int64_t initial_value) {
    WynAtomic* atomic = malloc(sizeof(WynAtomic));
    if (!atomic) return NULL;
    
    atomic->value = initial_value;
    return atomic;
}

void wyn_atomic_free(WynAtomic* atomic) {
    free(atomic);
}

int64_t wyn_atomic_load(const WynAtomic* atomic) {
    if (!atomic) return 0;
    return __atomic_load_n(&atomic->value, __ATOMIC_SEQ_CST);
}

void wyn_atomic_store(WynAtomic* atomic, int64_t value) {
    if (!atomic) return;
    __atomic_store_n((int64_t*)&atomic->value, value, __ATOMIC_SEQ_CST);
}

int64_t wyn_atomic_fetch_add(WynAtomic* atomic, int64_t value) {
    if (!atomic) return 0;
    return __atomic_fetch_add((int64_t*)&atomic->value, value, __ATOMIC_SEQ_CST);
}

int64_t wyn_atomic_fetch_sub(WynAtomic* atomic, int64_t value) {
    if (!atomic) return 0;
    return __atomic_fetch_sub((int64_t*)&atomic->value, value, __ATOMIC_SEQ_CST);
}

bool wyn_atomic_compare_exchange(WynAtomic* atomic, int64_t* expected, int64_t desired) {
    if (!atomic || !expected) return false;
    return __atomic_compare_exchange_n((int64_t*)&atomic->value, expected, desired, 
                                      false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

int64_t wyn_atomic_exchange(WynAtomic* atomic, int64_t value) {
    if (!atomic) return 0;
    return __atomic_exchange_n((int64_t*)&atomic->value, value, __ATOMIC_SEQ_CST);
}

// Thread-safe collections
WynThreadSafeVec* wyn_threadsafe_vec_new(void) {
    WynThreadSafeVec* vec = malloc(sizeof(WynThreadSafeVec));
    if (!vec) return NULL;
    
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
    
    WynMutex* mutex = wyn_mutex_new();
    if (!mutex) {
        free(vec);
        return NULL;
    }
    vec->mutex = *mutex;
    free(mutex);
    
    return vec;
}

void wyn_threadsafe_vec_free(WynThreadSafeVec* vec) {
    if (!vec) return;
    
    free(vec->data);
    pthread_mutex_destroy(&vec->mutex.mutex);
    free(vec);
}

bool wyn_threadsafe_vec_push(WynThreadSafeVec* vec, void* item) {
    if (!vec) return false;
    
    pthread_mutex_lock(&vec->mutex.mutex);
    
    // Grow if needed
    if (vec->len >= vec->cap) {
        size_t new_cap = vec->cap == 0 ? 4 : vec->cap * 2;
        void** new_data = realloc(vec->data, new_cap * sizeof(void*));
        if (!new_data) {
            pthread_mutex_unlock(&vec->mutex.mutex);
            return false;
        }
        vec->data = new_data;
        vec->cap = new_cap;
    }
    
    vec->data[vec->len] = item;
    vec->len++;
    
    pthread_mutex_unlock(&vec->mutex.mutex);
    return true;
}

bool wyn_threadsafe_vec_pop(WynThreadSafeVec* vec, void** item) {
    if (!vec || !item) return false;
    
    pthread_mutex_lock(&vec->mutex.mutex);
    
    if (vec->len == 0) {
        pthread_mutex_unlock(&vec->mutex.mutex);
        return false;
    }
    
    vec->len--;
    *item = vec->data[vec->len];
    
    pthread_mutex_unlock(&vec->mutex.mutex);
    return true;
}

bool wyn_threadsafe_vec_get(WynThreadSafeVec* vec, size_t index, void** item) {
    if (!vec || !item) return false;
    
    pthread_mutex_lock(&vec->mutex.mutex);
    
    if (index >= vec->len) {
        pthread_mutex_unlock(&vec->mutex.mutex);
        return false;
    }
    
    *item = vec->data[index];
    
    pthread_mutex_unlock(&vec->mutex.mutex);
    return true;
}

size_t wyn_threadsafe_vec_len(WynThreadSafeVec* vec) {
    if (!vec) return 0;
    
    pthread_mutex_lock(&vec->mutex.mutex);
    size_t len = vec->len;
    pthread_mutex_unlock(&vec->mutex.mutex);
    
    return len;
}

// Utility functions
size_t wyn_thread_hardware_concurrency(void) {
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return nprocs > 0 ? (size_t)nprocs : 1;
}

uint64_t wyn_thread_current_id(void) {
    return (uint64_t)pthread_self();
}
