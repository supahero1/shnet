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
	async_sync = 1,
	async_free = 2,
	async_ptr_free = 4
};


struct async_event
{
	int fd;

	uint8_t socket:1;
	uint8_t server:1;
};


struct async_loop
{
	struct epoll_event* events;
	void (*on_event)(struct async_loop*, uint32_t, struct async_event*);

	pthread_t thread;

	struct async_event evt;

	uint32_t events_len;

	int fd;
};


extern void*
async_loop_thread(void*);


extern int
async_loop(struct async_loop* const);


extern int
async_loop_start(struct async_loop* const);


extern void
async_loop_free(struct async_loop* const);


extern void
async_loop_shutdown(const struct async_loop* const, const enum async_shutdown);


extern int
async_loop_add(const struct async_loop* const,
	struct async_event* const, const uint32_t);


extern int
async_loop_mod(const struct async_loop* const,
	struct async_event* const, const uint32_t);


extern int
async_loop_remove(const struct async_loop* const, struct async_event* const);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_async_h_ */
