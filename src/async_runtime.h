#ifndef ASYNC_RUNTIME_H
#define ASYNC_RUNTIME_H

typedef enum {
    WYN_FUTURE_PENDING,
    WYN_FUTURE_READY
} WynFutureState;

typedef struct {
    WynFutureState state;
    void* value;
} WynFuture;

WynFuture* wyn_future_new(void);
int wyn_future_poll(WynFuture* future);
void wyn_future_ready(WynFuture* future, void* value);
void* wyn_block_on(WynFuture* future);

#endif