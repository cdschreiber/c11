#include <c11/threads.h>

#if defined(HAVE_THREADS_H_WORKAROUND)

#include <assert.h>
#include <string.h>

#if defined(HAVE_POSIX_THREADS) && !defined(HAVE_TIMEDLOCK)

#if defined(__APPLE__)
#   include <pthread_spis.h>
#endif /* defined(__APPLE__) */

#define INVALID_THRDID ((pthread_t)-1)

/*
 *  7.26.4 Mutex functions
 */

void mtx_destroy(mtx_t* mtx)
{
    assert(mtx->count == 0);
    pthread_mutex_destroy(&mtx->mtx);
    if (mtx->type & mtx_timed)
        pthread_cond_destroy(&mtx->cond);
}

int mtx_init(mtx_t* mtx, int type)
{
    memset(mtx, 0, sizeof(*mtx));
    int res = 0;
    if (mtx->type & mtx_timed)
    {
        res = pthread_cond_init(&mtx->cond, NULL);
        if (res)
            return (res == ENOMEM) ? thrd_nomem : thrd_error;
#if defined(__APPLE__)
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpolicy_np(&attr
            , _PTHREAD_MUTEX_POLICY_FIRSTFIT);
        res = pthread_mutex_init(&mtx->mtx, &attr);
        pthread_mutexattr_destroy(&attr);
#else
        res = pthread_mutex_init(&mtx->mtx, NULL);
#endif /* defined(__APPLE__) */
        if (res)
            pthread_cond_destroy(&mtx->cond);
        mtx->thrdid = INVALID_THRDID;
        mtx->type = type;
    }
    else if (type & mtx_recursive)
    {
        pthread_mutexattr_t attr = { 0 };
        pthread_mutexattr_init(&attr);
#if defined(__APPLE__)
        pthread_mutexattr_setpolicy_np(&attr
            , _PTHREAD_MUTEX_POLICY_FIRSTFIT);
#endif /* defined(__APPLE__) */
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

static inline int handle_recursion(mtx_t* mtx)
{
    if (mtx->count++ == 0)
    {
        mtx->thrdid = pthread_self();
    }
    else if (!(mtx->type & mtx_recursive))
    {
        mtx->count--;
        return thrd_error;
    }
    return thrd_success;
}

int mtx_lock(mtx_t* mtx)
{
    if (mtx->type & mtx_timed)
    {
        if (!pthread_equal(mtx->thrdid, pthread_self()))
        {
            pthread_mutex_lock(&mtx->mtx);
            while (mtx->locked)
                pthread_cond_wait(&mtx->cond, &mtx->mtx);
            mtx->locked = 1;
            pthread_mutex_unlock(&mtx->mtx);
        }
        return handle_recursion(mtx);
    }
    if (pthread_mutex_lock(&mtx->mtx) == 0)
        return thrd_success;
    return thrd_error;
}

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts)
{
    if (!(mtx->type & mtx_timed))
        return thrd_error;
    if (!pthread_equal(mtx->thrdid, pthread_self()))
    {
        pthread_mutex_lock(&mtx->mtx);
        while (mtx->locked)
        {
            int res = pthread_cond_timedwait(&mtx->cond, &mtx->mtx, ts);
            if (res)
            {
                pthread_mutex_unlock(&mtx->mtx);
                return (res == ETIMEDOUT) ? thrd_busy : thrd_error;
            }
        }
        mtx->locked = 1;
        pthread_mutex_unlock(&mtx->mtx);
    }
    return handle_recursion(mtx);
}

int mtx_trylock(mtx_t* mtx)
{
    if (mtx->type & mtx_timed)
    {
        if (!pthread_equal(mtx->thrdid, pthread_self()))
        {
            pthread_mutex_lock(&mtx->mtx);
            if (mtx->locked)
            {
                pthread_mutex_unlock(&mtx->mtx);
                return thrd_busy;
            }
            mtx->locked = 1;
            pthread_mutex_unlock(&mtx->mtx);
        }
        return handle_recursion(mtx);
    }
    int res = pthread_mutex_trylock(&mtx->mtx);
    if (res == 0)
        return thrd_success;
    return (res == EBUSY) ? thrd_busy : thrd_error;
}

int mtx_unlock(mtx_t* mtx)
{
    if (mtx->type & mtx_timed)
    {
        assert(mtx->count && pthread_equal(mtx->thrdid, pthread_self()));
        if (mtx->count-- == 1)
        {
            mtx->thrdid = INVALID_THRDID;
            pthread_mutex_lock(&mtx->mtx);
            mtx->locked = 0;
            pthread_cond_signal(&mtx->cond);
            pthread_mutex_unlock(&mtx->mtx);
        }
        return thrd_success;
    }
    if (pthread_mutex_unlock(&mtx->mtx) == 0)
        return thrd_success;
    return thrd_error;
}

#elif defined(HAVE_WINDOWS_THREADS)

#include <stdlib.h>

#define INVALID_THRDID ((DWORD)-1)

/*
 *  7.26.3 Condition variable functions
 */

int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts)
{
    mtx->thrdid = INVALID_THRDID;
	mtx->count--;
    DWORD msec = INFINITE;
    if (ts)
    {
        struct timespec now = { 0 };
        timespec_get(&now, TIME_UTC);
        __int64 diff = (ts->tv_sec * 1000000000L + ts->tv_nsec)
            - (now.tv_sec * 1000000000L + now.tv_nsec);
        diff = (diff > 0) ? diff : 0;
        msec = (DWORD)(diff / 1000000LL);
    }
    int ret = thrd_success;
    if (!SleepConditionVariableSRW(cond, &mtx->srwlock, msec, 0))
    {
        if (GetLastError() == ERROR_TIMEOUT)
            ret = thrd_timedout;
        else
            ret = thrd_error;
    }
    mtx->thrdid = GetCurrentThreadId();
	mtx->count++;
    return ret;
}

