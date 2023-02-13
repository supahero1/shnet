#include <shnet/test.h>
#include <shnet/time.h>

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/*
 * NOTE: these tests MAY fail if the system is under heavy load. Normally,
 * this 2k lines of code test executes almost instantly with Valgrind on.
 * The test will fail after 1 second of inactivity (caused by anything).
 */

#define safety_timeout 1


struct time_timers timers = {0};


void
assert_0(void* data)
{
	(void) data;

	assert(0);
}


void
wake(void* data)
{
	(void) data;

	test_wake();
}


void
wake_int(void* data)
{
	assert(!time_cancel_interval(&timers, data));

	test_wake();
}


struct test_data
{
	struct time_timer* ref;
	struct time_timer* self;

	int reverse;
	int res;
	int type;
};


#define type_timeout 0
#define type_interval 1


void
cancel(void* data)
{
	struct test_data* d = data;

	if(d->type == type_timeout)
	{
		assert(time_cancel_timeout(&timers, d->ref) == d->res);

		time_lock(&timers);

		if(d->reverse && d->self)
		{
			assert(!time_cancel_interval_raw(&timers, d->self));
		}

		time_unlock(&timers);
	}
	else
	{
		assert(time_cancel_interval(&timers, d->ref) == d->res);

		time_lock(&timers);

		if(d->self)
		{
			assert(!time_cancel_interval_raw(&timers, d->self));
		}

		time_unlock(&timers);
	}
}


void
cancel_1(void* data)
{
	struct test_data* d = data;

	if(d->type == type_timeout)
	{
		assert(time_cancel_timeout_raw(&timers, d->ref) == d->res);

		if(d->reverse && d->self)
		{
			assert(!time_cancel_interval(&timers, d->self));
		}
	}
	else
	{
		assert(time_cancel_interval_raw(&timers, d->ref) == d->res);

		if(d->self)
		{
			assert(!time_cancel_interval(&timers, d->self));
		}
	}
}


void
cancel_2(void* data)
{
	struct test_data* d = data;

	time_lock(&timers);

	if(d->type == type_timeout)
	{
		assert(time_cancel_timeout_raw(&timers, d->ref) == d->res);

		if(d->reverse && d->self)
		{
			assert(!time_cancel_interval_raw(&timers, d->self));
		}
	}
	else
	{
		assert(time_cancel_interval_raw(&timers, d->ref) == d->res);
	}

	time_unlock(&timers);

	if(d->type == type_interval && d->self)
	{
		assert(!time_cancel_interval_raw(&timers, d->self));
	}
}


test_use_shnet_realloc()
test_use_pthread_create()
test_use_sem_init()
test_use_pthread_mutex_init()


