#ifndef _COMMON_COMMON_H
#define _COMMON_COMMON_H

#undef bool

#include "platform.h"

#include <stdlib.h>

#if !defined(TARGET_DOS16)

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

typedef uint8_t byte;
typedef uint16_t word;

#if defined(OS_LINUX) || defined(OS_MACOS)
// glibc/BSD <sys/types.h> already typedefs uint as a 32-bit unsigned int.
// Pull it in first, then shadow the name so game code keeps the 16-bit
// DOS-era uint.
#include <sys/types.h>
typedef uint16_t u5_uint;
#define uint u5_uint
#else
typedef uint16_t uint;
#endif

#else

typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

typedef signed char s8;
typedef signed int s16;
typedef signed long s32;

typedef unsigned char byte;
typedef unsigned int word;
typedef unsigned int uint;
typedef int bool;

#define true 1
#define false 0

#define const

#endif

#if defined(COMPILER_MSVC)

#define CHECK_PARAM_RANGE(v, inclusive_min, exclusive_max)                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((v) < (inclusive_min) || (v) >= (exclusive_max))                                                           \
            __debugbreak();                                                                                            \
    } while (0)

// TODO: message
#define ASSERT(x)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
            __debugbreak();                                                                                            \
    } while (0)

// use with caution
#define SET_UNINITIALIZED_8(x) (x) = 0xdd

#define CHECK_UNINITIALIZED_8(x)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((x) == 0xdd)                                                                                               \
            __debugbreak();                                                                                            \
    } while (0)

#define SET_UNINITIALIZED_16(x) (x) = 0xdead

#define CHECK_UNINITIALIZED_16(x)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((x) == 0xdead)                                                                                             \
            __debugbreak();                                                                                            \
    } while (0)

#elif !defined(TARGET_DOS16)

// TODO

#define CHECK_PARAM_RANGE(v, inclusive_min, exclusive_max)                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((v) < (inclusive_min) || (v) >= (exclusive_max))                                                           \
            *((u32*)0) = 1;                                                                                            \
    } while (0)

// TODO: message
#define ASSERT(x)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
            *((u32*)0) = 1;                                                                                            \
    } while (0)

// use with caution
#define SET_UNINITIALIZED_8(x) (x) = 0xdd

#define CHECK_UNINITIALIZED_8(x)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((x) == 0xdd)                                                                                               \
            *((u32*)0) = 1;                                                                                            \
    } while (0)

#define SET_UNINITIALIZED_16(x) (x) = 0xdead

#define CHECK_UNINITIALIZED_16(x)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((x) == 0xdead)                                                                                             \
            *((u32*)0) = 1;                                                                                            \
    } while (0)

#else

#define CHECK_PARAM_RANGE(v, inclusive_min, exclusive_max)
#define ASSERT(x)
#define SET_UNINITIALIZED_8(x)
#define CHECK_UNINITIALIZED_8(x)
#define SET_UNINITIALIZED_16(x)
#define CHECK_UNINITIALIZED_16(x)

#endif

extern void CDECL debug(char* str, ...);

#if defined(TARGET_SDL)
#define ENABLE_GRAP_OVERLAY 1
#define ENABLE_GRAP_OVERLAY_DEBUG 1
#endif

#if defined(COMPILER_MSVC)
#define u5_itoa _itoa
#elif defined(OS_LINUX) || defined(OS_MACOS)
// glibc and libSystem have no itoa.
static inline char* u5_itoa(int value, char* str, int radix)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char tmp[34];
    char* out = str;
    int i = 0;
    unsigned int v = (radix == 10 && value < 0) ? -(unsigned int)value : (unsigned int)value;

    if (radix == 10 && value < 0)
        *out++ = '-';
    do
    {
        tmp[i++] = digits[v % radix];
        v /= radix;
    } while (v != 0);
    while (i > 0)
        *out++ = tmp[--i];
    *out = '\0';
    return str;
}
#else
#define u5_itoa itoa
#endif

#if defined(OS_LINUX) || defined(OS_MACOS)
#include <sys/stat.h>
#define PATH_SEP "/"
#define u5_mkdir(path) mkdir((path), 0755)
#elif defined(OS_DOS32)
#include <sys/stat.h>
#define PATH_SEP "\\"
#define u5_mkdir(path) mkdir((path), 0755)
#else
#define PATH_SEP "\\"
// Callers include <Windows.h> before using this.
#define u5_mkdir(path) CreateDirectoryA((path), NULL)
#endif

#define ARRAYSIZE(array) ((int)(sizeof(array) / sizeof((array)[0])))

#if defined(ENABLE_TRANSLATION)
#include "translate.h"
#define _TEXT(addr, text) TRS_GetString(addr, text)
#if defined(_DEBUG)
#define _STATIC_TEXT(addr, text) #addr "|" ##text
#else
#define _STATIC_TEXT(addr, text) #addr
#endif
#else
#define _TEXT(addr, text) text
#define _STATIC_TEXT(addr, text) text
#endif

#endif