/*
 *  7.26.4 Mutex functions
 */

void mtx_destroy(mtx_t* mtx)
{
    assert(mtx->count == 0);
    (void)mtx;
}

int mtx_init(mtx_t* mtx, int type)
{
    memset(mtx, 0, sizeof(*mtx));
    InitializeSRWLock(&mtx->srwlock);
    InitializeConditionVariable(&mtx->cv);
    mtx->thrdid = INVALID_THRDID;
    mtx->type = type;
    return thrd_success;
}

static inline int handle_recursion(mtx_t* mtx)
{
    if (mtx->count++ == 0)
    {
        mtx->thrdid = GetCurrentThreadId();
    }
    else if (!(mtx->type & mtx_recursive))
    {
        mtx->count--;
        return thrd_error;
    }
    return thrd_success;
}

int mtx_lock(mtx_t* mtx)
{
    if (mtx->thrdid != GetCurrentThreadId())
    {
        if (mtx->type & mtx_timed)
        {
            AcquireSRWLockExclusive(&mtx->srwlock);
            while (mtx->locked)
            {
                if (!SleepConditionVariableSRW(&mtx->cv
                    , &mtx->srwlock, INFINITE, 0))
                {
                    ReleaseSRWLockExclusive(&mtx->srwlock);
                    return thrd_error;
                }
            }
            mtx->locked = 1;
            ReleaseSRWLockExclusive(&mtx->srwlock);
        }
        else
        {
            AcquireSRWLockExclusive(&mtx->srwlock);
        }
    }
    return handle_recursion(mtx);
}

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts)
{
    if (!(mtx->type & mtx_timed))
        return thrd_error;
    if (mtx->thrdid != GetCurrentThreadId())
    {
        struct timespec now = { 0 };
        __int64 diff = (ts->tv_sec * 1000000000L + ts->tv_nsec)
            - (now.tv_sec * 1000000000L + now.tv_nsec);
        diff = (diff > 0) ? diff : 0;
        DWORD msec = (DWORD)(diff / 1000000LL);
        AcquireSRWLockExclusive(&mtx->srwlock);
        while (mtx->locked)
        {
            if (!SleepConditionVariableSRW(&mtx->cv
                , &mtx->srwlock, msec, 0))
            {
                ReleaseSRWLockExclusive(&mtx->srwlock);
                if (GetLastError() == ERROR_TIMEOUT)
                    return thrd_timedout;
                else
                    return thrd_error;
            }
        }
        mtx->locked = 1;
        ReleaseSRWLockExclusive(&mtx->srwlock);
    }
    return handle_recursion(mtx);
}

