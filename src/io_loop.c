// I/O event loop - kqueue (macOS) / epoll (Linux)
// When a coroutine yields on I/O, it registers the fd + its Task* here.
// Workers call wyn_io_poll() which re-enqueues tasks whose fds are ready.

#include "io_loop.h"
#include <stdatomic.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#include <sched.h>   // sched_yield (io init contention spin)
#include <time.h>    // nanosleep (poll_wait fallback when no reactor exists)
#endif

// Implemented in spawn_fast.c
extern void wyn_sched_enqueue(void* task_ptr);

#define MAX_IO_EVENTS 256
static _Atomic int io_initialized = 0;

// Set the first time ANY fd or timer is registered. The reactor is now created
// eagerly at scheduler start (so the wake channel always exists), which means
// wyn_io_poll can no longer use "fd < 0" as its free fast path - and paying a
// kevent/epoll_wait syscall per spin round regressed 1M spawns 767ms -> 999ms.
// A program that never does I/O never sets this, so its poll stays free.
// Monotone by design: once anything registers we poll unconditionally, exactly
// as before.
static _Atomic int io_ever_registered = 0;

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
int wyn_io_poll_wait(int t) { (void)t; return 0; }
void wyn_io_wake(void) {}
int wyn_io_has_reactor(void) { return 0; }
void wyn_io_shutdown(void) {}
#else

#include <sys/event.h>

// Atomic so the awaited-spawn coroutine path (W8) can hit these from multiple
// worker threads + main without racing on the lazy init. The init winner creates
// the fd and publishes it (release); readers acquire; losers spin until it's set.
static _Atomic int kq_fd = -1;

// Reserved EVFILT_USER ident used to interrupt a blocking kevent(). Outside the
// fd number space and far below the timer ident base (1<<40), so it can never
// collide with a socket or a timer registration.
#define WYN_IO_WAKE_IDENT ((uintptr_t)1)

void wyn_io_init(void) {
    if (atomic_exchange(&io_initialized, 1)) {
        // A concurrent initializer is/was running; wait until it published the fd.
        while (atomic_load_explicit(&kq_fd, memory_order_acquire) < 0) sched_yield();
        return;
    }
    int kq = kqueue();
    if (kq >= 0) {
        // Persistent EVFILT_USER channel; NOTE_TRIGGER on it wakes a blocked
        // kevent(). udata NULL so wyn_io_poll* never treats it as a task ptr.
        struct kevent ev;
        EV_SET(&ev, WYN_IO_WAKE_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);
        kevent(kq, &ev, 1, NULL, 0, NULL);
    }
    atomic_store_explicit(&kq_fd, kq, memory_order_release);
}

int wyn_io_has_reactor(void) { return 1; }

void wyn_io_wake(void) {
    int kq = atomic_load_explicit(&kq_fd, memory_order_acquire);
    if (kq < 0) return;
    struct kevent ev;
    EV_SET(&ev, WYN_IO_WAKE_IDENT, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);
}

void wyn_io_wait_readable(int fd, void* task_ptr) {
    atomic_store_explicit(&io_ever_registered, 1, memory_order_release);
    if (atomic_load_explicit(&kq_fd, memory_order_acquire) < 0) wyn_io_init();
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, task_ptr);
    kevent(atomic_load_explicit(&kq_fd, memory_order_acquire), &ev, 1, NULL, 0, NULL);
}

void wyn_io_wait_writable(int fd, void* task_ptr) {
    atomic_store_explicit(&io_ever_registered, 1, memory_order_release);
    if (atomic_load_explicit(&kq_fd, memory_order_acquire) < 0) wyn_io_init();
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, task_ptr);
    kevent(atomic_load_explicit(&kq_fd, memory_order_acquire), &ev, 1, NULL, 0, NULL);
}

int wyn_io_wait_timer(void* task_ptr, long long ms) {
    atomic_store_explicit(&io_ever_registered, 1, memory_order_release);
    if (atomic_load_explicit(&kq_fd, memory_order_acquire) < 0) wyn_io_init();
    if (ms < 0) ms = 0;
    // Unique ident well outside the fd number space so it never collides with a
    // socket registered for read/write. EV_ONESHOT auto-removes it after firing.
    uintptr_t ident = (uintptr_t)(atomic_fetch_add(&io_timer_seq, 1) + (1LL << 40));
    struct kevent ev;
    EV_SET(&ev, ident, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_USECONDS,
           (int64_t)ms * 1000, task_ptr);
    return kevent(atomic_load_explicit(&kq_fd, memory_order_acquire), &ev, 1, NULL, 0, NULL) == 0 ? 1 : 0;
}

