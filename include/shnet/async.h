#ifndef _shnet_async_h_
#define _shnet_async_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/epoll.h>

#include <shnet/threads.h>


enum async_shutdown
{
	ASYNC_SYNC = 1,
	ASYNC_FREE = 2,
	ASYNC_PTR_FREE = 4
};


struct async_loop;


typedef void (*async_loop_event_t)(struct async_loop*, int*, uint32_t);


struct async_loop
{
	struct epoll_event* events;
	async_loop_event_t on_event;

	uint32_t events_len;

	pthread_t thread;

	int event_fd;
	int epoll_fd;
};


extern void*
async_loop_thread(void* async_loop);


extern int
async_loop(struct async_loop* loop);


extern int
async_loop_start(struct async_loop* loop);


extern void
async_loop_free(struct async_loop* loop);


extern void
async_loop_shutdown(const struct async_loop* loop, enum async_shutdown flags);


extern int
async_loop_add(const struct async_loop* loop, int* event_fd, uint32_t events);


extern int
async_loop_mod(const struct async_loop* loop, int* event_fd, uint32_t events);


extern int
async_loop_remove(const struct async_loop* loop, int* event_fd);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_async_h_ */
