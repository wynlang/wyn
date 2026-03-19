#ifndef WYN_IO_LOOP_H
#define WYN_IO_LOOP_H

// Initialize the I/O event loop (called once at startup)
void wyn_io_init(void);

// Register fd for read readiness. When ready, task_ptr is re-enqueued to scheduler.
void wyn_io_wait_readable(int fd, void* task_ptr);

// Register fd for write readiness.
void wyn_io_wait_writable(int fd, void* task_ptr);

// Poll for ready events and re-enqueue tasks. Non-blocking.
int wyn_io_poll(void);

// Shutdown the I/O loop.
void wyn_io_shutdown(void);

// Get current task pointer (for I/O registration). Returns NULL if not in a task.
void* wyn_current_task(void);

// Mark current coroutine as I/O-parked (scheduler won't re-enqueue on yield).
void wyn_io_park(void);

#endif
