#ifndef THREADING_H
#define THREADING_H

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define THREADING_UNDEF_WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef THREADING_UNDEF_WIN32_LEAN_AND_MEAN
#undef THREADING_UNDEF_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#endif
#endif

#if defined(__has_include) && __has_include(<stdbool.h>)
#include <stdbool.h>
#else
#define bool int
#define true 1
#define false 0
#endif

#if !(defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201102L)) && !defined(_Thread_local)
 #if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__SUNPRO_CC) || defined(__IBMCPP__)
  #define _Thread_local __thread
 #else
  #define _Thread_local __declspec(thread)
 #endif
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && (((__GNUC__ << 8) | __GNUC_MINOR__) < ((4 << 8) | 9))
 #define _Thread_local __thread
#endif

#if defined(_MSC_VER) && !defined(__clang__) && !defined(_Noreturn)
#define _Noreturn __declspec(noreturn)
#define no_return _Noreturn
#endif


#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>

typedef pthread_rwlock_t rwlock_t;
typedef pthread_rwlockattr_t rwlockattr_t;

#define rwlock_init pthread_rwlock_init
#define rwlock_rdlock pthread_rwlock_rdlock
#define rwlock_wrlock pthread_rwlock_wrlock
#define rwlock_unlock pthread_rwlock_unlock
#define rwlock_tryrdlock pthread_rwlock_tryrdlock
#define rwlock_trywrlock pthread_rwlock_trywrlock
#define rwlock_destroy pthread_rwlock_destroy
#define rwlockattr_init pthread_rwlockattr_init
#define rwlockattr_destroy pthread_rwlockattr_destroy

#define RWLOCK_INITIALIZER PTHREAD_RWLOCK_INITIALIZER

#else

typedef struct {
    SRWLOCK lock;
    bool exclusive;
} rwlock_t;

typedef void *rwlockattr_t;

int rwlock_init(rwlock_t *rwlock, const rwlockattr_t *attr) {
    (void)attr;
    if (rwlock == NULL)
        return 1;
    InitializeSRWLock(&(rwlock->lock));
    rwlock->exclusive = false;
    return 0;
}

#define RWLOCK_INITIALIZER {.lock = SRWLOCK_INIT, .exclusive = false}

#define rwlockattr_init(attr) (void)attr
#define rwlockattr_destroy(attr) (void)attr

int rwlock_rdlock(rwlock_t *rwlock) {
    if (rwlock == NULL)
        return 1;
    AcquireSRWLockShared(&(rwlock->lock));
}

int rwlock_tryrdlock(rwlock_t *rwlock) {
    if (rwlock == NULL)
        return 1;
    return !TryAcquireSRWLockShared(&(rwlock->lock));
}

int rwlock_wrlock(rwlock_t *rwlock) {
    if (rwlock == NULL)
        return 1;
    AcquireSRWLockExclusive(&(rwlock->lock));
    rwlock->exclusive = true;
    return 0;
}

int rwlock_trywrlock(rwlock_t *rwlock) {
    BOOLEAN ret;

    if (rwlock == NULL)
        return 1;

    ret = TryAcquireSRWLockExclusive(&(rwlock->lock));
    if (ret)
        rwlock->exclusive = true;
    return ret;
}


int rwlock_unlock(rwlock_t *rwlock) {
    if (rwlock == NULL)
        return 1;

    if (rwlock->exclusive) {
        rwlock->exclusive = false;
        ReleaseSRWLockExclusive(&(rwlock->lock));
    } else {
        ReleaseSRWLockShared(&(rwlock->lock));
    }
    return 0;
}

int rwlock_destroy(rwlock_t *rwlock) {
    (void)rwlock;
    return 0;
}


#endif


#if defined(__has_include) && __has_include(<threads.h>) && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <process.h>
#else
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  #define THREADING_NO_RETURN _Noreturn
#elif defined(__GNUC__)
  #define THREADING_NO_RETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
    #define THREADING_NO_RETURN __declspec(noreturn)
#else
    #define THREADING_NO_RETURN
#endif