int mtx_trylock(mtx_t* mtx)
{
    if (mtx->thrdid != GetCurrentThreadId())
    {
        if (mtx->type & mtx_timed)
        {
            AcquireSRWLockExclusive(&mtx->srwlock);
            if (mtx->locked)
            {
                ReleaseSRWLockExclusive(&mtx->srwlock);
                return thrd_busy;
            }
            mtx->locked = 1;
            ReleaseSRWLockExclusive(&mtx->srwlock);
        }
        else
        {
            if (!TryAcquireSRWLockExclusive(&mtx->srwlock))
                return thrd_busy;
        }
    }
    return handle_recursion(mtx);
}

int mtx_unlock(mtx_t* mtx)
{
    assert(mtx->count && mtx->thrdid == GetCurrentThreadId());
    if (mtx->count-- == 1)
    {
        mtx->thrdid = INVALID_THRDID;
        if (mtx->type & mtx_timed)
        {
            AcquireSRWLockExclusive(&mtx->srwlock);
            mtx->locked = 0;
            WakeConditionVariable(&mtx->cv);
            ReleaseSRWLockExclusive(&mtx->srwlock);
        }
        else
        {
            ReleaseSRWLockExclusive(&mtx->srwlock);
        }
    }
    return thrd_success;
}

/*
 *  7.26.5 Thread functions
 */

struct thread
{
    _Alignas(CACHELINE_SIZE) HANDLE hnd;
    struct
    {
        void** values;
        SIZE_T capacity;
    } tss;
    DWORD id;
    LONG state;
};

struct thread_param
{
    thrd_start_t proc;
    void* data;
    HANDLE entry_event;
    struct thread* thrd;
};

enum
{
    st_running = 1,
    st_finished,
    st_joined,
    st_detached
};

static once_flag g_once_flag = ONCE_FLAG_INIT;

static DWORD g_thread_key = FLS_OUT_OF_INDEXES;

static struct thread* g_main_thread = NULL;

static void call_tss_destructors(struct thread*);

static void __stdcall on_thread_exit(void*);

static void on_process_exit(void);

/*
 *  The Old New Thing - Raymond Chen
 *  A better way to clean up.
 *  https://devblogs.microsoft.com/oldnewthing/20191011-00
 */

static void on_process_enter(void)
{
    g_thread_key = FlsAlloc(on_thread_exit);
    if (g_thread_key == FLS_OUT_OF_INDEXES)
        abort();
    atexit(on_process_exit);
}

static inline void set_current_thread(struct thread* thrd)
{
    call_once(&g_once_flag, on_process_enter);
    FlsSetValue(g_thread_key, thrd);
}

static inline struct thread* get_current_thread(void)
{
    call_once(&g_once_flag, on_process_enter);
    return FlsGetValue(g_thread_key);
}

static unsigned __stdcall start_thread(void* arg)
{
    struct thread_param* param = arg;
    thrd_start_t proc = param->proc;
    void* data = param->data;
    set_current_thread(param->thrd);
    SetEvent(param->entry_event);
    return proc(data);
}

