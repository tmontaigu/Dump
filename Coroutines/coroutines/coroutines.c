#include "coroutines.h"

#include <stdio.h>
#include <Windows.h>


LPVOID g_main_fiber = NULL;

struct coroutine
{
	void (*function)(coroutine_t*, void *arg);
	int is_finished;
	void* arg;
	void* fiber;
	void* yield_val;
};

static void coroutine_entry_point(coroutine_t* self) {
	self->function(self, self->arg);
	if (!self->is_finished)
	{
		coroutine_return(self);
	}
}

COROUTINE_RESULT coroutines_init(void)
{
	g_main_fiber = ConvertThreadToFiber(NULL);

	if (g_main_fiber == NULL)
	{
		return CO_OS_ERROR;
	}

	return CO_OK;
}

COROUTINE_RESULT coroutines_shutdown(void)
{
	if (ConvertFiberToThread() == FALSE)
	{
		return CO_OS_ERROR;
	}
}

COROUTINE_RESULT coroutine_new(coroutine_t** self, coroutine_fn fn, void* arg)
{
	if (self == NULL)
	{
		return CO_UNSPECIFIED_ERROR;
	}
	
	coroutine_t* co = HeapAlloc(GetProcessHeap(), 0, sizeof(coroutine_t));

	if (co == NULL)
	{
		return CO_ALLOC_ERROR;
	}

	co->function = fn;
	co->arg = arg;
	co->is_finished = FALSE;
	co->yield_val = NULL;
	co->fiber = CreateFiber(0 /* default_stack_size */, coroutine_entry_point, co);

	if (co->fiber == NULL)
	{
		HeapFree(GetProcessHeap(), 0, co);
		return CO_OS_ERROR;
	}

	*self = co;
	
	return CO_OK;
}


void coroutine_yield_value(coroutine_t* self, void* value)
{
	self->yield_val = value;
	SwitchToFiber(g_main_fiber);
}

void coroutine_yield(coroutine_t* self)
{
	self->yield_val = NULL;
	SwitchToFiber(g_main_fiber);
}

void coroutine_return(coroutine_t* self)
{
	self->is_finished = TRUE;
	SwitchToFiber(g_main_fiber);
}


void coroutine_resume(coroutine_t* self)
{
	if (self->is_finished == FALSE)
	{
		SwitchToFiber(self->fiber);
	}
}

void* coroutine_yielded_value(coroutine_t* self)
{
	return self->yield_val;
}

int coroutine_is_finished(const coroutine_t* self)
{
	return self->is_finished;
}

int coroutine_iter_next(coroutine_t* self)
{
	coroutine_resume(self);
	return !self->is_finished;
}

void coroutine_delete(coroutine_t *self)
{
	if (self == NULL)
	{
		return;
	}

	DeleteFiber(self->fiber);
	HeapFree(GetProcessHeap(), 0, self);
}
