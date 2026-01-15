#include <stdlib.h>
#include <unistd.h>
#include "async_runtime.h"

WynFuture* wyn_future_new(void) {
    WynFuture* future = malloc(sizeof(WynFuture));
    future->state = WYN_FUTURE_PENDING;
    future->value = NULL;
    return future;
}

int wyn_future_poll(WynFuture* future) {
    return future->state == WYN_FUTURE_READY;
}

void wyn_future_ready(WynFuture* future, void* value) {
    future->value = value;
    future->state = WYN_FUTURE_READY;
}

void* wyn_block_on(WynFuture* future) {
    while (!wyn_future_poll(future)) {
        usleep(1000); // 1ms
    }
    void* result = future->value;
    free(future);
    return result;
}