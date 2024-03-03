#ifndef THREAD_POOL_H
#define THREAD_POOL_H

typedef struct thread_pool_s thread_pool_t;

/// The signature of a function (task) a thread can execute
typedef void(thread_task_fn_t)(void*);

/// Creates a thread pool
///
/// \param num_threads tells how many threads the pool should have.
///  If zero, the pool will try to determine and use the number of CPU threads
///  of the machine
/// \return The thread pool or NULL in case of error
thread_pool_t * thread_pool_create(size_t num_threads);

/// Adds a task to be picked up by threads of the pool
void thread_pool_add_task(thread_pool_t *pool, thread_task_fn_t *fn, void* arg);

/// Blocks the current thread until all tasks are done
void thread_pool_wait(thread_pool_t *pool);

/// Blocks until tasks and threads shutdown, then deletes the pool
///
/// pool may be NULL
void thread_pool_delete(thread_pool_t *pool);

/// Returns the number of threads present in the pool
size_t thread_pool_num_threads(thread_pool_t *pool);

#endif // THREAD_POOL_H
