#include <shnet/async.h>
#include <shnet/error.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/eventfd.h>


void*
async_loop_thread(void* async_loop)
{
	pthread_cancel_off();

	struct async_loop* const loop = async_loop;

	while(1)
	{
		const int count =
			epoll_wait(loop->epoll_fd, loop->events, loop->events_len, -1);

		for(int i = 0; i < count; ++i)
		{
			int* const event = loop->events[i].data.ptr;
			const uint32_t mask = loop->events[i].events;

			if(*event == loop->event_fd)
			{
				assert(mask == EPOLLIN);

				eventfd_t flags;

				assert(!eventfd_read(loop->event_fd, &flags));

				flags >>= 1;

				if(!(flags & ASYNC_SYNC))
				{
					(void) pthread_detach(loop->thread);
				}

				if(flags & ASYNC_FREE)
				{
					async_loop_free(loop);
				}

				if(flags & ASYNC_PTR_FREE)
				{
					free(loop);
				}

				return NULL;
			}
			else
			{
				loop->on_event(loop, event, mask);
			}
		}
	}

	return NULL;
}


static void
async_loop_free_common(struct async_loop* const loop)
{
	free(loop->events);

	loop->events = NULL;
}


int
async_loop(struct async_loop* const loop)
{
	if(loop->events_len == 0)
	{
		loop->events_len = 64;
	}

	loop->events = shnet_malloc(sizeof(*loop->events) * loop->events_len);

	if(loop->events == NULL)
	{
		return -1;
	}

	safe_execute(
		loop->epoll_fd = epoll_create1(0), loop->epoll_fd == -1, errno
	);

	if(loop->epoll_fd == -1)
	{
		goto err_events;
	}

	safe_execute(
		loop->event_fd = eventfd(0, EFD_NONBLOCK), loop->event_fd == -1, errno
	);

	if(loop->event_fd == -1)
	{
		goto err_epoll_fd;
	}

	if(async_loop_add(loop, &loop->event_fd, EPOLLIN) == -1)
	{
		goto err_event_fd;
	}

	return 0;

	err_event_fd:

	(void) close(loop->event_fd);

	err_epoll_fd:

	(void) close(loop->epoll_fd);

	err_events:

	async_loop_free_common(loop);

	return -1;
}


int
async_loop_start(struct async_loop* const loop)
{
	return pthread_start(&loop->thread, async_loop_thread, loop);
}


void
async_loop_free(struct async_loop* const loop)
{
	(void) close(loop->epoll_fd);
	(void) close(loop->event_fd);
	async_loop_free_common(loop);
}


void
async_loop_shutdown(const struct async_loop* const loop,
	const enum async_shutdown flags)
{
	assert(!eventfd_write(loop->event_fd, 1 | (flags << 1)));

	if(flags & ASYNC_SYNC)
	{
		(void) pthread_join(loop->thread, NULL);
	}
}


static int
async_loop_modify(const struct async_loop* const loop,
	int* const event_fd, const int method, const uint32_t events)
{
	int err;

	safe_execute(err = epoll_ctl(loop->epoll_fd, method, *event_fd, &(
	(struct epoll_event)
	{
		.events = events,
		.data =
		(epoll_data_t)
		{
			.ptr = event_fd
		}
	}
	)), err, errno);

	return err;
}


int
async_loop_add(const struct async_loop* const loop,
	int* const event_fd, const uint32_t events)
{
	return async_loop_modify(loop, event_fd, EPOLL_CTL_ADD, events);
}


int
async_loop_mod(const struct async_loop* const loop,
	int* const event_fd, const uint32_t events)
{
	return async_loop_modify(loop, event_fd, EPOLL_CTL_MOD, events);
}


int
async_loop_remove(const struct async_loop* const loop, int* const event_fd)
{
	return async_loop_modify(loop, event_fd, EPOLL_CTL_DEL, 0);
}
