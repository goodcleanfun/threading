#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "threading.h"
#include "greatest/greatest.h"


#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <string.h>
#endif

#if defined(__APPLE__) || (defined(__MINGW32__) && (__GNUC__ < 4)) || defined(__TINYC__)
#define NO_CT_TLS
#endif

#ifndef NO_CT_TLS
_Thread_local int tls_var = 0;
#endif

// Test variables
mtx_t mutex;
cnd_t cond;
tss_t thread_specific_key;
#define NUM_CALL_ONCE_FLAGS 10000
once_flag once_flags[NUM_CALL_ONCE_FLAGS];
int count = 0;


static int thread_test_args(void *arg) {
    return *(int *)arg;
}

#define NUM_THREADS 4

TEST test_thread_arg_and_return(void) {
    thrd_t threads[NUM_THREADS];
    int ids[NUM_THREADS];
    int ret;

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = rand();
        ASSERT_EQ(thrd_create(&(threads[i]), thread_test_args, (void*) &(ids[i])), thrd_success);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], &ret);
        ASSERT_EQ(ret, ids[i]);
    }
    PASS();
}

#ifndef NO_CT_TLS
static int thread_test_local_storage(void *arg) {
    (void)arg;
    tls_var = rand();
    return 0;
}

TEST test_thread_local_storage(void) {
    thrd_t t1;

    tls_var = 1;
    ASSERT_EQ(thrd_create(&t1, thread_test_local_storage, NULL), thrd_success);   
    ASSERT_EQ(thrd_join(t1, NULL), thrd_success);
    ASSERT_EQ(tls_var, 1);
    PASS();
}
#endif


#define THREAD_LOCK_ITERS 10000

static int thread_lock(void *arg) {
    mtx_t try_mutex;

    (void)arg;
    for (int i = 0; i < THREAD_LOCK_ITERS; i++) {
        ASSERT_EQ(mtx_lock(&mutex), thrd_success);
        ASSERT_EQ(mtx_trylock(&mutex), thrd_busy);
        ++count;
        ASSERT_EQ(mtx_unlock(&mutex), thrd_success);
    }

    ASSERT_EQ(mtx_init(&try_mutex, mtx_plain), thrd_success);
    ASSERT_EQ(mtx_lock(&mutex), thrd_success);

    for (int i = 0; i < THREAD_LOCK_ITERS; i++) {
        ASSERT_EQ(mtx_trylock(&try_mutex), thrd_success);
        ASSERT_EQ(mtx_trylock(&try_mutex), thrd_busy);
        ++count;
        ASSERT_EQ(mtx_unlock(&try_mutex), thrd_success);
    }

    ASSERT_EQ(mtx_unlock(&mutex), thrd_success);
    mtx_destroy(&try_mutex);
    return 0;
}

#define MUTEX_LOCKING_THREADS 120

TEST test_thread_locking(void) {
    thrd_t threads[MUTEX_LOCKING_THREADS];

    count = 0;

    for (int i = 0; i < MUTEX_LOCKING_THREADS; i++) {
        ASSERT_EQ(thrd_create(&(threads[i]), thread_lock, NULL), thrd_success);
    }

    for (int i = 0; i < MUTEX_LOCKING_THREADS; i++) {
        ASSERT_EQ(thrd_join(threads[i], NULL), thrd_success);
    }

    ASSERT_EQ(count, MUTEX_LOCKING_THREADS * THREAD_LOCK_ITERS * 2);
    PASS();

}

struct test_mutex_data {
    mtx_t mutex;
    volatile int i;
    volatile int completed;
};

static int test_mutex_recursive_cb(void *data) {
    const int iterations = 10000;
    struct test_mutex_data *mutex_data = (struct test_mutex_data *)data;

    ASSERT_EQ(mtx_lock(&(mutex_data->mutex)), thrd_success);
    for (int i = 0; i < iterations; i++) {
        ASSERT_EQ(mtx_lock(&mutex_data->mutex), thrd_success);
        ASSERT_EQ(mutex_data->i++, i);
    }

    for (int i = iterations - 1; i >= 0; i--) {
        ASSERT_EQ(mtx_unlock(&(mutex_data->mutex)), thrd_success);
        ASSERT_EQ(--(mutex_data->i), i);
    }

    ASSERT_EQ(mutex_data->i, 0);
    mutex_data->completed++;
    mtx_unlock(&(mutex_data->mutex));

    return 0;
}