int
main()
{
	test_begin("time conversion");

	assert(time_sec_to_ns(0) == 0);
	assert(time_sec_to_us(0) == 0);
	assert(time_sec_to_ms(0) == 0);

	assert(time_ms_to_ns(0) == 0);
	assert(time_ms_to_us(0) == 0);
	assert(time_ms_to_sec(0) == 0);

	assert(time_us_to_ns(0) == 0);
	assert(time_us_to_sec(0) == 0);
	assert(time_us_to_ms(0) == 0);

	assert(time_ns_to_sec(0) == 0);
	assert(time_ns_to_ms(0) == 0);
	assert(time_ns_to_us(0) == 0);

	assert(time_sec_to_ns(1) == 1000000000);
	assert(time_sec_to_us(1) == 1000000);
	assert(time_sec_to_ms(1) == 1000);

	assert(time_ms_to_ns(1) == 1000000);
	assert(time_ms_to_us(1) == 1000);
	assert(time_ms_to_sec(999) == 0);
	assert(time_ms_to_sec(1999) == 1);

	assert(time_us_to_ns(1) == 1000);
	assert(time_us_to_sec(999999) == 0);
	assert(time_us_to_sec(1999999) == 1);
	assert(time_us_to_ms(999) == 0);
	assert(time_us_to_ms(1999) == 1);

	assert(time_ns_to_sec(999999999) == 0);
	assert(time_ns_to_sec(1999999999) == 1);
	assert(time_ns_to_ms(999999) == 0);
	assert(time_ns_to_ms(1999999) == 1);
	assert(time_ns_to_us(999) == 0);
	assert(time_ns_to_us(1999) == 1);

	test_end();


	test_begin("time init err 1");

	test_error(sem_init);

	assert(time_timers(&timers));

	test_end();


	test_begin("time init err 2");

	test_error(sem_init);
	test_error_delay(sem_init) = 1;

	assert(time_timers(&timers));

	test_end();


	test_begin("time init err 3");

	test_error(pthread_mutex_init);

	assert(time_timers(&timers));

	test_end();


	test_begin("time init");

	assert(!time_timers(&timers));

	time_free(&timers);

	test_end();


	test_begin("time init 2");

	assert(!time_timers(&timers));

	test_end();


	test_begin("time start err");

	test_error(pthread_create);

	assert(time_start(&timers));

	test_end();


	test_begin("time start");

	assert(!time_start(&timers));

	test_end();


	test_begin("time stop sync");

	time_stop_sync(&timers);

	assert(!time_start(&timers));

	test_end();


	test_begin("time stop async");

	time_stop_async(&timers);

	test_end();


	test_begin("time start locked");

	time_lock(&timers);

	assert(!time_start(&timers));

	test_end();


	test_begin("time stop sync locked");

	time_stop_sync(&timers);

	assert(!time_start(&timers));

	test_end();


	test_begin("time stop async locked");

	time_stop_async(&timers);

	time_unlock(&timers);

	test_end();


	/*
	 * TIMEOUTS
	 */

	test_begin("time timeouts add err");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(time_add_timeout(&timers, NULL));

	test_end();


	test_begin("time timeouts add raw err 1");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(time_add_timeout_raw(&timers, NULL));

	test_end();


	test_begin("time timeouts add raw err 2");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	time_lock(&timers);
	assert(time_add_timeout_raw(&timers, NULL));
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts add");

	assert(!time_start(&timers));

	struct time_timer ref1;

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_get_sec(safety_timeout),
		.ref = &ref1
	}
	)));

	test_end();


	test_begin("time timeouts add raw");

	struct time_timer ref2;

	time_lock(&timers);

	/*
	 * Can't do raw without a lock, because of a race condition.
	 */
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_get_ns(1),
		.ref = &ref2
	}
	)));
	time_unlock(&timers);

	test_wait();

	test_end();


	test_begin("time timeouts cancel");

	assert(!time_cancel_timeout(&timers, &ref1));

	test_end();


	test_begin("time timeouts cancel err");

	assert(time_cancel_timeout(&timers, &ref1));
	assert(time_cancel_timeout(&timers, &ref2));

	test_end();


	test_begin("time timeouts cancel raw");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_get_us(time_sec_to_us(safety_timeout)),
		.ref = &ref1
	}
	)));
	assert(!time_cancel_timeout_raw(&timers, &ref1));
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts cancel raw err");

	time_lock(&timers);
	assert(time_cancel_timeout_raw(&timers, &ref1));
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts prep");

	struct time_timer refs[101];
	const uint64_t base = time_get_sec(safety_timeout);

	assert(base > 0);

	for(int i = 0; i < 101; ++i)
	{
		assert(!time_add_timeout(&timers, &(
		(struct time_timeout)
		{
			.func = assert_0,
			.time = base + i,
			.ref = refs + i
		}
		)));
	}

	test_end();


	test_begin("time timeouts open decrease 1");

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = base + 50,
		.ref = &ref1
	}
	)));

	struct time_timeout* timeout = time_open_timeout(&timers, &ref1);

	assert(timeout);

	timeout->time = base + 10;

	time_close_timeout(&timers, &ref1);

	test_end();


	test_begin("time timeouts open increase 1");

	timeout = time_open_timeout(&timers, &ref1);

	assert(timeout);

	timeout->time = base + 90;

	time_close_timeout(&timers, &ref1);

	test_end();


	test_begin("time timeouts open decrease 2");

	timeout = time_open_timeout(&timers, &ref1);

	assert(timeout);

	timeout->time = base - 1;

	time_close_timeout(&timers, &ref1);

	test_end();


	test_begin("time timeouts open increase 2");

	timeout = time_open_timeout(&timers, &ref1);

	assert(timeout);

	timeout->time = base + 101;

	time_close_timeout(&timers, &ref1);

	test_end();


	test_begin("time timeouts open cancel");

	timeout = time_open_timeout(&timers, &ref1);

	assert(timeout);

	time_cancel_timeout_raw(&timers, &ref1);

	time_close_timeout(&timers, &ref1);

	test_end();


	test_begin("time timeouts open err");

	timeout = time_open_timeout(&timers, &ref1);

	assert(!timeout);

	test_end();


	test_begin("time timeouts open raw decrease 1");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
		(struct time_timeout)
	{
		.func = assert_0,
		.time = base + 50,
		.ref = &ref1
	})));

	timeout = time_open_timeout_raw(&timers, &ref1);

	assert(timeout);

	timeout->time = base + 10;

	time_close_timeout_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts open raw increase 1");

	time_lock(&timers);

	timeout = time_open_timeout_raw(&timers, &ref1);

	assert(timeout);

	timeout->time = base + 90;

	time_close_timeout_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts open raw decrease 2");

	time_lock(&timers);

	timeout = time_open_timeout_raw(&timers, &ref1);

	assert(timeout);

	timeout->time = base - 1;

	time_close_timeout_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts open raw increase 2");

	time_lock(&timers);

	timeout = time_open_timeout_raw(&timers, &ref1);

	assert(timeout);

	timeout->time = base + 101;

	time_close_timeout_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts open raw cancel");

	time_lock(&timers);

	timeout = time_open_timeout_raw(&timers, &ref1);

	assert(timeout);

	time_cancel_timeout_raw(&timers, &ref1);
	time_close_timeout_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts open raw err");

	time_lock(&timers);

	timeout = time_open_timeout_raw(&timers, &ref1);

	assert(!timeout);

	time_unlock(&timers);

	test_end();


	test_begin("time timeouts JIT cancel");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately,
		.ref = &ref1
	}
	)));

	/*
	 * Now pray that we switch to the timers thread.
	 * If we don't, this won't work, and code coverage won't be 100%.
	 * But I guess that's fine as long as it can happen.
	 */
	test_sleep(1);

	assert(!time_cancel_timeout_raw(&timers, &ref1));
	time_unlock(&timers);

	test_end();


	test_begin("time timeouts cancel self");

	struct test_data data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.res = -1
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately,
		.ref = &ref1
	}
	)));

	/*
	 * Even though the timer does not interfere with any of the next tests, we
	 * can't have this test fail asynchronously, because we won't know that this
	 * particular test failed and there will be big trouble finding the cause.
	 */
	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self min");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 0,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
		(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self avg");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 50,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self max");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 100,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel timeout");

	time_lock(&timers);

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	})));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	})));

	time_unlock(&timers);

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.res = -1
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw min 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 1,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw avg 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 51,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw max 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 99,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel timeout raw 1");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));
	time_unlock(&timers);

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.res = -1
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw min 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 2,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw avg 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 49,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel self raw max 2");

	data = (struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 98,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel timeout raw 2");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data = (struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));
	time_unlock(&timers);

	test_wait();

	test_end();


	test_begin("time timeouts cleanup");
	time_stop_sync(&timers);
	test_end();


	/*
	 * INTERVALS
	 */

	test_begin("time intervals add err");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(time_add_interval(&timers, NULL));

	test_end();


	test_begin("time intervals add raw err 1");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	assert(time_add_interval_raw(&timers, NULL));

	test_end();


	test_begin("time intervals add raw err 2");

	test_error(shnet_realloc);
	test_error_count(shnet_realloc) = 2;

	time_lock(&timers);
	assert(time_add_interval_raw(&timers, NULL));
	time_unlock(&timers);

	test_end();


	test_begin("time intervals add");

	assert(!time_start(&timers));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_get_sec(safety_timeout),
		.ref = &ref1
	}
	)));

	test_end();


	test_begin("time intervals add raw");

	/*
	 * Can't do raw without a lock, because of a race condition.
	 */
	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_get_ns(1),
		.ref = &ref2
	}
	)));
	time_unlock(&timers);

	test_wait();

	test_end();


	test_begin("time intervals cancel");

	assert(!time_cancel_interval(&timers, &ref1));

	test_end();


	test_begin("time intervals cancel err");

	assert(time_cancel_interval(&timers, &ref1));
	assert(time_cancel_interval(&timers, &ref2));

	test_end();


	test_begin("time intervals cancel raw");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_get_ms(time_sec_to_ms(safety_timeout)),
		.ref = &ref1
	}
	)));
	assert(!time_cancel_interval_raw(&timers, &ref1));
	time_unlock(&timers);

	test_end();


	test_begin("time intervals cancel raw err");

	time_lock(&timers);
	assert(time_cancel_interval_raw(&timers, &ref1));
	time_unlock(&timers);

	test_end();


	test_begin("time intervals prep");

	struct time_timer refs_int[101];

	for(int i = 0; i < 101; ++i)
	{
		assert(!time_add_interval(&timers, &(
		(struct time_interval)
		{
			.func = assert_0,
			.base_time = base,
			.interval = 1,
			.count = i,
			.ref = refs_int + i
		}
		)));
	}

	test_end();


	test_begin("time intervals open decrease 1");

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = base,
		.interval = 1,
		.count = 50,
		.ref = &ref1
	}
	)));

	struct time_interval* interval = time_open_interval(&timers, &ref1);

	assert(interval);

	interval->count = 10;

	time_close_interval(&timers, &ref1);

	test_end();


	test_begin("time intervals open increase 1");

	interval = time_open_interval(&timers, &ref1);

	assert(interval);

	interval->count = 90;

	time_close_interval(&timers, &ref1);

	test_end();


	test_begin("time intervals open decrease 2");

	interval = time_open_interval(&timers, &ref1);

	assert(interval);

	interval->count = 0;
	interval->base_time = base - 1;

	time_close_interval(&timers, &ref1);

	test_end();


	test_begin("time intervals open increase 2");

	interval = time_open_interval(&timers, &ref1);

	assert(interval);

	interval->count = 102;

	time_close_interval(&timers, &ref1);

	test_end();


	test_begin("time intervals open cancel");

	interval = time_open_interval(&timers, &ref1);

	assert(interval);

	time_cancel_interval_raw(&timers, &ref1);
	time_close_interval(&timers, &ref1);

	test_end();


	test_begin("time intervals open err");

	interval = time_open_interval(&timers, &ref1);

	assert(!interval);

	test_end();


	test_begin("time intervals open raw decrease 1");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = base + 50,
		.ref = &ref1
	}
	)));

	interval = time_open_interval_raw(&timers, &ref1);

	assert(interval);

	interval->base_time = base + 10;

	time_close_interval_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time intervals open raw increase 1");

	time_lock(&timers);

	interval = time_open_interval_raw(&timers, &ref1);

	assert(interval);

	interval->base_time = base + 90;

	time_close_interval_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time intervals open raw decrease 2");

	time_lock(&timers);

	interval = time_open_interval_raw(&timers, &ref1);

	assert(interval);

	interval->base_time = base - 1;

	time_close_interval_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time intervals open raw increase 2");

	time_lock(&timers);

	interval = time_open_interval_raw(&timers, &ref1);

	assert(interval);

	interval->base_time = base + 101;

	time_close_interval_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time intervals open raw cancel");

	time_lock(&timers);

	interval = time_open_interval_raw(&timers, &ref1);

	assert(interval);

	time_cancel_interval_raw(&timers, &ref1);
	time_close_interval_raw(&timers, &ref1);
	time_unlock(&timers);

	test_end();


	test_begin("time intervals open raw err");

	time_lock(&timers);

	interval = time_open_interval_raw(&timers, &ref1);

	assert(!interval);

	time_unlock(&timers);

	test_end();


	test_begin("time intervals cancel self");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	/*
	 * Even though the timer does not interfere with any of the next tests, we
	 * can't have this test fail asynchronously, because we won't know that this
	 * particular test failed and there will be big trouble finding the cause.
	 */
	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self min");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 0,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self avg");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 50,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self max");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 100,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel interval");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.self = &ref2,
		.res = 0
	};

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	struct time_timer ref3;

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref3,
		.base_time = time_immediately + time_step,
		.ref = &ref3
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time intervals cancel self raw 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self raw min 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 1,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self raw avg 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 51,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self raw max 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 99,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel interval raw 1");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.self = &ref2,
		.res = 0
	};

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref3,
		.base_time = time_immediately + time_step,
		.ref = &ref3
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time intervals cancel self raw 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self raw min 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 2,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self raw avg 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 49,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel self raw max 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 98,
		.self = &ref1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref1
	}
	)));

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref2,
		.base_time = time_immediately + time_step,
		.ref = &ref2
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel interval raw 2");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.self = &ref2,
		.res = 0
	};

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = wake_int,
		.data = &ref3,
		.base_time = time_immediately + time_step,
		.ref = &ref3
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();



	/*
	 * TIMEOUTS INTERVALS
	 */

	test_begin("time timeouts cancel interval");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data = (struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time timeouts cancel interval err");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = -1
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval min");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 3,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval avg");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 52,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval max");
	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 97,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval raw 1");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time timeouts cancel interval raw err 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = -1
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval min raw 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 4,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval avg raw 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 48,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval max raw 1");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 96,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_1,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval raw 2");

	time_lock(&timers);
	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = assert_0,
		.base_time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = 0
	};

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time timeouts cancel interval raw err 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = &ref1,
		.res = -1
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval min raw 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 5,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval avg raw 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 53,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
		(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time timeouts cancel interval max raw 2");

	data =
	(struct test_data)
	{
		.type = type_interval,
		.ref = refs_int + 95,
		.res = 0
	};

	assert(!time_add_timeout(&timers, &(
		(struct time_timeout)
	{
		.func = cancel_2,
		.data = &data,
		.time = time_immediately
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();



	/*
	 * INTERVALS TIMEOUTS
	 */

	test_begin("time intervals cancel timeout");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time intervals cancel timeout err");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.self = &ref2,
		.reverse = 1,
		.res = -1
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout min");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 3,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout avg");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 52,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout max");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 97,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout raw 1");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time intervals cancel timeout raw err 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.self = &ref2,
		.reverse = 1,
		.res = -1
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout min raw 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 4,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout avg raw 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 48,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout max raw 1");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 96,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_1,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout raw 2");

	time_lock(&timers);
	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = assert_0,
		.time = time_immediately + time_step * 2,
		.ref = &ref1
	}
	)));

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval_raw(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout_raw(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	time_unlock(&timers);
	test_wait();

	test_end();


	test_begin("time intervals cancel timeout raw err 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = &ref1,
		.self = &ref2,
		.reverse = 1,
		.res = -1
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout min raw 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 5,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout avg raw 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 53,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time intervals cancel timeout max raw 2");

	data =
	(struct test_data)
	{
		.type = type_timeout,
		.ref = refs + 95,
		.self = &ref2,
		.reverse = 1,
		.res = 0
	};

	assert(!time_add_interval(&timers, &(
	(struct time_interval)
	{
		.func = cancel_2,
		.data = &data,
		.base_time = time_immediately,
		.ref = &ref2
	}
	)));

	assert(!time_add_timeout(&timers, &(
	(struct time_timeout)
	{
		.func = wake,
		.time = time_immediately + time_step
	}
	)));

	test_wait();

	test_end();


	test_begin("time check");

	for(int i = 6; i < 48; ++i)
	{
		assert(!time_cancel_timeout(&timers, refs + i));
		assert(!time_cancel_interval(&timers, refs_int + i));
	}

	for(int i = 54; i < 95; ++i)
	{
		assert(!time_cancel_timeout(&timers, refs + i));
		assert(!time_cancel_interval(&timers, refs_int + i));
	}

	/*
	 * Make sure no unexpected timers are still up.
	 */
	test_sleep(time_sec_to_ms(safety_timeout));

	test_end();


	test_begin("time cleanup");

	time_lock(&timers);
	time_stop_sync(&timers);
	time_unlock(&timers);

	time_free(&timers);

	test_end();


	return 0;
}
