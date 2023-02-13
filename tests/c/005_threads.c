#include <shnet/test.h>
#include <shnet/threads.h>

#include <stdlib.h>
#include <string.h>

#ifndef SHNET_TEST_VALGRIND

#define thread_count 50UL
#define repeat 20

#define safety_timeout 500

#else

#define thread_count 25UL
#define repeat 3

#define safety_timeout 30000

#endif


pthreads_t threads = {0};


static uint32_t
threads_add(void)
{
	const uint32_t num = 10 + (rand() % 10);

	if(threads.used + num > thread_count)
	{
		return thread_count - threads.used;
	}

	return num;
}


static uint32_t
threads_del(void)
{
	const uint32_t num = 10 + (rand() % 10);

	if(num > threads.used)
	{
		return threads.used;
	}

	return num;
}


void*
assert_0(void* data)
{
	(void) data;

	assert(0);
}


void*
cb(void* data)
{
	(void) data;

	test_sleep(safety_timeout);

	assert(0);
}


void
cb_commit(void* data)
{
	(void) data;

	test_wake();
}


void*
cb_stop_sync_self(void* data)
{
	(void) data;

	pthread_cleanup_push(cb_commit, NULL);

	pthreads_cancel_sync(&threads, threads_del());

	assert(0);

	pthread_cleanup_pop(1);
}


void*
cb_stop_async_self(void* data)
{
	(void) data;

	pthread_cleanup_push(cb_commit, NULL);

	pthreads_cancel_async(&threads, threads_del());

	test_sleep(safety_timeout);

	assert(0);

	pthread_cleanup_pop(1);
}


test_use_shnet_malloc()
test_use_shnet_realloc()
test_use_pthread_create()
test_use_sem_init()
test_use_pthread_mutex_init()


int
main()
{
	test_error_retval(shnet_malloc) = NULL;
	test_error_retval(shnet_realloc) = NULL;
	test_error_retval(pthread_mutex_init) = ENOMEM;

	test_seed_random();


	test_begin("threads err 1");

	test_error(pthread_create);
	test_error_delay(pthread_create) = 3;

	assert(pthreads_start(&threads, assert_0, NULL, 5));

	assert(threads.used == 0);

	test_end();


	test_begin("threads err 2");

	test_error(shnet_malloc);

	assert(pthreads_start(&threads, assert_0, NULL, 5));

	assert(threads.used == 0);

	test_end();


	test_begin("threads err 3");

	pthreads_free(&threads);

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(pthreads_start(&threads, assert_0, NULL, 5));

	assert(threads.used == 0);

	test_end();


	test_begin("threads err 4");

	test_error(sem_init);

	assert(pthreads_start(&threads, assert_0, NULL, 5));

	assert(threads.used == 0);

	test_end();


	test_begin("threads err 5");

	test_error(pthread_mutex_init);

	assert(pthreads_start(&threads, assert_0, NULL, 5));

	assert(threads.used == 0);

	test_end();


	test_begin("threads stop sync");

	for(int i = 0; i < repeat; ++i)
	{
		assert(!pthreads_start(&threads, cb, NULL, threads_add()));

		pthreads_cancel_sync(&threads, threads_del());
	}

	pthreads_shutdown_sync(&threads);

	test_end();


	test_begin("threads stop sync self");

	for(int i = 0; i < repeat; ++i)
	{
		assert(!pthreads_start(&threads, cb, NULL, threads_add()));
		assert(!pthreads_start(&threads, cb_stop_sync_self, NULL, 1));

		test_wait();
	}
	pthreads_shutdown_sync(&threads);

	test_end();


	test_begin("threads stop async");

	for(int i = 0; i < repeat; ++i)
	{
		assert(!pthreads_start(&threads, cb, NULL, threads_add()));

		pthreads_cancel_async(&threads, threads_del());
	}

	pthreads_shutdown_sync(&threads);

	test_end();


	test_begin("threads stop async self");

	for(int i = 0; i < repeat; ++i)
	{
		assert(!pthreads_start(&threads, cb, NULL, threads_add()));
		assert(!pthreads_start(&threads, cb_stop_async_self, NULL, 1));

		test_wait();
	}

	pthreads_shutdown_sync(&threads);

	test_end();


	test_begin("threads stress sync 1");

	assert(!pthreads_start(&threads, cb, NULL, thread_count));
	for(int i = 0; i < repeat; ++i)
	{
		pthreads_cancel_sync(&threads, threads_del());

		assert(!pthreads_start(&threads, cb, NULL, threads_add()));
	}

	pthreads_shutdown_sync(&threads);

	test_end();


	test_begin("threads stress sync 2");

	for(int i = 0; i < repeat; ++i)
	{
		assert(!pthreads_start(&threads, cb, NULL, threads_add()));

		pthreads_cancel_sync(&threads, threads_del());
	}

	pthreads_shutdown_async(&threads);

	test_end();


	test_begin("threads stress async 1");

	assert(!pthreads_start(&threads, cb, NULL, thread_count));
	for(int i = 0; i < repeat; ++i)
	{
		pthreads_cancel_async(&threads, threads_del());

		assert(!pthreads_start(&threads, cb, NULL, threads_add()));
	}

	pthreads_shutdown_async(&threads);

	test_end();


	test_begin("threads stress async 2");

	for(int i = 0; i < repeat; ++i)
	{
		assert(!pthreads_start(&threads, cb, NULL, threads_add()));

		pthreads_cancel_async(&threads, threads_del());
	}

	pthreads_shutdown_sync(&threads);

	test_end();


	pthreads_free(&threads);


	return 0;
}
