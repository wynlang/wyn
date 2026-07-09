// I/O event loop — kqueue (macOS) / epoll (Linux)
// When a coroutine yields on I/O, it registers the fd + its Task* here.
// Workers call wyn_io_poll() which re-enqueues tasks whose fds are ready.

#include "io_loop.h"
#include <stdatomic.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// Implemented in spawn_fast.c
extern void wyn_sched_enqueue(void* task_ptr);

#define MAX_IO_EVENTS 256
static _Atomic int io_initialized = 0;

// Monotonic identifier source for one-shot timers (kqueue EVFILT_TIMER idents,
// kept out of the fd number space to avoid colliding with real sockets).
static _Atomic long long io_timer_seq = 0;

// ============================================================================
// macOS: kqueue
// ============================================================================
#if defined(__APPLE__)

#ifdef __TINYC__
// TCC on macOS: stub implementations (kqueue headers not available)
void wyn_io_init(void) { atomic_exchange(&io_initialized, 1); }
void wyn_io_wait_readable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
void wyn_io_wait_writable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
int wyn_io_wait_timer(void* task_ptr, long long ms) { (void)task_ptr; (void)ms; return 0; }
int wyn_io_poll(void) { return 0; }
void wyn_io_shutdown(void) {}
#else

#include <sys/event.h>

static int kq_fd = -1;

void wyn_io_init(void) {
    if (atomic_exchange(&io_initialized, 1)) return;
    kq_fd = kqueue();
}

void wyn_io_wait_readable(int fd, void* task_ptr) {
    if (kq_fd < 0) wyn_io_init();
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, task_ptr);
    kevent(kq_fd, &ev, 1, NULL, 0, NULL);
}

void wyn_io_wait_writable(int fd, void* task_ptr) {
    if (kq_fd < 0) wyn_io_init();
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, task_ptr);
    kevent(kq_fd, &ev, 1, NULL, 0, NULL);
}

int wyn_io_wait_timer(void* task_ptr, long long ms) {
    if (kq_fd < 0) wyn_io_init();
    if (ms < 0) ms = 0;
    // Unique ident well outside the fd number space so it never collides with a
    // socket registered for read/write. EV_ONESHOT auto-removes it after firing.
    uintptr_t ident = (uintptr_t)(atomic_fetch_add(&io_timer_seq, 1) + (1LL << 40));
    struct kevent ev;
    EV_SET(&ev, ident, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_USECONDS,
           (int64_t)ms * 1000, task_ptr);
    return kevent(kq_fd, &ev, 1, NULL, 0, NULL) == 0 ? 1 : 0;
}

int wyn_io_poll(void) {
    if (kq_fd < 0) return 0;
    struct kevent events[MAX_IO_EVENTS];
    struct timespec zero = {0, 0};
    int n = kevent(kq_fd, NULL, 0, events, MAX_IO_EVENTS, &zero);
    for (int i = 0; i < n; i++) {
        if (events[i].udata) wyn_sched_enqueue(events[i].udata);
    }
    return n > 0 ? n : 0;
}

void wyn_io_shutdown(void) {
    if (kq_fd >= 0) { close(kq_fd); kq_fd = -1; }
    atomic_store(&io_initialized, 0);
}

#endif // !__TINYC__

// ============================================================================
// Linux: epoll
// ============================================================================
#elif defined(__linux__)

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <stdint.h>

#include <stdlib.h>
#include <pthread.h>

static int ep_fd = -1;

// A one-shot timer needs its timerfd closed after it fires. epoll_event.data is
// a union, so we can't stash both the task ptr and the fd in one event. Instead
// we wrap them in a heap node and store the node pointer in .data.ptr, then keep
// a registry of live node pointers so wyn_io_poll can tell a timer node apart
// from a plain task ptr (sockets store the raw task ptr). When active_timers is
// 0 the poll fast-path skips the registry check entirely (no socket overhead).
typedef struct { int tfd; void* task; } TimerNode;
#define MAX_TIMERFDS 4096
static TimerNode* timer_registry[MAX_TIMERFDS];
static int active_timers = 0;
static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;

