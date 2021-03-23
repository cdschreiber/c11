#ifndef __TIME_H__
#define __TIME_H__

/*
 *  Copyright (c) 2015-2021 Christoph Schreiber
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (http://www.boost.org/LICENSE_1_0.txt)
 */

#include <c11/_cdefs.h>

#if defined(HAVE_TIME_H)
#   include <time.h>
#else
#   define HAVE_TIME_H_WORKAROUND 1
#endif /* defined(HAVE_TIME_H) */

#if defined(HAVE_TIME_H_WORKAROUND)

#if defined(__APPLE__)

#include <time.h>
#include <sys/time.h>

/*
 *  7.27.1 Components of time
 */

#if !defined(TIME_UTC)
#   define TIME_UTC 1
#endif /* !defined(TIME_UTC) */

#if !defined(CLOCK_REALTIME)
#   define CLOCK_REALTIME 0
#endif /* !defined(CLOCK_REALTIME) */

#if !defined(_STRUCT_TIMESPEC)

struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};

#endif /* !defined(_STRUCT_TIMESPEC) */

/*
 *  7.27.2 Time manipulation functions
 */

static int clock_gettime_workaround(long unused, struct timespec* ts)
{
    (void)unused;
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000L;
    return 0;
}

static inline int timespec_get(struct timespec* ts, int base)
{
    static volatile atomic_intptr_t get_system_time = 0;
    intptr_t proc = atomic_load(&get_system_time);
    if (proc == 0)
    {
#if ((__MAC_OS_X_VERSION_MIN_REQUIRED+0) >= 101200) /* Sierra */
        proc = (intptr_t)clock_gettime;
        if (proc == 0)
#endif /* ((__MAC_OS_X_VERSION_MIN_REQUIRED+0) >= 101200) */
        {
            proc = (intptr_t)clock_gettime_workaround;
        }
        atomic_store(&get_system_time, proc);
    }
    ((int (*)(long, struct timespec*))proc)(CLOCK_REALTIME, ts);
    return base;
}

#elif defined(_WIN32)

#include <assert.h>
#include <time.h>
#include <c11/stdatomic.h>

#define WIN32_LEAN_AND_MEAN  1
#include <windows.h>

/*
 *  7.27.1 Components of time
 */

#if !defined(TIME_UTC)
#   define TIME_UTC 1
#endif /* !defined(TIME_UTC) */

#if !defined(_TIMESPEC_DEFINED)

struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};

#endif /* !defined(_TIMESPEC_DEFINED) */

/*
 *  7.27.2 Time manipulation functions
 */

typedef void (WINAPI* GET_SYSTEM_TIME_AS_FILETIME)(LPFILETIME);

static inline int timespec_get(struct timespec* ts, int base)
{
    static volatile atomic_intptr_t get_system_time_as_filetime = 0;
    intptr_t proc = atomic_load(&get_system_time_as_filetime);
    if (proc == 0)
    {
        HMODULE hnd = GetModuleHandleW(L"kernel32.dll");
        assert(hnd);
        proc = (intptr_t)GetProcAddress(hnd
            , "GetSystemTimePreciseAsFileTime");
        if (proc == 0)
        {
            proc = (intptr_t)GetProcAddress(hnd
                , "GetSystemTimeAsFileTime");
            assert(proc);
        }
        atomic_store(&get_system_time_as_filetime, proc);
    }
    if (base != TIME_UTC)
        return 0;
    FILETIME ft = { 0 };
    ((GET_SYSTEM_TIME_AS_FILETIME)proc)(&ft);
    LARGE_INTEGER li = { 0 };
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = (LONG)ft.dwHighDateTime;
    li.QuadPart -= 116444736000000000LL;
    ts->tv_sec = (time_t)(li.QuadPart / 10000000LL);
    ts->tv_nsec = (long)(li.QuadPart % 10000000LL) * 100L;
    return base;
}

#endif /* defined(__APPLE__) */

#endif /* defined(HAVE_TIME_H_WORKAROUND) */

#undef HAVE_TIME_H_WORKAROUND

#endif /* __TIME_H__ */
