#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "thread_pool.h"

#include <math.h>

// Includes used to sleep
#if defined(__unix__)
#include <unistd.h>
#elif defined(WIN32)
#include <windows.h>
#endif

// https://nachtimwald.com/2019/04/05/cross-platform-thread-wrapper/

void worker(void *arg)
{
    int *val = arg;

    *val += 1000;

#if defined(__unix__)
    if (*val%2)
        usleep(100000);
#elif defined(WIN32)
    Sleep(100); // milliseconds
#endif
}

int main(void) {
    printf("Start\n");

    static const size_t num_items   = 50;

    int *vals = calloc(num_items, sizeof(*vals));
    if (vals == NULL) {
        printf("Failure when creating values\n");
        return EXIT_FAILURE;
    }

    thread_pool_t *pool = thread_pool_create(0);
    if (pool == NULL) {
        printf("Failure when creating the thread_pool\n");
        return EXIT_FAILURE;
    }

    size_t num_threads = thread_pool_num_threads(pool);
    printf("The pool has %zu threads\n", num_threads);

    for (size_t i=0; i<num_items; i++) {
        vals[i] = (int)i;
    }

    for (size_t i=0; i<num_items; i++) {
        thread_pool_add_task(pool, worker, vals+i);
    }

    thread_pool_wait(pool);

    for (size_t i=0; i<num_items; i++) {
        assert(vals[i] == i + 1000);
    }

    thread_pool_delete(pool);
    free(vals);

    printf("End\n");
    return 0;
}