#define NUM_RECURSIVE_THREADS 120

TEST test_mutex_recursive(void) {
    thrd_t threads[NUM_RECURSIVE_THREADS];
    struct test_mutex_data mutex_data = {0};

    ASSERT_EQ(mtx_init(&(mutex_data.mutex), mtx_recursive), thrd_success);
    mutex_data.i = 0;
    mutex_data.completed = 0;

    for (int i = 0; i < NUM_RECURSIVE_THREADS; i++) {
        ASSERT_EQ(thrd_create(&(threads[i]), test_mutex_recursive_cb, &mutex_data), thrd_success);
    }

    for (int i = 0; i < NUM_RECURSIVE_THREADS; i++) {
        ASSERT_EQ(thrd_join(threads[i], NULL), thrd_success);
    }

    ASSERT_EQ(mutex_data.completed, NUM_RECURSIVE_THREADS);
    mtx_destroy(&(mutex_data.mutex));
    PASS();
}

static int timespec_compare(const struct timespec *a, const struct timespec *b) {
    if (a->tv_sec != b->tv_sec) {
        return a->tv_sec - b->tv_sec;
    } else if (a->tv_nsec != b->tv_nsec) {
        return a->tv_nsec - b->tv_nsec;
    }
    return 0;
}

#define NANOSECONDS_PER_SECOND 1000000000

static void timespec_add_ns(struct timespec *ts, long ns) {
    ts->tv_sec += ns / NANOSECONDS_PER_SECOND;
    ts->tv_nsec += ns % NANOSECONDS_PER_SECOND;
    if (ts->tv_nsec >= NANOSECONDS_PER_SECOND) {
        ts->tv_sec++;
        ts->tv_nsec -= NANOSECONDS_PER_SECOND;
    }
}

struct test_mutex_timed_data {
    mtx_t mutex;
    struct timespec start;
    struct timespec timeout;
    struct timespec end;
    struct timespec upper;
};

static int test_mutex_timed_thread_func(void *arg) {
    int ret;
    struct timespec ts;
    struct test_mutex_timed_data *data = (struct test_mutex_timed_data *)arg;

    ret = mtx_timedlock(&(data->mutex), &(data->timeout));
    ASSERT_EQ(ret, thrd_timedout);

    timespec_get(&ts, TIME_UTC);
    ret = timespec_compare(&ts, &(data->start));
    ASSERT(ret >= 0);
    ret = timespec_compare(&ts, &(data->timeout));
    ASSERT(ret >= 0);
    ret = timespec_compare(&ts, &(data->end));
    ASSERT(ret < 0);

    ret = mtx_lock(&(data->mutex));
    ASSERT_EQ(ret, thrd_success);

    timespec_get(&ts, TIME_UTC);
    ret = timespec_compare(&ts, &(data->end));
    ASSERT(ret >= 0);
    ret = timespec_compare(&ts, &(data->upper));
    ASSERT(ret < 0);
    mtx_unlock(&(data->mutex));
    return 0;
}

