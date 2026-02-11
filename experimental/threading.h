#ifndef WYN_THREADING_H
#define WYN_THREADING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

// Forward declarations
typedef struct WynThread WynThread;
typedef struct WynMutex WynMutex;
typedef struct WynMutexGuard WynMutexGuard;
typedef struct WynRwLock WynRwLock;
typedef struct WynRwLockReadGuard WynRwLockReadGuard;
typedef struct WynRwLockWriteGuard WynRwLockWriteGuard;
typedef struct WynChannel WynChannel;
typedef struct WynAtomic WynAtomic;

// Thread function type
typedef void* (*WynThreadFunc)(void* arg);

// Thread result status
typedef enum {
    WYN_THREAD_RUNNING,
    WYN_THREAD_FINISHED,
    WYN_THREAD_JOINED,
    WYN_THREAD_ERROR
} WynThreadStatus;

// Thread structure
typedef struct WynThread {
    pthread_t handle;
    WynThreadFunc func;
    void* arg;
    void* result;
    WynThreadStatus status;
    bool detached;
} WynThread;

// Mutex structure
typedef struct WynMutex {
    pthread_mutex_t mutex;
    bool initialized;
} WynMutex;

// Mutex guard for RAII-style locking
typedef struct WynMutexGuard {
    WynMutex* mutex;
    bool locked;
} WynMutexGuard;

// Read-Write lock structure
typedef struct WynRwLock {
    pthread_rwlock_t rwlock;
    bool initialized;
} WynRwLock;

// Read guard for RwLock
typedef struct WynRwLockReadGuard {
    WynRwLock* rwlock;
    bool locked;
} WynRwLockReadGuard;

// Write guard for RwLock
typedef struct WynRwLockWriteGuard {
    WynRwLock* rwlock;
    bool locked;
} WynRwLockWriteGuard;

// Channel for message passing
typedef struct WynChannelNode {
    void* data;
    struct WynChannelNode* next;
} WynChannelNode;

typedef struct WynChannel {
    WynChannelNode* head;
    WynChannelNode* tail;
    size_t count;
    size_t capacity;
    WynMutex mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool closed;
} WynChannel;

// Atomic operations
typedef struct WynAtomic {
    volatile int64_t value;
} WynAtomic;

// Thread management
WynThread* wyn_thread_spawn(WynThreadFunc func, void* arg);
WynThread* wyn_thread_spawn_detached(WynThreadFunc func, void* arg);
void* wyn_thread_join(WynThread* thread);
bool wyn_thread_detach(WynThread* thread);
void wyn_thread_free(WynThread* thread);
WynThreadStatus wyn_thread_status(const WynThread* thread);
void wyn_thread_yield(void);
void wyn_thread_sleep_ms(uint32_t milliseconds);

// Mutex operations
WynMutex* wyn_mutex_new(void);
void wyn_mutex_free(WynMutex* mutex);
WynMutexGuard* wyn_mutex_lock(WynMutex* mutex);
bool wyn_mutex_try_lock(WynMutex* mutex, WynMutexGuard** guard);
void wyn_mutex_guard_unlock(WynMutexGuard* guard);

// RwLock operations
WynRwLock* wyn_rwlock_new(void);
void wyn_rwlock_free(WynRwLock* rwlock);
WynRwLockReadGuard* wyn_rwlock_read(WynRwLock* rwlock);
WynRwLockWriteGuard* wyn_rwlock_write(WynRwLock* rwlock);
bool wyn_rwlock_try_read(WynRwLock* rwlock, WynRwLockReadGuard** guard);
bool wyn_rwlock_try_write(WynRwLock* rwlock, WynRwLockWriteGuard** guard);
void wyn_rwlock_read_guard_unlock(WynRwLockReadGuard* guard);
void wyn_rwlock_write_guard_unlock(WynRwLockWriteGuard* guard);

// Channel operations
WynChannel* wyn_channel_new(size_t capacity);
void wyn_channel_free(WynChannel* channel);
bool wyn_channel_send(WynChannel* channel, void* data);
bool wyn_channel_try_send(WynChannel* channel, void* data);
bool wyn_channel_recv(WynChannel* channel, void** data);
bool wyn_channel_try_recv(WynChannel* channel, void** data);
void wyn_channel_close(WynChannel* channel);
bool wyn_channel_is_closed(const WynChannel* channel);
size_t wyn_channel_len(const WynChannel* channel);
size_t wyn_channel_capacity(const WynChannel* channel);

// Atomic operations
WynAtomic* wyn_atomic_new(int64_t initial_value);
void wyn_atomic_free(WynAtomic* atomic);
int64_t wyn_atomic_load(const WynAtomic* atomic);
void wyn_atomic_store(WynAtomic* atomic, int64_t value);
int64_t wyn_atomic_fetch_add(WynAtomic* atomic, int64_t value);
int64_t wyn_atomic_fetch_sub(WynAtomic* atomic, int64_t value);
bool wyn_atomic_compare_exchange(WynAtomic* atomic, int64_t* expected, int64_t desired);
int64_t wyn_atomic_exchange(WynAtomic* atomic, int64_t value);

// Thread-safe collections (basic implementations)
typedef struct WynThreadSafeVec {
    void** data;
    size_t len;
    size_t cap;
    WynMutex mutex;
} WynThreadSafeVec;

WynThreadSafeVec* wyn_threadsafe_vec_new(void);
void wyn_threadsafe_vec_free(WynThreadSafeVec* vec);
bool wyn_threadsafe_vec_push(WynThreadSafeVec* vec, void* item);
bool wyn_threadsafe_vec_pop(WynThreadSafeVec* vec, void** item);
bool wyn_threadsafe_vec_get(WynThreadSafeVec* vec, size_t index, void** item);
size_t wyn_threadsafe_vec_len(WynThreadSafeVec* vec);

// Utility functions
size_t wyn_thread_hardware_concurrency(void);
uint64_t wyn_thread_current_id(void);

#endif // WYN_THREADING_H