// Returns the node's index if ptr is a registered timer node, else -1.
static int timer_registry_find(void* ptr) {
    if (active_timers == 0) return -1;
    for (int i = 0; i < MAX_TIMERFDS; i++) {
        if (timer_registry[i] == ptr) return i;
    }
    return -1;
}

void wyn_io_init(void) {
    if (atomic_exchange(&io_initialized, 1)) return;
    ep_fd = epoll_create1(0);
}

void wyn_io_wait_readable(int fd, void* task_ptr) {
    if (ep_fd < 0) wyn_io_init();
    struct epoll_event ev = { .events = EPOLLIN | EPOLLONESHOT, .data.ptr = task_ptr };
    // Try ADD first, if fd already registered use MOD
    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &ev);
}

void wyn_io_wait_writable(int fd, void* task_ptr) {
    if (ep_fd < 0) wyn_io_init();
    struct epoll_event ev = { .events = EPOLLOUT | EPOLLONESHOT, .data.ptr = task_ptr };
    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &ev);
}

int wyn_io_wait_timer(void* task_ptr, long long ms) {
    if (ep_fd < 0) wyn_io_init();
    if (ms < 0) ms = 0;
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) return 0;
    struct itimerspec its = {0};
    its.it_value.tv_sec = ms / 1000;
    // A zero timeout leaves the timer disarmed, so clamp to 1ns to still fire.
    its.it_value.tv_nsec = (ms == 0) ? 1 : (ms % 1000) * 1000000LL;
    if (timerfd_settime(tfd, 0, &its, NULL) < 0) { close(tfd); return 0; }

    TimerNode* node = malloc(sizeof(TimerNode));
    if (!node) { close(tfd); return 0; }
    node->tfd = tfd;
    node->task = task_ptr;

    pthread_mutex_lock(&timer_lock);
    int slot = -1;
    for (int i = 0; i < MAX_TIMERFDS; i++) {
        if (!timer_registry[i]) { timer_registry[i] = node; slot = i; break; }
    }
    if (slot >= 0) active_timers++;
    pthread_mutex_unlock(&timer_lock);
    if (slot < 0) { free(node); close(tfd); return 0; }  // registry full

    struct epoll_event ev = { .events = EPOLLIN | EPOLLONESHOT, .data.ptr = node };
    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, tfd, &ev) < 0) {
        pthread_mutex_lock(&timer_lock);
        timer_registry[slot] = NULL; active_timers--;
        pthread_mutex_unlock(&timer_lock);
        free(node); close(tfd);
        return 0;
    }
    return 1;
}

int wyn_io_poll(void) {
    if (ep_fd < 0) return 0;
    struct epoll_event events[MAX_IO_EVENTS];
    int n = epoll_wait(ep_fd, events, MAX_IO_EVENTS, 0);
    for (int i = 0; i < n; i++) {
        void* ptr = events[i].data.ptr;
        if (!ptr) continue;
        pthread_mutex_lock(&timer_lock);
        int slot = timer_registry_find(ptr);
        if (slot >= 0) { timer_registry[slot] = NULL; active_timers--; }
        pthread_mutex_unlock(&timer_lock);
        if (slot >= 0) {
            // Timer fired: close its timerfd, resume the parked task.
            TimerNode* node = (TimerNode*)ptr;
            close(node->tfd);
            void* task = node->task;
            free(node);
            if (task) wyn_sched_enqueue(task);
        } else {
            wyn_sched_enqueue(ptr);  // plain socket task ptr
        }
    }
    return n > 0 ? n : 0;
}

void wyn_io_shutdown(void) {
    if (ep_fd >= 0) { close(ep_fd); ep_fd = -1; }
    atomic_store(&io_initialized, 0);
}

// ============================================================================
// Fallback: no-op — busy-yield still works
// ============================================================================
#else

void wyn_io_init(void) { (void)io_initialized; }
void wyn_io_wait_readable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
void wyn_io_wait_writable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
int wyn_io_wait_timer(void* task_ptr, long long ms) { (void)task_ptr; (void)ms; (void)io_timer_seq; return 0; }
int wyn_io_poll(void) { return 0; }
void wyn_io_shutdown(void) {}

#endif
