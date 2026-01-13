#ifndef PLATFORM_H
#define PLATFORM_H

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #ifndef WYN_PLATFORM_WINDOWS
        #define WYN_PLATFORM_WINDOWS
    #endif
    #define WYN_PLATFORM_NAME "windows"
#elif defined(__APPLE__)
    #ifndef WYN_PLATFORM_MACOS
        #define WYN_PLATFORM_MACOS
    #endif
    #define WYN_PLATFORM_NAME "macos"
#elif defined(__linux__)
    #ifndef WYN_PLATFORM_LINUX
        #define WYN_PLATFORM_LINUX
    #endif
    #define WYN_PLATFORM_NAME "linux"
#else
    #ifndef WYN_PLATFORM_UNKNOWN
        #define WYN_PLATFORM_UNKNOWN
    #endif
    #define WYN_PLATFORM_NAME "unknown"
#endif

// Platform-specific includes
#ifdef WYN_PLATFORM_WINDOWS
    #include <windows.h>
    #include <winsock2.h>
    #define WYN_PATH_SEP "\\"
    #define WYN_EXE_EXT ".exe"
#else
    #include <unistd.h>
    #include <sys/types.h>
    #define WYN_PATH_SEP "/"
    #define WYN_EXE_EXT ""
#endif

// Platform-specific functions
const char* wyn_get_platform_name(void);
int wyn_platform_init(void);
void wyn_platform_cleanup(void);

#endif // PLATFORM_H