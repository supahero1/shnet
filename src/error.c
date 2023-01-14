#include <errno.h>
#include <stdlib.h>

#include <shnet/error.h>


int
error_handler(int error, int count)
{
	(void) count;

	switch(error)
	{

	case 0:
	case EINTR:
	{
		return 0;
	}

	default:
	{
		return -1;
	}

	}
}


void*
shnet_malloc(const size_t size)
{
	void* ptr;

	safe_execute(ptr = malloc(size), ptr == NULL, ENOMEM);

	return ptr;
}


void*
shnet_calloc(const size_t num, const size_t size)
{
	void* ptr;

	safe_execute(ptr = calloc(num, size), ptr == NULL, ENOMEM);

	return ptr;
}


void*
shnet_realloc(void* const in, const size_t size)
{
	void* ptr;

	safe_execute(ptr = realloc(in, size), ptr == NULL, ENOMEM);

	return ptr;
}
