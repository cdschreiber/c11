#ifndef __THREADS_H__
#define __THREADS_H__

/*
 *  Copyright (c) 2015-2021 Christoph Schreiber
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (http://www.boost.org/LICENSE_1_0.txt)
 */

#include <c11/_cdefs.h>

#if defined(HAVE_THREADS_H)
#   include <threads.h>
#else
#   define HAVE_THREADS_H_WORKAROUND 1
#endif /* defined(HAVE_THREADS_H) */

#if defined(HAVE_THREADS_H_WORKAROUND)

#if defined(__linux__) || defined(__APPLE__)
#   define HAVE_POSIX_THREADS 1
#elif defined(_WIN32)
#   define HAVE_WINDOWS_THREADS 1
#else
#   error Threads are not supported on your platform!
#endif /* defined(__linux__) ... */

#include <stdint.h>
#include <c11/time.h>

#if defined(HAVE_POSIX_THREADS)
#   include <errno.h>
#   include <pthread.h>
#   include <sched.h>
#   include <unistd.h>
#   if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L)
#       define HAVE_TIMEDLOCK 1
#   endif /* defined(_POSIX_TIMEOUTS) ... */
#elif defined(HAVE_WINDOWS_THREADS)
#   include <process.h>
#   define WIN32_LEAN_AND_MEAN  1
#   include <windows.h>
#endif /* defined(HAVE_POSIX_THREADS) */

/*
 *  7.26.1.3 Macros
 */

#define thread_local _Thread_local

#if defined(HAVE_POSIX_THREADS)
#   define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#   define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS
#elif defined(HAVE_WINDOWS_THREADS)
#   define ONCE_FLAG_INIT INIT_ONCE_STATIC_INIT
#   define TSS_DTOR_ITERATIONS 4
#endif /* defined(HAVE_POSIX_THREADS) */

/*
 *  7.26.1.4 Types
 */

#if defined(HAVE_POSIX_THREADS)

typedef pthread_cond_t cnd_t;

typedef pthread_t thrd_t;

typedef pthread_key_t tss_t;

typedef struct
{
    pthread_mutex_t mtx;
#if !defined(HAVE_TIMEDLOCK)
    pthread_cond_t cond;
    pthread_t thrdid;
    unsigned count;
    short locked;
    short type;
#endif /* !defined(HAVE_TIMEDLOCK) */
} mtx_t;

typedef pthread_once_t once_flag;

#elif defined(HAVE_WINDOWS_THREADS)

typedef CONDITION_VARIABLE cnd_t;

#if !defined(CACHELINE_SIZE)
#   define CACHELINE_SIZE 64
#endif /* !definedCACHELINE_SIZE) */

typedef intptr_t thrd_t;

typedef intptr_t tss_t;

typedef struct
{
    SRWLOCK srwlock;
    CONDITION_VARIABLE cv;
    DWORD thrdid;
    DWORD count;
    int locked;
    int type;
} mtx_t;

typedef INIT_ONCE once_flag;

#endif /* defined(HAVE_POSIX_THREADS) */

typedef void (*tss_dtor_t)(void*);

typedef int (*thrd_start_t)(void*);

/*
 *  7.26.1.5 Enumeration constants
 */

enum
{
    mtx_plain,
    mtx_recursive,
    mtx_timed
};

enum
{
    thrd_success,
    thrd_busy,
    thrd_error,
    thrd_nomem,
    thrd_timedout
};

/*
 *  7.26.2 Initialization functions
 */

#if defined(HAVE_POSIX_THREADS)

static inline void call_once(once_flag* flag, void (*func)(void))
{
    pthread_once(flag, func);
}

#elif defined(HAVE_WINDOWS_THREADS)

/*
 *  C4054: type cast from function pointer to data pointer
 *  C4055: type cast from data pointer to function pointer
 */

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4054 4055)
#endif /* defined(_MSC_VER) */

static int __stdcall call_once_callback(PINIT_ONCE init_once
    , PVOID param, PVOID* context)
{
    (void)init_once;
    (void)context;
    ((void (*)(void))param)();
    return 1;
}

