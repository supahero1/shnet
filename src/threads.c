#include <shnet/error.h>
#include <shnet/threads.h>

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>


void
pthread_cancel_on()
{
	(void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}


void
pthread_cancel_off()
{
	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}


void
pthread_async_on()
{
	(void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}


void
pthread_async_off()
{
	(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
}


int
pthread_start_explicit(pthread_t* id,
	const pthread_attr_t* const attr, void* (*func)(void*), void* const data)
{
	int err;
	pthread_t tid;

	if(id == NULL)
	{
		id = &tid;
	}

	safe_execute(err = pthread_create(id, attr, func, data), err != 0, err);

	if(err != 0)
	{
		errno = err;

		return -1;
	}

	return 0;
}


int
pthread_start(pthread_t* const id, void* (*func)(void*), void* const data)
{
	return pthread_start_explicit(id, NULL, func, data);
}


void
pthread_cancel_sync(const pthread_t id)
{
	if(pthread_equal(id, pthread_self()))
	{
		(void) pthread_detach(id);
		pthread_exit(NULL);
	}
	else
	{
		(void) pthread_cancel(id);
		(void) pthread_join(id, NULL);
	}
}


void
pthread_cancel_async(const pthread_t id)
{
	(void) pthread_detach(id);
	(void) pthread_cancel(id);
}



static void*
pthreads_thread(void* pthreads_thread_data)
{
	/*
	 * Notes for future me:
	 * NO, pthread_barrier_t may not be used here, because
	 * it cannot be interrupted by a cancellation request,
	 * which alone defeats the whole point of this system.
	 * Jesus...     This took too many tries to get right.
	 */
	struct pthreads_data* const data = pthreads_thread_data;

	while(sem_wait(&data->sem) != 0);

	void* (*func)(void*) = data->func;
	void* const arg = data->arg;

	if(atomic_fetch_sub_explicit(&data->count, 1, memory_order_relaxed) == 1)
	{
		(void) pthread_mutex_unlock(&data->mutex);
	}

	return func(arg);
}


static int
pthreads_resize(pthreads_t* const threads, const uint32_t new_size)
{
	if(new_size == threads->size)
	{
		return 0;
	}

	if(new_size == 0)
	{
		pthreads_free(threads);

		return 0;
	}

	void* const ptr =
		shnet_realloc(threads->ids, sizeof(pthread_t) * new_size);

	if(ptr == NULL)
	{
		return -1;
	}

	threads->ids = ptr;
	threads->size = new_size;

	return 0;
}


int
pthreads_start_explicit(pthreads_t* const threads,
	const pthread_attr_t* const attr, void* (*func)(void*),
	void* const arg, const uint32_t amount)
{
	if(amount == 0)
	{
		return 0;
	}

	const uint32_t new_size = threads->used + amount;

	if(
		new_size > threads->size &&
		pthreads_resize(threads, (new_size << 1) | 1) &&
		pthreads_resize(threads, new_size)
	)
	{
		return -1;
	}

	struct pthreads_data* const data =
		shnet_malloc(sizeof(struct pthreads_data));

	if(data == NULL)
	{
		return -1;
	}

	int ret = 0;
	int err;

	safe_execute(err = sem_init(&data->sem, 0, 0), err, errno);

	if(err)
	{
		ret = -1;

		goto goto_data;
	}

	safe_execute(err = pthread_mutex_init(&data->mutex, NULL), err, err);

	if(err)
	{
		ret = -1;

		goto goto_sem;
	}

	data->arg = arg;
	data->func = func;

	atomic_init(&data->count, amount);

	for(uint32_t i = 0; i < amount; ++i, ++threads->used)
	{
		if(pthread_start_explicit(
			threads->ids + threads->used, attr, pthreads_thread, data
		))
		{
			pthreads_cancel_sync(threads, i);

			ret = -1;

			goto goto_mutex;
		}
	}

	(void) pthread_mutex_lock(&data->mutex);

	for(uint32_t i = 0; i < amount; ++i)
	{
		(void) sem_post(&data->sem);
	}

	/*
	 * Another note for future me:
	 * Confirmation is required. Otherwise, pthreads_cancel_*()
	 * will cancel our threads before they get the data freed.
	 */
	(void) pthread_mutex_lock(&data->mutex);
	(void) pthread_mutex_unlock(&data->mutex);

	goto_mutex:

	(void) pthread_mutex_destroy(&data->mutex);

	goto_sem:

	(void) sem_destroy(&data->sem);

	goto_data:

	free(data);

	return ret;
}


int
pthreads_start(pthreads_t* const threads,
	void* (*func)(void*), void* const arg, const uint32_t amount)
{
	return pthreads_start_explicit(threads, NULL, func, arg, amount);
}


static void
pthreads_shrink(pthreads_t* const threads)
{
	if(threads->used < (threads->size >> 2))
	{
		(void) pthreads_resize(threads, threads->used << 1);
	}
}


void
pthreads_cancel_sync(pthreads_t* const threads, const uint32_t amount)
{
	if(amount == 0)
	{
		return;
	}

	const uint32_t total = threads->used - amount;
	const pthread_t self = pthread_self();
	int ourself = 0;

	for(uint32_t i = total; i < threads->used; ++i)
	{
		if(!pthread_equal(threads->ids[i], self))
		{
			(void) pthread_cancel(threads->ids[i]);
		}
		else
		{
			ourself = 1;
		}
	}

	for(uint32_t i = total; i < threads->used; ++i)
	{
		if(!pthread_equal(threads->ids[i], self))
		{
			(void) pthread_join(threads->ids[i], NULL);
		}
	}

	threads->used = total;

	pthreads_shrink(threads);

	if(ourself)
	{
		(void) pthread_detach(self);
		pthread_exit(NULL);
	}
}


void
pthreads_cancel_async(pthreads_t* const threads, const uint32_t amount)
{
	if(amount == 0)
	{
		return;
	}

	const uint32_t total = threads->used - amount;
	const pthread_t self = pthread_self();
	int ourself = 0;

	for(uint32_t i = total; i < threads->used; ++i)
	{
		if(!pthread_equal(threads->ids[i], self))
		{
			(void) pthread_detach(threads->ids[i]);
			(void) pthread_cancel(threads->ids[i]);
		}
		else
		{
			ourself = 1;
		}
	}

	threads->used = total;

	pthreads_shrink(threads);

	if(ourself)
	{
		(void) pthread_detach(self);
		(void) pthread_cancel(self);
	}
}


void
pthreads_shutdown_sync(pthreads_t* const threads)
{
	pthreads_cancel_sync(threads, threads->used);
}


void
pthreads_shutdown_async(pthreads_t* const threads)
{
	pthreads_cancel_async(threads, threads->used);
}


void
pthreads_free(pthreads_t* const threads)
{
	free(threads->ids);

	threads->ids = NULL;
	threads->used = 0;
	threads->size = 0;
}



void*
thread_pool_thread(void* thread_pool_thread_data)
{
	struct thread_pool* const pool = thread_pool_thread_data;

	while(1)
	{
		thread_pool_work(pool);
	}
}


int
thread_pool(struct thread_pool* const pool)
{
	int err;

	safe_execute(err = pthread_mutex_init(&pool->mutex, NULL), err != 0, err);

	if(err != 0)
	{
		errno = err;

		return -1;
	}

	safe_execute(err = sem_init(&pool->sem, 0, 0), err, errno);

	if(err)
	{
		(void) pthread_mutex_destroy(&pool->mutex);
	}

	return err;
}


void
thread_pool_lock(struct thread_pool* const pool)
{
	(void) pthread_mutex_lock(&pool->mutex);
}


void
thread_pool_unlock(struct thread_pool* const pool)
{
	(void) pthread_mutex_unlock(&pool->mutex);
}


static void
thread_pool_free_common(struct thread_pool* const pool)
{
	free(pool->queue);

	pool->queue = NULL;
	pool->used = 0;
	pool->size = 0;
}


static int
thread_pool_resize_raw(struct thread_pool* const pool, const uint32_t new_size)
{
	if(new_size == pool->size)
	{
		return 0;
	}

	if(new_size == 0)
	{
		thread_pool_free_common(pool);

		return 0;
	}

	void* const ptr =
		shnet_realloc(pool->queue, sizeof(struct thread_pool_job) * new_size);

	if(ptr == NULL)
	{
		return -1;
	}

	pool->queue = ptr;
	pool->size = new_size;

	return 0;
}


static int
thread_pool_add_common(struct thread_pool* const pool,
	void (*func)(void*), void* const data, const int lock)
{
	if(lock)
	{
		thread_pool_lock(pool);
	}

	const uint32_t new_size = pool->used + 1;

	if(
		new_size > pool->size &&
		thread_pool_resize_raw(pool, (new_size << 1) | 1) &&
		thread_pool_resize_raw(pool, new_size)
	)
	{
		if(lock)
		{
			thread_pool_unlock(pool);
		}

		return -1;
	}

	pool->queue[pool->used++] =
	(struct thread_pool_job) {
		.func = func,
		.data = data
	};

	if(lock)
	{
		thread_pool_unlock(pool);
	}

	(void) sem_post(&pool->sem);

	return 0;
}


int
thread_pool_add_raw(struct thread_pool* const pool,
	void (*func)(void*), void* const data)
{
	return thread_pool_add_common(pool, func, data, 0);
}


int
thread_pool_add(struct thread_pool* const pool,
	void (*func)(void*), void* const data)
{
	return thread_pool_add_common(pool, func, data, 1);
}


static void
thread_pool_try_work_common(struct thread_pool* const pool, const int lock)
{
	if(lock)
	{
		thread_pool_lock(pool);
	}

	if(pool->used == 0)
	{
		if(lock)
		{
			thread_pool_unlock(pool);
		}

		return;
	}

	struct thread_pool_job data = *pool->queue;

	--pool->used;

	(void) memmove(pool->queue, pool->queue + 1,
		sizeof(struct thread_pool_job) * pool->used);

	if(pool->used < (pool->size >> 2))
	{
		(void) thread_pool_resize_raw(pool, pool->used << 1);
	}

	if(lock)
	{
		thread_pool_unlock(pool);
	}

	data.func(data.data);
}


void
thread_pool_try_work_raw(struct thread_pool* const pool)
{
	thread_pool_try_work_common(pool, 0);
}


void
thread_pool_try_work(struct thread_pool* const pool)
{
	thread_pool_try_work_common(pool, 1);
}


void
thread_pool_work_raw(struct thread_pool* const pool)
{
	while(sem_wait(&pool->sem) != 0);

	thread_pool_try_work_raw(pool);
}


void
thread_pool_work(struct thread_pool* const pool)
{
	pthread_async_off();
	pthread_cancel_on();
	while(sem_wait(&pool->sem) != 0);
	pthread_cancel_off();
	thread_pool_try_work(pool);
	pthread_cancel_on();
	pthread_async_on();
}


void
thread_pool_free(struct thread_pool* const pool)
{
	(void) sem_destroy(&pool->sem);
	(void) pthread_mutex_destroy(&pool->mutex);
	thread_pool_free_common(pool);
}
