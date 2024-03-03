#include <stdio.h>

#include "coroutines.h"

void hello_word_coroutine(coroutine_t *co, void *arg)
{
	printf("Hello");
	coroutine_yield(co);
	printf("World\n");
}



void hello_world()
{
	coroutine_t* co;
	if (coroutine_new(&co, hello_word_coroutine, NULL) != CO_OK)
	{
		fprintf(stderr, "Failed to create the coroutine\n");
		return;
	}

	coroutine_resume(co);
	printf(", ");
	coroutine_resume(co);

	// Calling resume one more time than needed is a no-op
	coroutine_resume(co);

	coroutine_delete(co);
}


int main(void)
{
	coroutines_init();

	hello_world();
	
	coroutines_shutdown();
	return 0;
}