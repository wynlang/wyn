#ifndef WYN_IO_LOOP_H
#define WYN_IO_LOOP_H

// Initialize the I/O event loop (called once at startup)
void wyn_io_init(void);

// Register fd for read readiness. When ready, task_ptr is re-enqueued to scheduler.
void wyn_io_wait_readable(int fd, void* task_ptr);

// Register fd for write readiness.
void wyn_io_wait_writable(int fd, void* task_ptr);

// Register a one-shot timer. After `ms` milliseconds elapse, task_ptr is
// re-enqueued to the scheduler (like an fd becoming ready), letting a
// coroutine sleep cooperatively instead of blocking its worker thread.
// Returns 1 if the timer was armed, 0 on the TCC/fallback stub paths (caller
// then falls back to a blocking sleep).
int wyn_io_wait_timer(void* task_ptr, long long ms);

// Poll for ready events and re-enqueue tasks. Non-blocking.
int wyn_io_poll(void);

// Blocking variant: wait up to timeout_ms (negative = indefinitely) for I/O or
// timer readiness, re-enqueue the ready tasks, return the count.
//
// CONTRACT: on a reactor build (wyn_io_has_reactor() == 1) this ALWAYS waits -
// it lazily creates the reactor if no fd/timer has been registered yet, and
// falls back to a plain sleep if the reactor cannot be created at all. Callers
// spin-loop on it, so a "return 0 immediately" path would reintroduce exactly
// the busy-spin this exists to remove.
//
// On non-reactor builds (TCC / fallback stubs) it returns 0 without waiting;
// callers MUST gate on wyn_io_has_reactor() and keep their old yield-spin.
int wyn_io_poll_wait(int timeout_ms);

// Interrupt a thread blocked in wyn_io_poll_wait. Idempotent, async-safe.
void wyn_io_wake(void);

// 1 if this build has a real reactor (kqueue/epoll) behind wyn_io_poll_wait.
int wyn_io_has_reactor(void);

// Shutdown the I/O loop.
void wyn_io_shutdown(void);

// Get current task pointer (for I/O registration). Returns NULL if not in a task.
void* wyn_current_task(void);

// Mark current coroutine as I/O-parked (scheduler won't re-enqueue on yield).
void wyn_io_park(void);

#endif
