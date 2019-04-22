#ifndef __AC_PLATFORM_H
#define __AC_PLATFORM_H

// platform definitions. Not intended for replacing types or checking for libraries.

#ifdef _WIN32
    //define something for Windows (32-bit and 64-bit)
    #define WINDOWS_VERSION
#elif defined(__APPLE__)
    #include "TargetConditionals.h"
    #if TARGET_OS_SIMULATOR
        #define IOS_VERSION
    #elif TARGET_OS_IOS
        #define IOS_VERSION
    #elif TARGET_OS_OSX
        #define MAC_VERSION
    #else
        #error "Unknown Apple platform"
    #endif
#elif defined(__ANDROID__) || defined(ANDROID)
    #define ANDROID_VERSION
#elif defined(__linux__)
    #define LINUX_VERSION
#else
    #error "Unknown platform"
#endif


#if defined(__LP64__)
    // LP64 machine, OS X or Linux
    // int 32bit | long 64bit | long long 64bit | void* 64bit
    #define AGS_64BIT
    #define AGS_LP64
#elif defined(_WIN64)
    // LLP64 machine, Windows
    // int 32bit | long 32bit | long long 64bit | void* 64bit
    #define AGS_64BIT
    #define AGS_LLP64
#else
    // 32-bit machine, Windows or Linux or OS X
    // int 32bit | long 32bit | long long 64bit | void* 32bit
    #define AGS_32BIT
    #define AGS_ILP32
#endif

#ifdef _MSC_VER

    // the linux compiler won't allow extern inline
    #define AGS_PLAT_HAS_EXTERN_INLINE

#endif

#if defined(_WIN32)
    #define AGS_LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define AGS_LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define AGS_BIG_ENDIAN
#else
    #error "Unknown platform"
#endif

#endif // __AC_PLATFORM_H