// Define thread types
#if defined(_WIN32) || defined(_WIN64)
typedef HANDLE thrd_t;
typedef struct {
    union {
        CRITICAL_SECTION cs;  /* Critical section handle */
        HANDLE mut;           /* Mutex handle */
    } handle;                 /* Mutex handle */
    bool already_locked;      /* true if the mtx is already locked */
    bool recursive;           /* true if the mtx is recursive */
    bool timed;               /* true if the mtx is timed */
} mtx_t;
typedef struct {
    HANDLE events[2];                  /* Signal and broadcast event HANDLEs. */
    unsigned int num_waiters;           /* Count of the number of waiters. */
    CRITICAL_SECTION num_waiters_lock;  /* Serialize access to num_waiters. */
} cnd_t;
typedef DWORD tss_t;
#else
typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
#endif

// Thread return values
enum {
    thrd_error = 0,
    thrd_success = 1,
    thrd_timedout = 2,
    thrd_busy = 3,
    thrd_nomem = 4
};

// Mutex types
enum {
    mtx_plain = 0,
    mtx_timed = 1,
    mtx_recursive = 2
};

// Thread function type
typedef int (*thrd_start_t)(void *);

typedef void (*tss_dtor_t)(void *);

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    INIT_ONCE once;
#else
    pthread_once_t once;
#endif
} once_flag;

#define ONCE_FLAG_INIT {0}


#if defined(_WIN32) || defined(_WIN64)
struct _tss_data {
  void* value;
  tss_t key;
  struct _tss_data* next;
};

#ifndef TSS_DESTRUCTOR_NUM_PASSES
#define TSS_DESTRUCTOR_NUM_PASSES (4)
#define TSS_DESTRUCTOR_NUM_PASSES_DEFINED
#endif

#define THREADING_WINDOWS_TLS_MAX_SLOTS 1088
static tss_dtor_t _tss_destructors[THREADING_WINDOWS_TLS_MAX_SLOTS] = { NULL, };
#undef THREADING_WINDOWS_TLS_MAX_SLOTS

static _Thread_local struct _tss_data* _tss_head = NULL;
static _Thread_local struct _tss_data* _tss_tail = NULL;

static void _tss_cleanup (void) {
    struct _tss_data *data = NULL;
    bool found = true;

    for (int i = 0; i < TSS_DESTRUCTOR_NUM_PASSES && found; i++) {
        found = false;
        for (data = _tss_head; data != NULL; data = data->next) {
            if (data->value != NULL) {
                void *value = data->value;
                data->value = NULL;

                if (_tss_destructors[data->key] != NULL) {
                    found = true;
                    _tss_destructors[data->key](value);
                }
            }
        }
    }

    while (_tss_head != NULL) {
        data = _tss_head->next;
        free(_tss_head);
        _tss_head = data;
    }
    _tss_head = NULL;
    _tss_tail = NULL;
}

#ifdef TSS_DESTRUCTOR_NUM_PASSES_DEFINED
#undef TSS_DESTRUCTOR_NUM_PASSES
#undef TSS_DESTRUCTOR_NUM_PASSES_DEFINED
#endif

static void NTAPI _tss_callback(PVOID h, DWORD reason, PVOID pv) {
    (void)h;
    (void)pv;

    if (_tss_head != NULL && (reason == DLL_THREAD_DETACH || reason == DLL_PROCESS_DETACH)) {
        _tss_cleanup();
    }
}

#if defined(_MSC_VER)
    #ifdef _M_X64
        #pragma const_seg(".CRT$XLB")
    #else
        #pragma data_seg(".CRT$XLB")
    #endif
    PIMAGE_TLS_CALLBACK p_thread_callback = _tss_callback;
    #ifdef _M_X64
        #pragma data_seg()
    #else
        #pragma const_seg()
    #endif
#else
    PIMAGE_TLS_CALLBACK p_thread_callback __attribute__((section(".CRT$XLB"))) = _tss_callback;
#endif


typedef struct {
    thrd_start_t func;
    void* arg;
} _thrd_wrapper_data;

static DWORD WINAPI _thrd_wrapper_func(LPVOID arg) {
    _thrd_wrapper_data* data = (_thrd_wrapper_data*)arg;
    thrd_start_t func = data->func;
    void* func_arg = data->arg;
    free(data);

    DWORD ret = (DWORD)func(func_arg);
    if (_tss_head != NULL) {
        _tss_cleanup();
    }
    return ret;
}

#endif

// Thread creation
static int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
#if defined(_WIN32) || defined(_WIN64)
    _thrd_wrapper_data* data = (_thrd_wrapper_data*)malloc(sizeof(_thrd_wrapper_data));
    if (data == NULL) {
        return thrd_nomem;
    }
    data->func = func;
    data->arg = arg;

    *thr = CreateThread(NULL, 0, _thrd_wrapper_func, (LPVOID)data, 0, NULL);
    return *thr ? thrd_success : thrd_error;