int wyn_io_poll(void) {
    // Fast path: nothing has ever been registered, so there is provably nothing
    // to report. Workers call this on every spin round, and since the reactor is
    // now created eagerly the old "kq_fd < 0" check no longer short-circuits it -
    // without this, pure-compute workloads pay a kevent syscall per round.
    if (!atomic_load_explicit(&io_ever_registered, memory_order_acquire)) return 0;
    int kq = atomic_load_explicit(&kq_fd, memory_order_acquire);
    if (kq < 0) return 0;
    struct kevent events[MAX_IO_EVENTS];
    struct timespec zero = {0, 0};
    int n = kevent(kq, NULL, 0, events, MAX_IO_EVENTS, &zero);
    for (int i = 0; i < n; i++) {
        if (events[i].udata) wyn_sched_enqueue(events[i].udata);
    }
    return n > 0 ? n : 0;
}

// Sleep for timeout_ms. Used only when the reactor could not be created, so a
// caller that spin-loops on poll_wait still yields the CPU instead of spinning.
static void wyn_io_blind_sleep(int timeout_ms) {
    if (timeout_ms < 0) timeout_ms = 1000;
    struct timespec req = { timeout_ms / 1000, (long)(timeout_ms % 1000) * 1000000L };
    nanosleep(&req, NULL);
}

int wyn_io_poll_wait(int timeout_ms) {
    // The reactor is created LAZILY by the first wyn_io_wait_* call. A program
    // that spawns a task with no yield point never registers anything, so kq_fd
    // would still be -1 here - and returning 0 without waiting turned the
    // designated-poller claim into a tight 200%-CPU spin. Create it on demand so
    // this call always blocks (the EVFILT_USER wake channel is what a concurrent
    // wake_processor() needs anyway).
    if (atomic_load_explicit(&kq_fd, memory_order_acquire) < 0) wyn_io_init();
    int kq = atomic_load_explicit(&kq_fd, memory_order_acquire);
    if (kq < 0) { wyn_io_blind_sleep(timeout_ms); return 0; }
    struct kevent events[MAX_IO_EVENTS];
    struct timespec ts, *tsp = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }
    int n = kevent(kq, NULL, 0, events, MAX_IO_EVENTS, tsp);
    int woke = 0;
    for (int i = 0; i < n; i++) {
        // udata NULL == the EVFILT_USER wake channel; nothing to enqueue.
        if (events[i].udata) { wyn_sched_enqueue(events[i].udata); woke++; }
    }
    return woke;
}

void wyn_io_shutdown(void) {
    int kq = atomic_exchange_explicit(&kq_fd, -1, memory_order_acq_rel);
    if (kq >= 0) close(kq);
    atomic_store(&io_initialized, 0);
}

#endif // !__TINYC__

// ============================================================================
// Linux: epoll
// ============================================================================
#elif defined(__linux__)

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <stdint.h>

#include <stdlib.h>
#include <pthread.h>

static _Atomic int ep_fd = -1;  // atomic: awaited-coro path (W8) hits it from many threads
static _Atomic int wake_fd = -1;  // eventfd used to interrupt a blocking epoll_wait

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
    if (atomic_exchange(&io_initialized, 1)) {
        while (atomic_load_explicit(&ep_fd, memory_order_acquire) < 0) sched_yield();
        return;
    }
    int ep = epoll_create1(0);
    if (ep >= 0) {
        int wf = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wf >= 0) {
            // data.ptr NULL so the dispatch helper recognises it as the wake
            // channel and never treats it as a task ptr.
            struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL };
            if (epoll_ctl(ep, EPOLL_CTL_ADD, wf, &ev) < 0) { close(wf); wf = -1; }
        }
        atomic_store_explicit(&wake_fd, wf, memory_order_release);
    }
    atomic_store_explicit(&ep_fd, ep, memory_order_release);
}

int wyn_io_has_reactor(void) { return 1; }

void wyn_io_wake(void) {
    int wf = atomic_load_explicit(&wake_fd, memory_order_acquire);
    if (wf < 0) return;
    uint64_t one = 1;
    ssize_t r = write(wf, &one, sizeof(one));
    (void)r;
}

void wyn_io_wait_readable(int fd, void* task_ptr) {
    atomic_store_explicit(&io_ever_registered, 1, memory_order_release);
    if (atomic_load_explicit(&ep_fd, memory_order_acquire) < 0) wyn_io_init();
    int ep = atomic_load_explicit(&ep_fd, memory_order_acquire);
    struct epoll_event ev = { .events = EPOLLIN | EPOLLONESHOT, .data.ptr = task_ptr };
    // Try ADD first, if fd already registered use MOD
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev) < 0)
        epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ev);
}

