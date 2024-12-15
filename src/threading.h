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

#if defined(__has_include) && __has_include(<stdbool.h>)
#include <stdbool.h>
#else
#define bool int
#define true 1
#define false 0
#endif

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
    } handle;                /* Mutex handle */
    bool already_locked;      /* true if the mtx is already locked */
    bool recursive;           /* true if the mtx is recursive */
    bool timed;               /* true if the mtx is timed */
} mtx_t;
typedef CONDITION_VARIABLE cnd_t;
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

// Thread creation
static int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
#if defined(_WIN32) || defined(_WIN64)
    *thr = (HANDLE)_beginthreadex(NULL, 0, (unsigned(__stdcall *)(void *))func, arg, 0, NULL);
    return *thr ? thrd_success : thrd_error;
#else
    return pthread_create(thr, NULL, (void *(*)(void *))func, arg) == 0 ? thrd_success : thrd_error;
#endif
}

// Thread join
static int thrd_join(thrd_t thr, int *res) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD ret = WaitForSingleObject(thr, INFINITE);
    if (ret == WAIT_OBJECT_0) {
        if (res) {
            DWORD code;
            GetExitCodeThread(thr, &code);
            *res = (int)code;
        }
        CloseHandle(thr);
        return thrd_success;
    }
    return thrd_error;
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
    CloseHandle(thr);
    return thrd_success;
#else
    return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
#endif
}

// Thread exit
THREADING_NO_RETURN static void thrd_exit(int res) {
#if defined(_WIN32) || defined(_WIN64)
    _endthreadex((unsigned)res);
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
    return lhs == rhs;
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
        EnterCriticalSection(&mtx->handle.cs);
    } else {
        DWORD result = WaitForSingleObject(mtx->handle.mut, INFINITE);
        if (result != WAIT_OBJECT_0) {
            return thrd_error;
        }
    }

    if (!mtx->recursive) {
        while (mtx->already_locked) {
            Sleep(1);
            mtx->already_locked = true;
        }
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

    if ((now.tv_sec < ts->tv_sec) || ((now.tv_sec == ts->tv_sec) && (now.tv_nsec <= ts->tv_nsec))) {
        timeout_ms = (DWORD)(ts->tv_sec - now.tv_sec) * 1000;
        timeout_ms += (ts->tv_nsec - now.tv_nsec) / 1000000;
        timeout_ms += 1;
    }

    DWORD result = WaitForSingleObject(mtx->handle.mut, timeout_ms);
    if (result == WAIT_OBJECT_0) {
        if (!mtx->recursive) {
            while (mtx->already_locked) {
                Sleep(1);
                mtx->already_locked = true;
            }
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
        LeaveCriticalSection(&mtx->handle.cs);
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


// Condition variable initialization
static int cnd_init(cnd_t *cond) {
#if defined(_WIN32) || defined(_WIN64)
    InitializeConditionVariable(cond);
    return thrd_success;
#else
    return pthread_cond_init(cond, NULL) == 0 ? thrd_success : thrd_error;
#endif
}

// Condition variable destruction
static void cnd_destroy(cnd_t *cond) {
#if !defined(_WIN32) && !defined(_WIN64)
    pthread_cond_destroy(cond);
#endif
}

// Condition variable wait
static int cnd_wait(cnd_t *cond, mtx_t *mtx) {
#if defined(_WIN32) || defined(_WIN64)
    if (mtx->timed) {
        return thrd_error;
    }
    SleepConditionVariableCS(cond, &(mtx->handle.cs), INFINITE);
    return thrd_success;
#else
    return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
#endif
}

// Condition variable timed wait
static int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts) {
#if defined(_WIN32) || defined(_WIN64)
    if (mtx->timed) {
        return thrd_error;
    }
    DWORD ms = (DWORD)((ts->tv_sec * 1000) + (ts->tv_nsec / 1000000));
    return SleepConditionVariableCS(cond, &(mtx->handle.cs), ms) ? thrd_success : thrd_timedout;
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
    WakeConditionVariable(cond);
    return thrd_success;
#else
    return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
#endif
}

// Condition variable broadcast
static int cnd_broadcast(cnd_t *cond) {
#if defined(_WIN32) || defined(_WIN64)
    WakeAllConditionVariable(cond);
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
    return (*key != TLS_OUT_OF_INDEXES) ? thrd_success : thrd_error;
#else
    return pthread_key_create(key, destructor) == 0 ? thrd_success : thrd_error;
#endif
}

// Thread-specific storage deletion
static void tss_delete(tss_t key) {
#if defined(_WIN32) || defined(_WIN64)
    TlsFree(key);
#else
    pthread_key_delete(key);
#endif
}

// Thread-specific storage get
static void *tss_get(tss_t key) {
#if defined(_WIN32) || defined(_WIN64)
    return TlsGetValue(key);
#else
    return pthread_getspecific(key);
#endif
}

// Thread-specific storage set
static int tss_set(tss_t key, void *value) {
#if defined(_WIN32) || defined(_WIN64)
    return TlsSetValue(key, value) ? thrd_success : thrd_error;
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