#else
    return pthread_create(thr, NULL, (void *(*)(void *))func, arg) == 0 ? thrd_success : thrd_error;
#endif
}

// Thread join
static int thrd_join(thrd_t thr, int *res) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD ret = WaitForSingleObject(thr, INFINITE);
    if (ret == WAIT_FAILED) {
        return thrd_error;
    }
    if (res != NULL) {
        DWORD code;
        if (GetExitCodeThread(thr, &code) == 0) {
            return thrd_error;
        }
        *res = (int)code;
    }
    CloseHandle(thr);
    return thrd_success;
#else
    void *retval;
    if (pthread_join(thr, &retval) == 0) {
        if (res) *res = (int)(intptr_t)retval;
        return thrd_success;
    }
    return thrd_error;
#endif
}

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
#if defined(_WIN32) || defined(_WIN64)
    struct timespec start;
    timespec_get(&start, TIME_UTC);
    DWORD t = SleepEx((DWORD)(duration->tv_sec * 1000 +
                      duration->tv_nsec / 1000000 +
                      (((duration->tv_nsec % 1000000) == 0) ? 0 : 1)),
                      true);
    if (t == 0) {
        return 0;
    } else {
        if (remaining != NULL) {
            timespec_get(remaining, TIME_UTC);
            remaining->tv_sec -= start.tv_sec;
            remaining->tv_nsec -= start.tv_nsec;
            if (remaining->tv_nsec < 0) {
                remaining->tv_nsec += 1000000000;
                remaining->tv_sec -= 1;
            }
        }
        return (t == WAIT_IO_COMPLETION) ? -1 : -2;
    }
#else
    int res = nanosleep(duration, remaining);
    if (res == 0) {
        return 0;
    } else if (errno == EINTR) {
        return -1;
    } else {
        return -2;
    }
#endif
}

// Thread detach
static int thrd_detach(thrd_t thr) {
#if defined(_WIN32) || defined(_WIN64)
    // https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
    return CloseHandle(thr) != 0 ? thrd_success : thrd_error;
#else
    return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
#endif
}

// Thread exit
THREADING_NO_RETURN static void thrd_exit(int res) {
#if defined(_WIN32) || defined(_WIN64)
    if (_tss_head != NULL) {
        _tss_cleanup();
    }
    ExitThread((DWORD)res);
#else
    pthread_exit((void *)(intptr_t)res);
#endif
}

// Thread yield
static void thrd_yield(void) {
#if defined(_WIN32) || defined(_WIN64)
    (void)SwitchToThread();
#else
    sched_yield();
#endif
}

// Thread current
static thrd_t thrd_current(void) {
#if defined(_WIN32) || defined(_WIN64)
    return GetCurrentThread();
#else
    return pthread_self();
#endif
}

// Thread equal
static int thrd_equal(thrd_t lhs, thrd_t rhs) {
#if defined(_WIN32) || defined(_WIN64)
    return GetThreadId(lhs) == GetThreadId(rhs);
#else
    return pthread_equal(lhs, rhs);
#endif
}