void wyn_io_wait_writable(int fd, void* task_ptr) {
    atomic_store_explicit(&io_ever_registered, 1, memory_order_release);
    if (atomic_load_explicit(&ep_fd, memory_order_acquire) < 0) wyn_io_init();
    int ep = atomic_load_explicit(&ep_fd, memory_order_acquire);
    struct epoll_event ev = { .events = EPOLLOUT | EPOLLONESHOT, .data.ptr = task_ptr };
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev) < 0)
        epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ev);
}

int wyn_io_wait_timer(void* task_ptr, long long ms) {
    atomic_store_explicit(&io_ever_registered, 1, memory_order_release);
    if (atomic_load_explicit(&ep_fd, memory_order_acquire) < 0) wyn_io_init();
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
    if (epoll_ctl(atomic_load_explicit(&ep_fd, memory_order_acquire), EPOLL_CTL_ADD, tfd, &ev) < 0) {
        pthread_mutex_lock(&timer_lock);
        timer_registry[slot] = NULL; active_timers--;
        pthread_mutex_unlock(&timer_lock);
        free(node); close(tfd);
        return 0;
    }
    return 1;
}

// Shared event-dispatch body used by BOTH wyn_io_poll and wyn_io_poll_wait, so
// the blocking and non-blocking paths can never disagree about how an event is
// turned back into a runnable task.
static int wyn_io_dispatch(struct epoll_event* events, int n) {
    int woke = 0;
    for (int i = 0; i < n; i++) {
        void* ptr = events[i].data.ptr;
        if (!ptr) {
            // The wake eventfd. Its counter MUST be drained or EPOLLIN stays
            // level-triggered and every subsequent epoll_wait returns instantly
            // (i.e. the spin we are removing comes straight back).
            int wf = atomic_load_explicit(&wake_fd, memory_order_acquire);
            if (wf >= 0) { uint64_t v; ssize_t r = read(wf, &v, sizeof(v)); (void)r; }
            continue;
        }
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
            if (task) { wyn_sched_enqueue(task); woke++; }
        } else {
            wyn_sched_enqueue(ptr);  // plain socket task ptr
            woke++;
        }
    }
    return woke;
}

int wyn_io_poll(void) {
    // Fast path: see the kqueue branch - the eagerly-created reactor means this
    // is the only thing keeping a pure-compute spin round syscall-free.
    if (!atomic_load_explicit(&io_ever_registered, memory_order_acquire)) return 0;
    int ep = atomic_load_explicit(&ep_fd, memory_order_acquire);
    if (ep < 0) return 0;
    struct epoll_event events[MAX_IO_EVENTS];
    int n = epoll_wait(ep, events, MAX_IO_EVENTS, 0);
    if (n <= 0) return 0;
    wyn_io_dispatch(events, n);
    return n;
}

// Sleep for timeout_ms. Used only when the reactor could not be created, so a
// caller that spin-loops on poll_wait still yields the CPU instead of spinning.
static void wyn_io_blind_sleep(int timeout_ms) {
    if (timeout_ms < 0) timeout_ms = 1000;
    struct timespec req = { timeout_ms / 1000, (long)(timeout_ms % 1000) * 1000000L };
    nanosleep(&req, NULL);
}

int wyn_io_poll_wait(int timeout_ms) {
    // Lazily create the reactor: see the kqueue branch for why returning 0
    // without waiting here becomes a tight CPU spin in the designated poller.
    if (atomic_load_explicit(&ep_fd, memory_order_acquire) < 0) wyn_io_init();
    int ep = atomic_load_explicit(&ep_fd, memory_order_acquire);
    if (ep < 0) { wyn_io_blind_sleep(timeout_ms); return 0; }
    struct epoll_event events[MAX_IO_EVENTS];
    int n = epoll_wait(ep, events, MAX_IO_EVENTS, timeout_ms < 0 ? -1 : timeout_ms);
    if (n <= 0) return 0;
    return wyn_io_dispatch(events, n);
}

void wyn_io_shutdown(void) {
    int ep = atomic_exchange_explicit(&ep_fd, -1, memory_order_acq_rel);
    int wf = atomic_exchange_explicit(&wake_fd, -1, memory_order_acq_rel);
    if (wf >= 0) close(wf);
    if (ep >= 0) close(ep);
    atomic_store(&io_initialized, 0);
}

// ============================================================================
// Fallback: no-op - busy-yield still works
// ============================================================================
#else

void wyn_io_init(void) { (void)io_initialized; }
void wyn_io_wait_readable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
void wyn_io_wait_writable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
int wyn_io_wait_timer(void* task_ptr, long long ms) { (void)task_ptr; (void)ms; (void)io_timer_seq; return 0; }
int wyn_io_poll(void) { return 0; }
int wyn_io_poll_wait(int t) { (void)t; return 0; }
void wyn_io_wake(void) {}
int wyn_io_has_reactor(void) { return 0; }
void wyn_io_shutdown(void) {}

#endif
