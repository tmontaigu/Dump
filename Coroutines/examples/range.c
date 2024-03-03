#include <stdio.h>

#include "coroutines.h"


void infinite_range(coroutine_t* co, void* arg)
{
	unsigned int i = 0;
	while (1)
	{
		i++;
		coroutine_yield_value(co, &i);
	}
}


void example_infinite_coroutine()
{
	printf("Example 1\n");
	coroutine_t* co;
	if (coroutine_new(&co, infinite_range, NULL) != CO_OK)
	{
		fprintf(stderr, "Failed to create the coroutine\n");
		return;
	}


	for (size_t i = 0; i < 10; ++i)
	{
		coroutine_resume(co);
		unsigned int* val = coroutine_yielded_value(co);
		printf("Value yielded! %u\n", *val);
	}

	coroutine_delete(co);
}

/**************************************************************************************/

typedef struct
{
	int start;
	int stop;
	int step;
} range_param_t;

void finite_range(coroutine_t* co, void* arg)
{
	const range_param_t* params = arg;
	for (int i = params->start; i < params->stop; i += params->step)
	{
		coroutine_yield_value(co, &i);
	}
	coroutine_return(co);
}

void example_finite_coroutine()
{
	printf("Example 2\n");
	range_param_t params = { 0,10 ,2 };
	coroutine_t* range;
	if (coroutine_new(&range, finite_range, &params) != CO_OK)
	{
		fprintf(stderr, "Failed to create the coroutine\n");
		return;
	}

	while (coroutine_iter_next(range))
	{
		int* val = coroutine_yielded_value(range);
		printf("Value yielded! %u\n", *val);
	}

	coroutine_delete(range);
}

int main(void)
{
	coroutines_init();

	example_infinite_coroutine();
	example_finite_coroutine();

	coroutines_shutdown();
	return 0;
}