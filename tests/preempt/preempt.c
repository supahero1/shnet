#include "preempt.h"

#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>

static struct preemption_data queue[10];

static int queue_len = 0;
static int preemption_off = 0;

static int
(*_pthread_create_real)(pthread_t*,
	const pthread_attr_t*, void* (*)(void*), void*) = NULL;


int
pthread_create(pthread_t* a,
	const pthread_attr_t* b, void* (*c)(void*), void* d)
{
	if(_pthread_create_real == NULL)
	{
		_pthread_create_real = (int (*)(pthread_t*, const pthread_attr_t*,
			void* (*)(void*), void*)) dlsym(RTLD_NEXT, "pthread_create");

		if(_pthread_create_real == NULL)
		{
			puts(dlerror());
			assert(!"(shnet_preempt: not supposed to happen)");
		}
	}

	if(preemption_off)
	{
		queue[queue_len++] =
		(struct preemption_data)
		{
			.a = a,
			.b = b,
			.c = c,
			.d = d
		};

		return 0;
	}

	return _pthread_create_real(a, b, c, d);
}


void
test_preempt_off(void)
{
	preemption_off = 1;
}


void
test_preempt_on(void)
{
	preemption_off = 0;

	for(int i = 0; i < queue_len; ++i)
	{
		assert(!_pthread_create_real(
			queue[i].a,
			queue[i].b,
			queue[i].c,
			queue[i].d
		));
	}

	queue_len = 0;
}