static inline void call_once(once_flag* flag, void (*func)(void))
{
     InitOnceExecuteOnce((PINIT_ONCE)flag, call_once_callback
        , (void*)func, NULL);
}

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif /* defined(_MSC_VER) */

#endif /* defined(HAVE_POSIX_THREADS) */

/*
 *  7.26.3 Condition variable functions
 */

#if defined(HAVE_POSIX_THREADS)

static inline int cnd_broadcast(cnd_t* cond)
{
    if (pthread_cond_broadcast(cond) == 0)
        return thrd_success;
    return thrd_error;
}

static inline void cnd_destroy(cnd_t* cond)
{
    pthread_cond_destroy(cond);
}

static inline int cnd_init(cnd_t* cond)
{
    int res = pthread_cond_init(cond, NULL);
    if (res == 0)
        return thrd_success;
    return (res == ENOMEM) ? thrd_nomem : thrd_error;
}

static inline int cnd_signal(cnd_t* cond)
{
    if (pthread_cond_signal(cond) == 0)
        return thrd_success;
    return thrd_error;
}

static inline int cnd_timedwait(cnd_t* cond, mtx_t* mtx
    , const struct timespec* ts)
{
    int res = pthread_cond_timedwait(cond, &mtx->mtx, ts);
    if (res == 0)
        return thrd_success;
    return (res == ETIMEDOUT) ? thrd_timedout : thrd_error;
}

static inline int cnd_wait(cnd_t* cond, mtx_t* mtx)
{
    if (pthread_cond_wait(cond, &mtx->mtx) == 0)
        return thrd_success;
    return thrd_error;
}

#elif defined(HAVE_WINDOWS_THREADS)

static inline int cnd_broadcast(cnd_t* cond)
{
    WakeAllConditionVariable(cond);
    return thrd_success;
}

static inline void cnd_destroy(cnd_t* cond)
{
    (void)cond;
}

static inline int cnd_init(cnd_t* cond)
{
    InitializeConditionVariable(cond);
    return thrd_success;
}

static inline int cnd_signal(cnd_t* cond)
{
    WakeConditionVariable(cond);
    return thrd_success;
}

int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);

static inline int cnd_wait(cnd_t* cond, mtx_t* mtx)
{
    return cnd_timedwait(cond, mtx, NULL);
}

#endif /* defined(HAVE_POSIX_THREADS) */

/*
 *  7.26.4 Mutex functions
 */

#if defined(HAVE_POSIX_THREADS) && defined(HAVE_TIMEDLOCK)

static inline void mtx_destroy(mtx_t* mtx)
{
    pthread_mutex_destroy(&mtx->mtx);
}

static inline int mtx_init(mtx_t* mtx, int type)
{
    int res = 0;
    if (type & mtx_recursive)
    {
        pthread_mutexattr_t attr = { 0 };
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        res = pthread_mutex_init(&mtx->mtx, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    else
    {
        res = pthread_mutex_init(&mtx->mtx, NULL);
    }
    if (res == 0)
        return thrd_success;
    return (res == ENOMEM) ? thrd_nomem : thrd_error;
}

static inline int mtx_lock(mtx_t* mtx)
{
    if (pthread_mutex_lock(&mtx->mtx) == 0)
        return thrd_success;
    return thrd_error;
}

static inline int mtx_timedlock(mtx_t* mtx, const struct timespec* ts)
{
    int res = pthread_mutex_timedlock(&mtx->mtx, ts);
    if (res == 0)
        return thrd_success;
    return (res == ETIMEDOUT) ? thrd_busy : thrd_error;
}

static inline int mtx_trylock(mtx_t* mtx)
{
    int res = pthread_mutex_trylock(&mtx->mtx);
    if (res == 0)
        return thrd_success;
    return (res == EBUSY) ? thrd_busy : thrd_error;
}

static inline int mtx_unlock(mtx_t* mtx)
{
    if (pthread_mutex_unlock(&mtx->mtx) == 0)
        return thrd_success;
    return thrd_error;
}

#elif defined(HAVE_POSIX_THREADS) || defined(HAVE_WINDOWS_THREADS)

void mtx_destroy(mtx_t* mtx);

int mtx_init(mtx_t* mtx, int type);

int mtx_lock(mtx_t* mtx);

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts);

int mtx_trylock(mtx_t* mtx);

int mtx_unlock(mtx_t* mtx);

#endif /* defined(HAVE_POSIX_THREADS) ... */

/*
 *  7.26.5 Thread functions
 */

#if defined(HAVE_POSIX_THREADS)

static inline int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    int res = pthread_create(thr, 0, (void*(*)(void*))func, arg);
    if (res == 0)
        return thrd_success;
    return (res == ENOMEM) ? thrd_nomem : thrd_error;
}

