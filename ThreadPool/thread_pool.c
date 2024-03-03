#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <malloc.h>

#include "thread_pool.h"

#if defined(WIN32)
#include <windows.h>
#endif

#if defined (__unix__) // || (defined (__APPLE__) && defined (__MACH__))
#include <unistd.h>
#endif


long try_get_num_threads() {
#if defined (__unix__) // || (defined (__APPLE__) && defined (__MACH__))
    return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    unsigned long numCPU = sysinfo.dwNumberOfProcessors;
    return (long)numCPU; // I very much doubt that there could be > LONG_MAX CPU
#else
    return 0;
#endif
}

#if !defined(__STDC_NO_THREADS__)
#include <threads.h>

typedef thrd_t thread_t;

typedef int main_thread_fn_return_t;
typedef main_thread_fn_return_t(thread_fn_main_t)(void*);

int thread_create(thread_t *thread, thread_fn_main_t thread_fn_main, void*arg) {
    int status = thrd_create(thread, thread_fn_main, arg);
    if (status != 0) {
        return 1;
    }
    status = thrd_detach(*thread);
    return status != 0;
}

void thread_destroy(thread_t* thread) {
    (void*)thread;
    // There is nothing to do for std threads
}

typedef mtx_t mutex_t;

int mutex_init(mutex_t* mutex) {
    return mtx_init(mutex, mtx_plain) != 0;
}

int mutex_lock(mutex_t* mutex) {
    return mtx_lock(mutex) != 0;
}

int mutex_unlock(mutex_t* mutex) {
    return mtx_unlock(mutex) != 0;
}

void mutex_destroy(mutex_t* mutex) {
    mtx_destroy(mutex);
}

typedef cnd_t condvar_t;

int condvar_init(condvar_t *condvar) {
    return cnd_init(condvar) != 0;
}

int condvar_wait(condvar_t *condvar, mutex_t *mutex) {
    return cnd_wait(condvar, mutex) != 0;
}

int condvar_broadcast(condvar_t *condvar) {
    return cnd_broadcast(condvar) != 0;
}

int condvar_signal(condvar_t *condvar) {
    return cnd_signal(condvar) != 0;
}

void condvar_destroy(condvar_t *condvar) {
    cnd_destroy(condvar);
}
#elif defined(WIN32)


typedef HANDLE thread_t;

typedef unsigned long main_thread_fn_return_t;
typedef main_thread_fn_return_t(thread_fn_main_t)(void*);

int thread_create(thread_t *thread, thread_fn_main_t thread_fn_main, void*arg) {
    // thread_t is a handle which is void *
     *thread = CreateThread(
        NULL, // Default security parameters
        0, // Default stack size
        thread_fn_main,
        arg,
        0, // Threads will run immediately
        NULL // We don't want to have the thread id
    );
     return thread == NULL;
}

void thread_destroy(thread_t* thread) {
    CloseHandle(*thread);
}

typedef CRITICAL_SECTION mutex_t;

