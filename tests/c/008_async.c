#include <shnet/test.h>
#include <shnet/async.h>

#include <unistd.h>
#include <stdlib.h>
#include <sys/eventfd.h>


void
onevt(struct async_loop* loop, uint32_t events, struct async_event* event)
{
	(void) loop;

	uint64_t out;

	assert(!eventfd_read(event->fd, &out));

	assert(events == EPOLLIN);
	assert(out == (uintptr_t) event);

	test_wake();
}


test_use_shnet_malloc()
test_use_pthread_create()
test_use_eventfd()
test_use_epoll_create1()
test_use_epoll_ctl()


int
main()
{
	test_begin("async init err 1");

	test_error(shnet_malloc);

	struct async_loop loop = {0};

	assert(async_loop(&loop));

	test_end();


	test_begin("async init err 2");

	test_error(epoll_create1);

	assert(async_loop(&loop));

	test_end();


	test_begin("async init err 3");

	test_error(eventfd);

	assert(async_loop(&loop));

	test_end();


	test_begin("async init err 4");

	test_error(epoll_ctl);

	assert(async_loop(&loop));

	test_end();


	test_begin("async init");

	assert(!async_loop(&loop));

	async_loop_free(&loop);

	test_end();


	test_begin("async init 2");

	assert(!async_loop(&loop));

	test_end();


	test_begin("async start err");

	test_error(pthread_create);

	assert(async_loop_start(&loop));

	test_end();


	test_begin("async start");

	assert(!async_loop_start(&loop));

	test_end();


	test_begin("async stop sync");

	async_loop_shutdown(&loop, async_sync);
	async_loop_free(&loop);

	assert(!async_loop(&loop));
	assert(!async_loop_start(&loop));

	test_end();


	test_begin("async stop async");

	async_loop_shutdown(&loop, async_free);

	test_end();


	test_begin("async stop async free");

	struct async_loop* loop2 = calloc(1, sizeof(*loop2));

	assert(loop2);

	assert(!async_loop(loop2));
	assert(!async_loop_start(loop2));

	async_loop_shutdown(loop2, async_free | async_ptr_free);

	test_end();


	test_begin("async event init");

	struct async_loop l = {0};
	l.events_len = 2;
	l.on_event = onevt;

	assert(!async_loop(&l));
	assert(!async_loop_start(&l));

	struct async_event events[5] = {0};

	for(int i = 0; i < 5; ++i)
	{
		events[i].fd = eventfd(0, EFD_NONBLOCK);

		assert(events[i].fd != -1);
		assert(!async_loop_add(&l, events + i, EPOLLIN | EPOLLET));
	}

	test_end();


	test_begin("async event 1");

	for(int i = 0; i < 5; ++i)
	{
		assert(!eventfd_write(events[i].fd, (uintptr_t)(events + i)));
	}

	for(int i = 0; i < 5; ++i)
	{
		test_wait();
	}

	test_end();


	test_begin("async event 2");

	assert(!async_loop_mod(&l, events + 0, 0));
	assert(!eventfd_write(events[0].fd, 1));

	for(int i = 1; i < 5; ++i)
	{
		assert(!eventfd_write(events[i].fd, (uintptr_t)(events + i)));
	}

	for(int i = 0; i < 4; ++i)
	{
		test_wait();
	}

	test_end();


	test_begin("async event 3");

	assert(!eventfd_write(events[0].fd, (uintptr_t) events - 1));
	assert(!async_loop_mod(&l, events + 0, EPOLLIN));

	test_wait();

	test_end();


	test_begin("async manual");

	async_loop_shutdown(&l, async_sync);
	async_loop_shutdown(&l, 0);

	(void) async_loop_thread(&l);

	test_end();


	test_begin("async event free");

	for(int i = 0; i < 5; ++i)
	{
		assert(!async_loop_remove(&l, events + i));
		close(events[i].fd);
	}

	async_loop_free(&l);

	assert(l.on_event == onevt);
	assert(l.events_len == 2);

	test_end();


	return 0;
}