TEST test_mutex_timed(void) {
    struct test_mutex_timed_data data;
    thrd_t thread;
    const struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = (NANOSECONDS_PER_SECOND / 10) * 2
    };
    struct timespec remaining = {0};
    remaining.tv_sec = interval.tv_sec;
    remaining.tv_nsec = interval.tv_nsec;
    struct timespec start;
    struct timespec end;


    mtx_init(&(data.mutex), mtx_timed);
    mtx_lock(&(data.mutex));

    timespec_get(&data.start, TIME_UTC);
    data.timeout = data.start;
    timespec_add_ns(&(data.timeout), NANOSECONDS_PER_SECOND / 10);
    data.end = data.timeout;
    timespec_add_ns(&(data.end), NANOSECONDS_PER_SECOND / 10);
    data.upper = data.end;
    timespec_add_ns(&(data.upper), NANOSECONDS_PER_SECOND / 10);

    ASSERT_EQ(thrd_create(&thread, test_mutex_timed_thread_func, &data), thrd_success);
    timespec_get(&start, TIME_UTC);
    ASSERT_EQ(thrd_sleep(&interval, &remaining), 0);
    timespec_get(&end, TIME_UTC);
    mtx_unlock(&(data.mutex));

    ASSERT_EQ(thrd_join(thread, NULL), thrd_success);

    PASS();
}

TEST test_sleep(void) {
    struct timespec ts;
    struct timespec interval;
    struct timespec end_ts;

    interval.tv_sec = 0;
    interval.tv_nsec = NANOSECONDS_PER_SECOND / 10;

    timespec_get(&ts, TIME_UTC);
    timespec_add_ns(&ts, NANOSECONDS_PER_SECOND / 10);
    thrd_sleep(&interval, NULL);
    timespec_get(&end_ts, TIME_UTC);
    ASSERT(timespec_compare(&ts, &end_ts) <= 0);
    PASS();
}

static int test_thread_exit_func(void *arg) {
    (void)arg;
    test_sleep();
    thrd_exit(2);
    return 1;
}

TEST test_thread_exit(void) {
    thrd_t thread;
    int res;
    ASSERT_EQ(thrd_create(&thread, test_thread_exit_func, NULL), thrd_success);
    ASSERT_EQ(thrd_join(thread, &res), thrd_success);
    ASSERT_EQ(res, 2);
    PASS();
}

static int thread_condition_notifier(void *arg) {
    (void)arg;

    mtx_lock(&mutex);
    --count;
    cnd_broadcast(&cond);
    mtx_unlock(&mutex);
    return 0;
}

static int thread_condition_waiter(void *arg) {
    (void)arg;

    fflush(stdout);
    mtx_lock(&mutex);
    while (count > 0) {
        fflush(stdout);
        cnd_wait(&cond, &mutex);
    }
    mtx_unlock(&mutex);
    return 0;
}

#define NUM_CONDITION_THREADS 40

TEST test_condition_variables(void) {
    thrd_t waiter, t[NUM_CONDITION_THREADS];
    
    // global count
    count = NUM_CONDITION_THREADS;

    ASSERT_EQ(thrd_create(&waiter, thread_condition_waiter, NULL), thrd_success);

    for (int i = 0; i < NUM_CONDITION_THREADS; i++) {
        ASSERT_EQ(thrd_create(&(t[i]), thread_condition_notifier, NULL), thrd_success);
    }

    ASSERT_EQ(thrd_join(waiter, NULL), thrd_success);
    for (int i = 0; i < NUM_CONDITION_THREADS; i++) {
        ASSERT_EQ(thrd_join(t[i], NULL), thrd_success);
    }
    PASS();
}

static int thread_yield(void *arg) {
    (void)arg;
    thrd_yield();
    return 0;
}

#define NUM_YIELD_THREADS 40

TEST test_yield(void) {
    thrd_t t[NUM_YIELD_THREADS];

    for (int i = 0; i < NUM_YIELD_THREADS; i++) {
        ASSERT_EQ(thrd_create(&(t[i]), thread_yield, NULL), thrd_success);
    }

    thrd_yield();

    for (int i = 0; i < NUM_YIELD_THREADS; i++) {
        ASSERT_EQ(thrd_join(t[i], NULL), thrd_success);
    }
    PASS();
}

TEST test_time(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    ASSERT(ts.tv_sec > 0);
    PASS();
}

static void thread_once_func(void) {
    mtx_lock(&mutex);
    ++count;
    mtx_unlock(&mutex);
}

static int thread_once(void *data) {
    (void)data;
    for (int i = 0; i < NUM_CALL_ONCE_FLAGS; i++) {
        call_once(&once_flags[i], thread_once_func);
    }
    return 0;
}

