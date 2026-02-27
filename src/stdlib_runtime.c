// Standard Library Runtime Wrappers for Wyn
#include <stdlib.h>
#include <string.h>

// Time wrappers
extern long wyn_time_now();
extern long long wyn_time_now_millis();
extern void wyn_time_sleep(int seconds);

long Time_now() {
    return wyn_time_now();
}

long long Time_now_millis() {
    return wyn_time_now_millis();
}

// Time_sleep is defined in wyn_runtime.h

// Crypto wrappers
extern unsigned int wyn_crypto_hash32(const char* data, size_t len);
extern unsigned long long wyn_crypto_hash64(const char* data, size_t len);

unsigned int Crypto_hash32(const char* data) {
    return wyn_crypto_hash32(data, strlen(data));
}

unsigned long long Crypto_hash64(const char* data) {
    return wyn_crypto_hash64(data, strlen(data));
}