int mutex_init(mutex_t* mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int mutex_lock(mutex_t* mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int mutex_unlock(mutex_t* mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

void mutex_destroy(mutex_t* mutex) {

}

typedef CONDITION_VARIABLE  condvar_t;

int condvar_init(condvar_t *condvar) {
    InitializeConditionVariable (condvar);
    return 0;
}

int condvar_wait(condvar_t *condvar, mutex_t *mutex) {
    SleepConditionVariableCS(condvar, mutex, INFINITE);
    return 0;
}

int condvar_broadcast(condvar_t *condvar) {
    WakeAllConditionVariable(condvar);
    return 0;
}

int condvar_signal(condvar_t *condvar) {
    WakeConditionVariable(condvar);
    return 0;
}

void condvar_destroy(condvar_t *condvar) {

}
#else
#error "This compiler & standard library does not have C11's threads"
#endif


/// Definition of a task
typedef struct thread_task_s {
    /// The function to execute
    thread_task_fn_t *fn;
    /// The argument of the function
    void *arg;
    /// Pointer to next task (if any)
    struct thread_task_s *next;
} thread_task_t;

struct thread_pool_s {
    size_t num_threads;
    thread_t *threads;

    thread_task_t *first_task;
    thread_task_t *last_task;

    thread_task_t *dangling_task;

    // Mutex used for all operations
    // and the cond var below
    mutex_t mutex;
    // cond var on which worker thread
    // wait to be notified of available tasks
    condvar_t cond_task_available;
    // cond var on which 'main' thread waits
    // to know when tasks are done / threads are shutting down
    condvar_t cond_thread_done;

    // Contains the number of threads that are alive
    // (but not necessary working on some task)
    size_t thread_count;
    // Threads shall stop
    bool stop_requested;
};

/// The function that each thread of the pool will run
///
/// When there are no task available, the thread waits
/// otherwise it picks up and to one of the tasks
main_thread_fn_return_t thread_fn_main(void* arg) {
    assert(arg != NULL);
    thread_pool_t *pool = arg;
    int status;

    while (1) {
        status = mutex_lock(&pool->mutex);
        if (status != 0) {
            return 1;
        }

        while(!pool->stop_requested && pool->first_task == NULL && status == 0) {
            status = condvar_wait(&pool->cond_task_available, &pool->mutex);
        }

        // Got woken up by and error
        if (status != 0) {
            status = mutex_unlock(&pool->mutex);
            assert(status == 0);
            return 1;
        }

        // Woken up because thread shall stop
        if (pool->stop_requested) {
            pool->thread_count -= 1;
            status = condvar_signal(&pool->cond_thread_done);
            assert(status == 0);
            status = mutex_unlock(&pool->mutex);
            assert(status == 0);
            return 0;
        }

        // Woken up because there are some tasks to do
        if (pool->first_task != NULL) {
            thread_task_t *work = pool->first_task;
            pool->first_task = work->next;

            if (pool->last_task == work) {
                assert(pool->first_task == NULL);
                pool->last_task = NULL;
            }

            status = mutex_unlock(&pool->mutex);
            assert(status == 0);

            work->fn(work->arg);

            status = mutex_lock(&pool->mutex);
            assert(status == 0);

            work->next = pool->dangling_task;
            pool->dangling_task = work;

            status = condvar_signal(&pool->cond_thread_done);
            assert(status == 0);
            status = mutex_unlock(&pool->mutex);
            assert(status == 0);
            continue;
        }

        assert(0); // We should not be there
    }

    return 0;
}


thread_pool_t * thread_pool_create(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = try_get_num_threads();
    }

    // assert msg: Cannot determine number of threads in this platform
    // please provide a number
    assert(num_threads != 0);

    thread_pool_t *pool = malloc(sizeof(*pool));

    if (pool == NULL) {
        return NULL;
    }

    pool->threads = malloc(sizeof(thread_t) * num_threads);
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    pool->num_threads = num_threads;
    pool->stop_requested = 0;
    pool->first_task = NULL;
    pool->last_task = NULL;
    pool->dangling_task = NULL;

    if (mutex_init(&pool->mutex) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if (condvar_init(&pool->cond_task_available) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if (condvar_init(&pool->cond_thread_done) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // Lock ourselves while we are creating threads
    if (mutex_lock(&pool->mutex) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    pool->thread_count = 0;
    for (size_t i = 0; i < pool->num_threads; i++) {
        int status = thread_create(&pool->threads[i], thread_fn_main, pool);
        assert(status == 0);
        pool->thread_count += 1;
    }
    assert(mutex_unlock(&pool->mutex) == 0);


    return pool;
}

void thread_pool_wait(thread_pool_t *pool) {
    assert(pool != NULL);

    int status;
    status = mutex_lock(&pool->mutex);
    if (status != 0) {
        return;
    }

    while (((pool->thread_count != 0 && pool->stop_requested) || pool->first_task != NULL) && status == 0) {
        status = condvar_wait(&pool->cond_thread_done, &pool->mutex);
    }
    mutex_unlock(&pool->mutex);
}

void thread_pool_add_task(thread_pool_t *pool, thread_task_fn_t *fn, void* arg)
{
    assert(pool != NULL);

    int status = mutex_lock(&pool->mutex);

    thread_task_t *work;
    if (pool->dangling_task != NULL) {
        work = pool->dangling_task;
        pool->dangling_task = work->next;
    } else {
        work = malloc(sizeof(*work));
        assert(work != NULL);
    }

    work->next = NULL;
    work->arg = arg;
    work->fn = fn;

    assert(status == 0);

    if (pool->last_task == NULL) {
        assert(pool->first_task == NULL);
        pool->first_task = pool->last_task = work;
    } else {
        pool->last_task->next = work;
        pool->last_task = work;
    }

    // Tells waiting threads that a task arrived
    status = condvar_broadcast(&pool->cond_task_available);
    assert(status == 0);
    status = mutex_unlock(&pool->mutex);
    assert(status == 0);
}

void thread_pool_delete(thread_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    int status;
    status = mutex_lock(&pool->mutex);
    if (status != 0) {
        // this is kinda bad no ?
        return;
    }

    pool->stop_requested = true;

    status = condvar_broadcast(&pool->cond_task_available);
    assert(status == 0);
    status = mutex_unlock(&pool->mutex);
    assert(status == 0);

    thread_pool_wait(pool);

    mutex_destroy(&pool->mutex);
    condvar_destroy(&pool->cond_task_available);
    condvar_destroy(&pool->cond_thread_done);
    for (size_t i = 0; i < pool->num_threads; ++i) {
        thread_destroy(&pool->threads[i]);
    }
    free(pool->threads);

    thread_task_t *work = pool->dangling_task;
    while (work != NULL) {
        thread_task_t *next = work->next;
        free(work);
        work = next;
    }
    free(pool);
}

size_t thread_pool_num_threads(thread_pool_t *pool) {
    assert(pool != NULL);
    return pool->num_threads;
}
