#include <shnet/test.h>
#include <shnet/threads.h>

#include <string.h>
#include <stdatomic.h>


const uint32_t target = 1000;
_Atomic uint32_t count;


uint32_t
get_count(void)
{
	return atomic_load_explicit(&count, memory_order_relaxed);
}


void
set_count(const uint32_t a)
{
	atomic_store_explicit(&count, a, memory_order_relaxed);
}


void
add_count(const uint32_t a)
{
	atomic_fetch_add_explicit(&count, a, memory_order_relaxed);
}


void
work(void* data)
{
	(void) data;

	add_count(1);
}


void
pool_work(void* data)
{
	(void) data;

	for(unsigned long i = 0; i < 100; ++i)
	{
		add_count(1);
	}

	if(get_count() == target)
	{
		test_wake();
	}
}


test_use_shnet_realloc()
test_use_sem_init()
test_use_pthread_mutex_init()


int
main()
{
	struct thread_pool pool = {0};

	set_count(0);


	test_begin("thread pool init err 1");

	test_error(pthread_mutex_init);

	assert(thread_pool(&pool));

	assert(pool.used == 0);

	test_end();


	test_begin("thread pool init err 2");

	test_error(sem_init);

	assert(thread_pool(&pool));

	assert(pool.used == 0);

	test_end();


	test_begin("thread pool init");

	assert(!thread_pool(&pool));

	assert(pool.used == 0);

	thread_pool_free(&pool);

	assert(!thread_pool(&pool));

	assert(pool.used == 0);

	test_end();


	test_begin("thread pool add raw err 1");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(thread_pool_add_raw(&pool, (void (*)(void*)) 0xbad, (void*) 0xbad));

	test_end();


	test_begin("thread pool add raw err 2");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	thread_pool_lock(&pool);
	assert(thread_pool_add_raw(&pool, (void (*)(void*)) 0xbad, (void*) 0xbad));
	thread_pool_unlock(&pool);

	test_end();


	test_begin("thread pool add err");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(thread_pool_add(&pool, (void (*)(void*)) 0xbad, (void*) 0xbad));

	assert(pool.used == 0);

	test_end();


	test_begin("thread pool add raw 1");

	assert(!thread_pool_add_raw(&pool, work, NULL));

	assert(pool.used == 1);

	thread_pool_free(&pool);

	assert(!thread_pool(&pool));

	test_end();


	test_begin("thread pool add raw 2");

	thread_pool_lock(&pool);
	assert(!thread_pool_add_raw(&pool, work, NULL));
	thread_pool_unlock(&pool);

	assert(pool.used == 1);

	thread_pool_free(&pool);

	assert(!thread_pool(&pool));

	test_end();


	test_begin("thread pool add");

	assert(!thread_pool_add(&pool, work, NULL));

	assert(pool.used == 1);

	test_end();


	test_begin("thread pool try work raw 1");

	assert(get_count() == 0);

	thread_pool_try_work_raw(&pool);

	assert(get_count() == 1);

	assert(pool.used == 0);

	assert(!thread_pool_add_raw(&pool, work, NULL));

	set_count(0);

	test_end();


	test_begin("thread pool try work raw 2");

	assert(get_count() == 0);

	thread_pool_lock(&pool);
	thread_pool_try_work_raw(&pool);
	thread_pool_unlock(&pool);

	assert(get_count() == 1);

	assert(pool.used == 0);

	assert(!thread_pool_add_raw(&pool, work, NULL));

	set_count(0);

	test_end();


	test_begin("thread pool try work");

	assert(get_count() == 0);

	thread_pool_try_work(&pool);

	assert(get_count() == 1);

	assert(pool.used == 0);

	set_count(0);

	test_end();


	test_begin("thread pool try work raw empty 1");

	assert(get_count() == 0);

	thread_pool_try_work_raw(&pool);

	assert(get_count() == 0);

	test_end();


	test_begin("thread pool try work raw empty 2");

	assert(get_count() == 0);

	thread_pool_lock(&pool);
	thread_pool_try_work_raw(&pool);
	thread_pool_unlock(&pool);

	assert(get_count() == 0);

	test_end();


	test_begin("thread pool try work empty");

	assert(get_count() == 0);

	thread_pool_try_work(&pool);

	assert(get_count() == 0);

	test_end();


	test_begin("thread pool drain");

	while(sem_trywait(&pool.sem) == 0);

	test_end();


	test_begin("thread pool work raw 1");

	assert(get_count() == 0);

	thread_pool_lock(&pool);

	for(int i = 0; i < 10; ++i)
	{
		assert(!thread_pool_add_raw(&pool, work, NULL));
	}

	thread_pool_unlock(&pool);

	for(int i = 0; i < 9; ++i)
	{
		thread_pool_work_raw(&pool);
	}

	assert(get_count() == 9);

	assert(pool.used == 1);

	set_count(0);

	test_end();


	test_begin("thread pool work raw 2");

	assert(get_count() == 0);

	for(int i = 0; i < 6; ++i)
	{
		assert(!thread_pool_add(&pool, work, NULL));
	}

	thread_pool_lock(&pool);

	for(int i = 0; i < 7; ++i)
	{
		thread_pool_work_raw(&pool);
	}

	thread_pool_unlock(&pool);

	assert(get_count() == 7);

	assert(pool.used == 0);

	set_count(0);

	test_end();


	test_begin("thread pool work");

	assert(get_count() == 0);

	for(int i = 0; i < 13; ++i)
	{
		assert(!thread_pool_add_raw(&pool, work, NULL));
	}

	thread_pool_unlock(&pool);

	for(int i = 0; i < 13; ++i)
	{
		thread_pool_work(&pool);
	}

	assert(get_count() == 13);

	assert(pool.used == 0);

	set_count(0);

	test_end();


	test_begin("thread pool threads");

	pthreads_t threads = {0};

	assert(!pthreads_start(&threads, thread_pool_thread, &pool, 4));

	pthreads_shutdown_sync(&threads);

	test_end();


	test_begin("thread pool threads work");

	assert(get_count() == 0);

	assert(!pthreads_start(&threads, thread_pool_thread, &pool, 4));

	for(int i = 0; i < 10; ++i)
	{
		assert(!thread_pool_add(&pool, pool_work, NULL));
	}

	test_wait();

	assert(get_count() == target);

	pthreads_shutdown_sync(&threads);

	pthreads_free(&threads);

	test_end();


	test_begin("thread pool free");

	thread_pool_free(&pool);

	assert(pool.used == 0);

	test_end();


	return 0;
}