// Mutex initialization
static int mtx_init(mtx_t *mtx, int type) {
#if defined(_WIN32) || defined(_WIN64)
    mtx->already_locked = false;
    mtx->recursive = type == mtx_recursive ? 1 : 0;
    mtx->timed = type == mtx_timed ? 1 : 0;
    if (!mtx->timed) {
        InitializeCriticalSection(&(mtx->handle.cs));
    } else {
        mtx->handle.mut = CreateMutex(NULL, false, NULL);
        if (mtx->handle.mut == NULL) {
            return thrd_error;
        }
    }
    return thrd_success;
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (type == mtx_recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    int ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret == 0 ? thrd_success : thrd_error;
#endif
}

// Mutex lock
static int mtx_lock(mtx_t *mtx) {
#if defined(_WIN32) || defined(_WIN64)
    if (!mtx->timed) {
        EnterCriticalSection(&(mtx->handle.cs));
    } else {
        DWORD result = WaitForSingleObject(mtx->handle.mut, INFINITE);
        if (result != WAIT_OBJECT_0) {
            return thrd_error;
        }
    }

    if (!mtx->recursive) {
        while (mtx->already_locked) {
            Sleep(1);
        }
        mtx->already_locked = true;
    }
    return thrd_success;
#else
    return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
#endif
}

int mtx_timedlock(mtx_t *mtx, const struct timespec *ts) {
#if defined(_WIN32) || defined(_WIN64)
    if (!mtx->timed) {
        return thrd_error;
    }

    struct timespec now;
    timespec_get(&now, TIME_UTC);

    DWORD timeout_ms = 0;

    if ((now.tv_sec < ts->tv_sec) || ((now.tv_sec == ts->tv_sec) && (now.tv_nsec < ts->tv_nsec))) {
        timeout_ms = (DWORD)(ts->tv_sec - now.tv_sec) * 1000;
        timeout_ms += (ts->tv_nsec - now.tv_nsec) / 1000000;
        timeout_ms += 1;
    }

    DWORD result = WaitForSingleObject(mtx->handle.mut, timeout_ms);
    if (result == WAIT_OBJECT_0) {
        if (!mtx->recursive) {
            while (mtx->already_locked) {
                Sleep(1);
            }
            mtx->already_locked = true;
        }
        return thrd_success;
    } else if (result == WAIT_TIMEOUT) {
        return thrd_timedout;
    } else {
        return thrd_error;
    }
#elif defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L) && defined(_POSIX_THREADS) && (_POSIX_THREADS >= 200112L)
    int ret = pthread_mutex_timedlock(mtx, ts);
    if (ret == 0) {
        return thrd_success;
    } else if (ret == ETIMEDOUT) {
        return thrd_timedout;
    } else {
        return thrd_error;
    }
#else
    int rc = 0;
    struct timespec cur, dur;

    /* Try to acquire the lock and, if we fail, sleep for 5ms. */
    while ((rc = pthread_mutex_trylock(mtx)) == EBUSY) {
        timespec_get(&cur, TIME_UTC);

        if ((cur.tv_sec > ts->tv_sec) || ((cur.tv_sec == ts->tv_sec) && (cur.tv_nsec >= ts->tv_nsec)))
        {
            break;
        }

        dur.tv_sec = ts->tv_sec - cur.tv_sec;
        dur.tv_nsec = ts->tv_nsec - cur.tv_nsec;
        if (dur.tv_nsec < 0)
        {
            dur.tv_sec--;
            dur.tv_nsec += 1000000000;
        }

        if ((dur.tv_sec != 0) || (dur.tv_nsec > 5000000))
        {
            dur.tv_sec = 0;
            dur.tv_nsec = 5000000;
        }

        nanosleep(&dur, NULL);
    }

    if (rc == 0) {
        return thrd_success;
    } else if (rc == ETIMEDOUT || rc == EBUSY) {
        return thrd_timedout;
    } else {
        return thrd_error;
    }    
#endif
}

// Mutex trylock
static int mtx_trylock(mtx_t *mtx) {
#if defined(_WIN32) || defined(_WIN64)
    int ret = thrd_success;
    if (!mtx->timed) {
        ret = TryEnterCriticalSection(&(mtx->handle.cs)) ? thrd_success : thrd_busy;
    } else {
        ret = (WaitForSingleObject(mtx->handle.mut, 0) == WAIT_OBJECT_0) ? thrd_success : thrd_busy;
    }

    if ((!mtx->recursive) && (ret == thrd_success)) {
        if (mtx->already_locked) {
            LeaveCriticalSection(&(mtx->handle.cs));
            ret = thrd_busy;
        } else {
            mtx->already_locked = true;
        }
    }
    return ret;
#else
    return pthread_mutex_trylock(mtx) == 0 ? thrd_success : thrd_busy;
#endif
}


// Mutex unlock
static int mtx_unlock(mtx_t *mtx) {
#if defined(_WIN32) || defined(_WIN64)
    mtx->already_locked = false;
    if (!mtx->timed) {
        LeaveCriticalSection(&(mtx->handle.cs));
    } else {
        if (!ReleaseMutex(mtx->handle.mut)) {
            return thrd_error;
        }
    }
    return thrd_success;
#else
    return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
#endif
}

// Mutex destroy
static void mtx_destroy(mtx_t *mtx) {
#if defined(_WIN32) || defined(_WIN64)
    if (!mtx->timed) {
        DeleteCriticalSection(&(mtx->handle.cs));
    } else {
        CloseHandle(mtx->handle.mut);
    }
#else
    pthread_mutex_destroy(mtx);
#endif
}


#if defined(_WIN32) || defined(_WIN64)
#define _CONDITION_EVENT_ONE 0
#define _CONDITION_EVENT_ALL 1

#endif


// Condition variable initialization
static int cnd_init(cnd_t *cond) {
#if defined(_WIN32) || defined(_WIN64)
    cond->num_waiters = 0;

    /* Init critical section */
    InitializeCriticalSection(&cond->num_waiters_lock);

    /* Init events */
    cond->events[_CONDITION_EVENT_ONE] = CreateEvent(NULL, false, false, NULL);
    if (cond->events[_CONDITION_EVENT_ONE] == NULL) {
        cond->events[_CONDITION_EVENT_ALL] = NULL;
        return thrd_error;
    }
    cond->events[_CONDITION_EVENT_ALL] = CreateEvent(NULL, true, false, NULL);
    if (cond->events[_CONDITION_EVENT_ALL] == NULL) {
        CloseHandle(cond->events[_CONDITION_EVENT_ONE]);
        cond->events[_CONDITION_EVENT_ONE] = NULL;
        return thrd_error;
    }

    return thrd_success;
#else
    return pthread_cond_init(cond, NULL) == 0 ? thrd_success : thrd_error;
#endif
}

// Condition variable destruction
static void cnd_destroy(cnd_t *cond) {
#if defined(_WIN32) || defined(_WIN64)
    if (cond->events[_CONDITION_EVENT_ONE] != NULL) {
        CloseHandle(cond->events[_CONDITION_EVENT_ONE]);
    }
    if (cond->events[_CONDITION_EVENT_ALL] != NULL) {
        CloseHandle(cond->events[_CONDITION_EVENT_ALL]);
    }
    DeleteCriticalSection(&cond->num_waiters_lock);
#else
    pthread_cond_destroy(cond);
#endif
}



#if defined(_WIN32) || defined(_WIN64)
static int _cnd_timedwait_win32(cnd_t *cond, mtx_t *mtx, DWORD timeout)
{
    DWORD result;
    int lastWaiter;

    /* Increment number of waiters */
    EnterCriticalSection(&cond->num_waiters_lock);
    ++cond->num_waiters;
    LeaveCriticalSection(&cond->num_waiters_lock);

    /* Release the mutex while waiting for the condition (will decrease
        the number of waiters when done)... */
    mtx_unlock(mtx);

    /* Wait for either event to become signaled due to cnd_signal() or
        cnd_broadcast() being called */
    result = WaitForMultipleObjects(2, cond->events, FALSE, timeout);
    if (result == WAIT_TIMEOUT) {
        /* The mutex is locked again before the function returns, even if an error occurred */
        mtx_lock(mtx);
        return thrd_timedout;
    }
    else if (result == WAIT_FAILED) {
        /* The mutex is locked again before the function returns, even if an error occurred */
        mtx_lock(mtx);
        return thrd_error;
    }

    /* Check if we are the last waiter */
    EnterCriticalSection(&cond->num_waiters_lock);
    --cond->num_waiters;
    lastWaiter = (result == (WAIT_OBJECT_0 + _CONDITION_EVENT_ALL)) &&
                 (cond->num_waiters == 0);
    LeaveCriticalSection(&cond->num_waiters_lock);

    /* If we are the last waiter to be notified to stop waiting, reset the event */
    if (lastWaiter)
    {
        if (ResetEvent(cond->events[_CONDITION_EVENT_ALL]) == 0)
        {
            /* The mutex is locked again before the function returns, even if an error occurred */
            mtx_lock(mtx);
            return thrd_error;
        }
    }

    /* Re-acquire the mutex */
    mtx_lock(mtx);

    return thrd_success;
}
#endif


// Condition variable wait
static int cnd_wait(cnd_t *cond, mtx_t *mtx) {
#if defined(_WIN32) || defined(_WIN64)
    return _cnd_timedwait_win32(cond, mtx, INFINITE);
#else
    return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
#endif
}

// Condition variable timed wait
static int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts) {
#if defined(_WIN32) || defined(_WIN64)
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == TIME_UTC) {
        unsigned long long now_ms = now.tv_sec * 1000 + now.tv_nsec / 1000000;
        unsigned long long ts_ms  = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
        DWORD delta = (ts_ms > now_ms) ? (DWORD)(ts_ms - now_ms) : 0;
        return _cnd_timedwait_win32(cond, mtx, delta);
    }
    else {
        return thrd_error;
    }