int thrd_create(thrd_t* thr, int (*func)(void*), void* arg)
{
    struct thread* thrd = _aligned_malloc(sizeof(*thrd), CACHELINE_SIZE);
    if (thrd == NULL)
        return thrd_nomem;
    memset(thrd, 0, sizeof(*thrd));
    int rollback = 1;
    HANDLE entry_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (entry_event == NULL)
        goto failure;
    rollback = 2;
    struct thread_param param = { 0 };
    param.proc = func;
    param.data = arg;
    param.entry_event = entry_event;
    param.thrd = thrd;
    unsigned int id = 0;
    uintptr_t hnd = _beginthreadex(NULL, 0, start_thread
        , &param, CREATE_SUSPENDED, &id);
    if (hnd == 0)
        goto failure;
    rollback = 3;
    thrd->hnd = (HANDLE)hnd;
    thrd->id = id;
    thrd->state = st_running;
    if (ResumeThread(thrd->hnd) == (DWORD)-1)
        goto failure;
    if (WaitForSingleObject(entry_event, INFINITE) == WAIT_FAILED)
        goto failure;
    CloseHandle(entry_event);
    *thr = (thrd_t)thrd;
    return thrd_success;
failure:
    switch (rollback)
    {
    case 3: CloseHandle(thrd->hnd);
    case 2: CloseHandle(entry_event);
    case 1: _aligned_free(thrd);
    }
    return thrd_error;
}

