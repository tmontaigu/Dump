#pragma once

typedef enum
{
	CO_OK = 0,
	CO_OS_ERROR,
	CO_ALLOC_ERROR,
	CO_UNSPECIFIED_ERROR,
} COROUTINE_RESULT;


typedef struct coroutine coroutine_t;
typedef void (*coroutine_fn)(coroutine_t*, void* arg);

COROUTINE_RESULT coroutines_init(void);

COROUTINE_RESULT coroutines_shutdown(void);

COROUTINE_RESULT coroutine_new(coroutine_t** self, coroutine_fn fn, void* arg);


void coroutine_yield_value(coroutine_t* self, void* value);

void coroutine_yield(coroutine_t* self);

void coroutine_resume(coroutine_t* self);

void* coroutine_yielded_value(coroutine_t* self);

int coroutine_iter_next(coroutine_t* self);

void coroutine_return(coroutine_t* self);

void coroutine_delete(coroutine_t* self);