static inline thrd_t thrd_current(void)
{
    return pthread_self();
}

static inline int thrd_detach(thrd_t thr)
{
    if (pthread_detach(thr) == 0)
        return thrd_success;
    return thrd_error;
}

static inline int thrd_equal(thrd_t thr0, thrd_t thr1)
{
    return pthread_equal(thr0, thr1);
}

static inline _Noreturn void thrd_exit(int res)
{
    pthread_exit((void*)(intptr_t)res);
}

static inline int thrd_join(thrd_t thr, int* res)
{
    void* tmp = NULL;
    if (pthread_join(thr, &tmp))
        return thrd_error;
    if (res)
        *res = (int)(intptr_t)tmp;
    return thrd_success;
}

static inline int thrd_sleep(const struct timespec* duration
    , struct timespec* remaining)
{
    int res = nanosleep(duration, remaining);
    if (res == 0)
        return thrd_success;
    return (res == EINTR) ? -1 : -2;
}

static inline void thrd_yield(void)
{
    sched_yield();
}

#elif defined(HAVE_WINDOWS_THREADS)

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

thrd_t thrd_current(void);

int thrd_detach(thrd_t thr);

int thrd_equal(thrd_t thr0, thrd_t thr1);

/*
 *  #1628: function declared with 'noreturn' does return
 *  -Winvalid-noreturn: function declared 'noreturn' should not return
 */

#if defined(__INTEL_COMPILER)
#   pragma warning(push)
#   pragma warning(disable: 1628)
#elif defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Winvalid-noreturn"
#endif /* defined(__INTEL_COMPILER) */

static inline _Noreturn void thrd_exit(int res)
{
    _endthreadex(res);
}

#if defined(__INTEL_COMPILER)
#   pragma warning(pop)
#elif defined(__clang__)
#   pragma clang diagnostic pop
#endif /* defined(__INTEL_COMPILER) */

int thrd_join(thrd_t thr, int* res);

/*
 *  Neither NtDelayExecution nor SetWaitableTimer deliver
 *  a higher resolution than 15.625 ms (and we don't want
 *  to use timeBeginPeriod/timeEndPeriod).
 */

static inline int thrd_sleep(const struct timespec* duration
    , struct timespec* remaining)
{
    __int64 msec = (duration->tv_sec * 1000L
        + duration->tv_nsec / 1000000L);
    Sleep((DWORD)msec);
    if (remaining)
    {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return 0;
}

static inline void thrd_yield(void)
{
    if (!SwitchToThread())
        Sleep(0);
}

#endif /* defined(HAVE_POSIX_THREADS) */

/*
 *  7.26.6 Thread-specific storage functions
 */

#if defined(HAVE_POSIX_THREADS)

static inline int tss_create(tss_t* key, tss_dtor_t dtor)
{
    if (pthread_key_create(key, dtor) == 0)
        return thrd_success;
    return thrd_error;
}

static inline void tss_delete(tss_t key)
{
    pthread_key_delete(key);
}

static inline void* tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

static inline int tss_set(tss_t key, void* val)
{
    if (pthread_setspecific(key, val) == 0)
        return thrd_success;
    return thrd_error;
}

#elif defined(HAVE_WINDOWS_THREADS)

int tss_create(tss_t* key, tss_dtor_t dtor);

void tss_delete(tss_t key);

void* tss_get(tss_t key);

int tss_set(tss_t key, void* val);

#endif /* defined(HAVE_POSIX_THREADS) */

#endif /* defined(HAVE_THREADS_H_WORKAROUND) */

#undef HAVE_THREADS_H_WORKAROUND

#endif /* __THREADS_H__ */