#else
    int ret = pthread_cond_timedwait(cond, mtx, ts);
    if (ret == 0) {
        return thrd_success;
    } else if (ret == ETIMEDOUT) {
        return thrd_timedout;
    } else {
        return thrd_error;
    }
#endif
}

// Condition variable signal
static int cnd_signal(cnd_t *cond) {
#if defined(_WIN32) || defined(_WIN64)
    bool have_waiters = false;;

    /* Are there any waiters? */
    EnterCriticalSection(&cond->num_waiters_lock);
    have_waiters = (cond->num_waiters > 0);
    LeaveCriticalSection(&cond->num_waiters_lock);

    /* If we have any waiting threads, send them a signal */
    if(have_waiters) {
        if (SetEvent(cond->events[_CONDITION_EVENT_ONE]) == 0) {
            return thrd_error;
        }
    }

    return thrd_success;
#else
    return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
#endif
}

// Condition variable broadcast
static int cnd_broadcast(cnd_t *cond) {
#if defined(_WIN32) || defined(_WIN64)
    int have_waiters;

    /* Are there any waiters? */
    EnterCriticalSection(&cond->num_waiters_lock);
    have_waiters = (cond->num_waiters > 0);
    LeaveCriticalSection(&cond->num_waiters_lock);

    /* If we have any waiting threads, send them a signal */
    if(have_waiters) {
        if (SetEvent(cond->events[_CONDITION_EVENT_ALL]) == 0) {
            return thrd_error;
        }
    }

    return thrd_success;
#else
    return pthread_cond_broadcast(cond) == 0 ? thrd_success : thrd_error;
#endif
}