thrd_t thrd_current(void)
{
    struct thread* thrd = get_current_thread();
    if (thrd == NULL)
    {
        if (g_main_thread)
            abort();
        thrd = _aligned_malloc(sizeof(*thrd), CACHELINE_SIZE);
        if (thrd == NULL)
            abort();
        memset(thrd, 0, sizeof(*thrd));
        if (!DuplicateHandle(GetCurrentProcess()
            , GetCurrentThread(), GetCurrentProcess()
            , &thrd->hnd, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            _aligned_free(thrd);
            abort();
        }
        thrd->id = GetThreadId(thrd->hnd);
        thrd->state = st_detached;
        g_main_thread = thrd;
        set_current_thread(thrd);
    }
    return (thrd_t)thrd;
}

int thrd_detach(thrd_t thr)
{
    struct thread* thrd = (struct thread*)thr;
    LONG state = InterlockedCompareExchange(&thrd->state
        , st_detached, st_running);
    assert(state == st_running || state == st_finished);
    if (!CloseHandle(thrd->hnd))
        return thrd_error;
    if (state == st_finished)
    {
        free(thrd->tss.values);
        _aligned_free(thrd);
    }
    return thrd_success;
}

int thrd_equal(thrd_t thr0, thrd_t thr1)
{
    struct thread* lhs = (struct thread*)thr0;
    struct thread* rhs = (struct thread*)thr1;
    return (lhs->id == rhs->id);
}

int thrd_join(thrd_t thr, int* res)
{
    struct thread* thrd = (struct thread*)thr;
    if (GetCurrentThreadId() == thrd->id)
        return thrd_error;
    LONG state = InterlockedCompareExchange(&thrd->state
        , st_joined, st_running);
    if (state == st_detached)
        return thrd_error;
    assert(state == st_running || state == st_finished);
    if (WaitForSingleObject(thrd->hnd, INFINITE) == WAIT_FAILED)
        return thrd_error;
    if (res)
    {
        if (!GetExitCodeThread(thrd->hnd, (LPDWORD)res))
            return thrd_error;
    }
    if (!CloseHandle(thrd->hnd))
        return thrd_error;
    if (thrd->tss.values)
        free(thrd->tss.values);
    _aligned_free(thrd);
    return thrd_success;
}

/*
 *  7.26.6 Thread-specific storage functions
 */

static struct
{
    SRWLOCK lock;
    tss_dtor_t* dtors;
    size_t capacity;
    size_t size;
    size_t next_avail;
} g_tss_sweeper = { SRWLOCK_INIT, NULL, 0, 0, 0 };

#define INVALID_TSS_KEY ((size_t)-1)
#define INVALID_TSS_DTOR ((tss_dtor_t)1)

static size_t find_available_key(void)
{
    for (size_t i = g_tss_sweeper.next_avail
        ; i != g_tss_sweeper.size; i++)
    {
        if (g_tss_sweeper.dtors[i] == NULL)
            return i;
    }
    for (size_t i = 0; i != g_tss_sweeper.next_avail; i++)
    {
        if (g_tss_sweeper.dtors[i] == NULL)
            return i;
    }
    return INVALID_TSS_KEY;
}

static size_t allocate_key(void)
{
    if (g_tss_sweeper.size == g_tss_sweeper.capacity)
    {
        size_t capacity = g_tss_sweeper.capacity + 8;
        size_t nbtotal = capacity * sizeof(void*);
        tss_dtor_t* addr = realloc(g_tss_sweeper.dtors, nbtotal);
        if (addr == NULL)
            return INVALID_TSS_KEY;
        g_tss_sweeper.dtors = addr;
        g_tss_sweeper.capacity = capacity;
    }
    size_t key = g_tss_sweeper.size++;
    g_tss_sweeper.next_avail = g_tss_sweeper.size;
    return key;
}

int tss_create(tss_t* key, tss_dtor_t dtor)
{
    AcquireSRWLockExclusive(&g_tss_sweeper.lock);
    size_t k = find_available_key();
    if (k == INVALID_TSS_KEY)
        k = allocate_key();
    if (k == INVALID_TSS_KEY)
    {
        ReleaseSRWLockExclusive(&g_tss_sweeper.lock);
        return thrd_nomem;
    }
    *(size_t*)key = k;
    if (dtor)
        g_tss_sweeper.dtors[k] = dtor;
    else
        g_tss_sweeper.dtors[k] = INVALID_TSS_DTOR;
    ReleaseSRWLockExclusive(&g_tss_sweeper.lock);
    return thrd_success;
}

void tss_delete(tss_t key)
{
    size_t k = (size_t)key;
    assert(k < g_tss_sweeper.size);
    AcquireSRWLockExclusive(&g_tss_sweeper.lock);
    g_tss_sweeper.dtors[k] = NULL;
    if (g_tss_sweeper.next_avail > k)
        g_tss_sweeper.next_avail = k;
    ReleaseSRWLockExclusive(&g_tss_sweeper.lock);
}

void* tss_get(tss_t key)
{
    struct thread* thrd = get_current_thread();
    assert(thrd);
    size_t k = (size_t)key;
    if (k >= thrd->tss.capacity)
        return NULL;
    return thrd->tss.values[k];
}

int tss_set(tss_t key, void* val)
{
    struct thread* thrd = get_current_thread();
    assert(thrd);
    size_t k = (size_t)key;
    if (k >= thrd->tss.capacity)
    {
        size_t capacity = (((k + 1) + 7) & ~7);
        size_t nbtotal = capacity * sizeof(void*);
        void** addr = realloc(thrd->tss.values, nbtotal);
        if (addr == NULL)
            return thrd_nomem;
        thrd->tss.values = addr;
        thrd->tss.capacity = capacity;
    }
    thrd->tss.values[k] = val;
    return thrd_success;
}

static void call_tss_destructors(struct thread* thrd)
{
    int stop = 0;
    for (int i = 0; !stop && i < TSS_DTOR_ITERATIONS; i++)
    {
        stop = 1;
        for (size_t k = 0; k < thrd->tss.capacity; k++)
        {
            void* val = thrd->tss.values[k];
            if (val)
            {
                AcquireSRWLockShared(&g_tss_sweeper.lock);
                tss_dtor_t dtor = g_tss_sweeper.dtors[k];
                if (!(dtor == NULL || dtor == INVALID_TSS_DTOR))
                {
                    thrd->tss.values[k] = NULL;
                    dtor(val);
                    stop = 0;
                }
                ReleaseSRWLockShared(&g_tss_sweeper.lock);
            }
        }
    }
}

static void __stdcall on_thread_exit(void* arg)
{
    assert(arg);
    struct thread* thrd = arg;
    call_tss_destructors(thrd);
    set_current_thread(NULL);
    LONG state = InterlockedCompareExchange(&thrd->state
        , st_finished, st_running);
    assert(state != st_finished);
    if (state == st_detached)
    {
        if (thrd->tss.values)
            free(thrd->tss.values);
        _aligned_free(thrd);
    }
}

static void on_process_exit(void)
{
    if (g_main_thread)
        call_tss_destructors(g_main_thread);
    FlsFree(g_thread_key);
}

#endif /* defined(HAVE_POSIX_THREADS) ... */

#endif /* defined(HAVE_THREADS_H_WORKAROUND) */