#define NUM_ONCE_THREADS 16

TEST test_once(void) {
    const once_flag once_flag_init = ONCE_FLAG_INIT;
    thrd_t t[NUM_ONCE_THREADS];

    for (int i = 0; i < NUM_CALL_ONCE_FLAGS; i++) {
        once_flags[i] = once_flag_init;
    }

    mtx_lock(&mutex);
    count = 0;
    mtx_unlock(&mutex);

    for (int i = 0; i < NUM_ONCE_THREADS; i++) {
        ASSERT_EQ(thrd_create(&(t[i]), thread_once, NULL), thrd_success);
    }

    for (int i = 0; i < NUM_ONCE_THREADS; i++) {
        ASSERT_EQ(thrd_join(t[i], NULL), thrd_success);
    }

    ASSERT_EQ(count, NUM_CALL_ONCE_FLAGS);
    PASS();
}


struct test_thread_specific_data {
    tss_t key;
    mtx_t mutex;
    int values_freed;
} test_tss_data;

static void test_tss_free(void *val) {
    mtx_lock(&test_tss_data.mutex);
    test_tss_data.values_freed++;
    mtx_unlock(&test_tss_data.mutex);
    free(val);
}

static int test_tss_thread_func(void *arg) {
    int *value = (int *)malloc(sizeof(int));
    if (value == NULL) {
        return 1;
    }

    (void)arg;
    *value = rand();
    ASSERT_EQ(tss_get(test_tss_data.key), NULL);
    ASSERT_EQ(tss_set(test_tss_data.key, value), thrd_success);
    ASSERT_EQ(tss_get(test_tss_data.key), value);

    ASSERT_EQ(tss_set(test_tss_data.key, NULL), thrd_success);
    ASSERT_EQ(tss_get(test_tss_data.key), NULL);
    ASSERT_EQ(tss_set(test_tss_data.key, value), thrd_success);
    ASSERT_EQ(tss_get(test_tss_data.key), value);

    PASS();
}

#define NUM_TSS_THREADS 256

TEST test_tss(void) {
    thrd_t threads[NUM_TSS_THREADS];
    int *value = (int *)malloc(sizeof(int));
    *value = rand();

    ASSERT_EQ(tss_create(&test_tss_data.key, test_tss_free), thrd_success);
    ASSERT_EQ(mtx_init(&test_tss_data.mutex, mtx_plain), thrd_success);
    test_tss_data.values_freed = 0;

    ASSERT_EQ(tss_get(test_tss_data.key), NULL);
    ASSERT_EQ(tss_set(test_tss_data.key, value), thrd_success);
    ASSERT_EQ(tss_get(test_tss_data.key), value);

    for (int i = 0; i < NUM_TSS_THREADS; i++) {
        ASSERT_EQ(thrd_create(&(threads[i]), test_tss_thread_func, NULL), thrd_success);
    }

    for (int i = 0; i < NUM_TSS_THREADS; i++) {
        ASSERT_EQ(thrd_join(threads[i], NULL), thrd_success);
    }

    ASSERT_EQ(test_tss_data.values_freed, NUM_TSS_THREADS);
    ASSERT_EQ(tss_get(test_tss_data.key), value);
    tss_delete(test_tss_data.key);
    ASSERT_EQ(tss_get(test_tss_data.key), NULL);
    ASSERT_EQ(test_tss_data.values_freed, NUM_TSS_THREADS);
    free(value);

    PASS();
}





SUITE(test_threads_suite) {
    mtx_init(&mutex, mtx_plain);
    cnd_init(&cond);

    RUN_TEST(test_thread_arg_and_return);
    #ifndef NO_CT_TLS
    RUN_TEST(test_thread_local_storage);
    #endif
    RUN_TEST(test_thread_locking);

    // Clean up
    mtx_destroy(&mutex);
    cnd_destroy(&cond);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();

    RUN_SUITE(test_threads_suite);

    GREATEST_MAIN_END();
}
