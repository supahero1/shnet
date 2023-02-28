#ifndef _shnet_threads_h_
#define _shnet_threads_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>


extern void
pthread_cancel_on(void);


extern void
pthread_cancel_off(void);


extern void
pthread_async_on(void);


extern void
pthread_async_off(void);


extern int
pthread_start(pthread_t* id, void* (*func)(void*), void* data);


extern int
pthread_start_explicit(pthread_t* id,
	const pthread_attr_t* attr, void* (*func)(void*), void* data);


extern void
pthread_cancel_sync(pthread_t id);


extern void
pthread_cancel_async(pthread_t id);



typedef struct
{
	pthread_t* ids;

	uint32_t used;
	uint32_t size;
}
pthreads_t;


struct pthreads_data
{
	void* (*func)(void*);

	void* arg;

	sem_t sem;
	pthread_mutex_t mutex;

#ifndef __cplusplus
	_Atomic
#endif
	uint32_t count;
};


extern int
pthreads_start(pthreads_t* threads,
	void* (*func)(void*), void* data, uint32_t amount);


extern int
pthreads_start_explicit(pthreads_t* threads, const pthread_attr_t* attr,
	void* (*func)(void*), void* data, uint32_t amount);


extern void
pthreads_cancel_sync(pthreads_t* threads, uint32_t amount);


extern void
pthreads_cancel_async(pthreads_t* threads, uint32_t amount);


extern void
pthreads_shutdown_sync(pthreads_t* threads);


extern void
pthreads_shutdown_async(pthreads_t* threads);


extern void
pthreads_free(pthreads_t* threads);



struct thread_pool_job
{
	void (*func)(void*);

	void* data;
};


struct thread_pool
{
	sem_t sem;

	uint32_t used;
	uint32_t size;

	pthread_mutex_t mutex;

	struct thread_pool_job* queue;
};


extern void*
thread_pool_thread(void* pool);


extern int
thread_pool(struct thread_pool* pool);


extern void
thread_pool_lock(struct thread_pool* pool);


extern void
thread_pool_unlock(struct thread_pool* pool);


extern int
thread_pool_add_raw(struct thread_pool* pool, void (*func)(void*), void* data);


extern int
thread_pool_add(struct thread_pool* pool, void (*func)(void*), void* data);


extern void
thread_pool_try_work_raw(struct thread_pool* pool);


extern void
thread_pool_try_work(struct thread_pool* pool);


extern void
thread_pool_work_raw(struct thread_pool* pool);


extern void
thread_pool_work(struct thread_pool* pool);


extern void
thread_pool_free(struct thread_pool* pool);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_threads_h_ */