// Call once
static void call_once(once_flag *flag, void (*func)(void)) {
#if defined(_WIN32) || defined(_WIN64)
    InitOnceExecuteOnce(&flag->once, (PINIT_ONCE_FN)func, NULL, NULL);
#else
    pthread_once(&flag->once, func);
#endif
}


// Thread-specific storage creation
static int tss_create(tss_t *key, tss_dtor_t destructor) {
#if defined(_WIN32) || defined(_WIN64)
    *key = TlsAlloc();
    if (*key == TLS_OUT_OF_INDEXES) {
        return thrd_error;
    }
    _tss_destructors[*key] = destructor;
    return thrd_success;
#else
    return pthread_key_create(key, destructor) == 0 ? thrd_success : thrd_error;
#endif
}

// Thread-specific storage deletion
static void tss_delete(tss_t key) {
#if defined(_WIN32) || defined(_WIN64)
    struct _tss_data *data = (struct _tss_data *)TlsGetValue(key);
    struct _tss_data *prev = NULL;
    if (data != NULL) {
        if (data == _tss_head) {
            _tss_head = data->next;
        } else {
            prev = _tss_head;
            if (prev != NULL) {
                while (prev->next != data) {
                    prev = prev->next;
                }
            }
        }
        if (data == _tss_tail) {
            _tss_tail = prev;
        }
        free(data);
    }
    _tss_destructors[key] = NULL;
    TlsFree(key);
#else
    pthread_key_delete(key);
#endif
}

// Thread-specific storage get
static void *tss_get(tss_t key) {
#if defined(_WIN32) || defined(_WIN64)
    struct _tss_data *data = (struct _tss_data *)TlsGetValue(key);
    return data != NULL ? data->value : NULL;
#else
    return pthread_getspecific(key);
#endif
}

// Thread-specific storage set
static int tss_set(tss_t key, void *value) {
#if defined(_WIN32) || defined(_WIN64)
    struct _tss_data *data = (struct _tss_data *)TlsGetValue(key);
    if (data == NULL) {
        data = (struct _tss_data *)malloc(sizeof(struct _tss_data));
        if (data == NULL) {
            return thrd_nomem;
        }
        data->value = value;
        data->key = key;
        data->next = NULL;

        if (_tss_tail != NULL) {
            _tss_tail->next = data;
        } else {
            _tss_tail = data;  
        }

        if (_tss_head == NULL) {
            _tss_head = data;
        }

        if (!TlsSetValue(key, data)) {
            free(data);
            return thrd_error;
        }
    }
    data->value = value;
    return thrd_success;
#else
    return pthread_setspecific(key, value) == 0 ? thrd_success : thrd_error;
#endif
}

#ifdef __cplusplus
}
#endif

#undef THREADING_NO_RETURN
#endif // defined(__has_include) && __has_include(<threads.h>)
#endif // THREADING_H
