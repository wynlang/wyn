#include "platform.h"
#include <stdio.h>

const char* wyn_get_platform_name(void) {
    return WYN_PLATFORM_NAME;
}

int wyn_platform_init(void) {
#ifdef WYN_PLATFORM_WINDOWS
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

void wyn_platform_cleanup(void) {
#ifdef WYN_PLATFORM_WINDOWS
    WSACleanup();
#endif
}