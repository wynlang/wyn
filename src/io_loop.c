// I/O event loop — kqueue (macOS) / epoll (Linux)
// When a coroutine yields on I/O, it registers the fd + its Task* here.
// Workers call wyn_io_poll() which re-enqueues tasks whose fds are ready.

#include "io_loop.h"
#include <stdatomic.h>
#include <unistd.h>

// Implemented in spawn_fast.c
extern void wyn_sched_enqueue(void* task_ptr);

#define MAX_IO_EVENTS 256
static _Atomic int io_initialized = 0;

// ============================================================================
// macOS: kqueue
// ============================================================================
#if defined(__APPLE__)

#ifdef __TINYC__
// TCC on macOS: stub implementations (kqueue headers not available)
void wyn_io_init(void) { atomic_exchange(&io_initialized, 1); }
void wyn_io_wait_readable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
void wyn_io_wait_writable(int fd, void* task_ptr) { (void)fd; (void)task_ptr; }
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

static int ep_fd = -1;

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

int wyn_io_poll(void) {
    if (ep_fd < 0) return 0;
    struct epoll_event events[MAX_IO_EVENTS];
    int n = epoll_wait(ep_fd, events, MAX_IO_EVENTS, 0);
    for (int i = 0; i < n; i++) {
        if (events[i].data.ptr) wyn_sched_enqueue(events[i].data.ptr);
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
int wyn_io_poll(void) { return 0; }
void wyn_io_shutdown(void) {}

#endif
