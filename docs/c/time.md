# The time facility

The objective of this module is to bring kernel timers to the
userspace, making them from tens to thousands of times faster.

As of now (v5.17), the Linux kernel is using a red-black tree
for storing timers, and managing them involves system calls,
which can be costly. Additionally, 2 calls are required to
initialise a timer (create & set time), which further decreases
performance. This module boils down that cost to only 1 kernel
timer per an unlimited number of userspace timers.

Moreover, this module is using a very efficient heap implementation,
which has a little modification that allows deletions of known items
to be `O(log n)`, where "known items" means the application has a
pointer to an item it wants to delete. The pointer is updated
internally by the heap when elements are moved. To learn more
about this modified heap, see `src/archive/refheap.c`,
`docs/archive/refheap.md`, `tests/archive/refheap.c`,
and the benchmark below. This is kind of a compromise
between heaps and AVL trees.

On top of that, timers are small in size, and intervals (which
are bigger than timeouts) have a separate heap so that timeouts
don't need to be as big as them to share the same heap.

## Dependencies

- [`error.md`](./error.md)

- [`threads.md`](./threads.md)

## Basic knowledge

Time is always expressed as `uint64_t`.

This module features a lot of conversion functions in the format
`time_x_to_y(uint64_t)`. Available values of `x` and `y` are `ns`,
`us`, `ms`, and `sec`. `time_x_to_x()` functions do not exist.

Timers and time-fetching routines are using the `CLOCK_REALTIME`
clock. It is not possible to change that. Changing it
in the source code will lead to very unpleasant results
(very possibly 100% CPU usage when having at least one timer).

Time can be fetched using the `time_get_time(void)`
function. Returns `uint64_t` in nanoseconds.

One can write `time_get_x(uint64_t)` instead
of `time_get_time() + time_x_to_ns(uint64_t)`.

No time caching is done. All time
fetching functions return fresh values.

## Initialisation

```c
struct time_timers timers = {0};

int err = time_timers(&timers);

/* ... later, after stopping its thread ... */
time_free(&timers);
```

Throughout the rest of this documentation, `struct time_timers`
will be refered to as **time manager**.

The time manager needs a thread to keep track of timers:

```c
err = time_start(&timers);
```

The thread can be manipulated as specified in `threads.md` via `timers.thread`:

```c
/* pthread_cancel_sync(timers.thread) */
time_stop_sync(&timers);

/* pthread_cancel_async(timers.thread) */
time_stop_async(&timers);
```

All functions in the following section have thread-safe and unsafe versions. To
run the raw versions in bulk, the time manager needs to be locked, and unlocked
afterwards:

```c
time_lock(&timers);
/* ... time_xxx_raw(&timers) ... */
/* ... time_yyy_raw(&timers) ... */
/* ... time_zzz_raw(&timers) ... */
time_unlock(&timers);
```

The application can call `time_thread(&timers)` instead of `time_start(&timers)`
, however there is no way to stop the execution of the thread other than by
terminating it. Also note that if a timer that you run triggers an infinite
loop, you can call `time_thread()` from `main()` and then run your program
via `gdb`. Once you think the infinite loop is running, send an interrupt and
`gdb` will log the stack trace for the loop. Note that the interrupt arrives
to the `main()` function, so there would be no way to debug timers with `gdb`
if they were ran via `time_start()` (in another thread) (unless maybe if you
block the signal in `main()`, but doing that in a heavily multithreaded
environment is like trying to find a needle in a haystack, since the signal
can potentially reach any running thread).

## Timers

The kernel does not divide timers into timeouts and intervals. That is different
for this module. Even though function semantics are basically the same for both,
memory footprint plays a role here (an interval is 16 bytes larger than a
timeout. That's 1.5x larger for 64bit, 2x for 32bit).

All timeout functions are exactly the same as their interval counterparts.
Simply substitute `timeout` with `interval` and you are ready to go.

### Timeouts

A timer can be added to the time manager like so:

```c
void
timer_cb(void* data)
{
  /* ... do something cool with the data ... */
}


uint64_t expiration = time_get_sec(1);
void* data = &timers;

struct time_timeout timeout =
(struct time_timeout)
{
	.func = timer_cb,
	.data = data,
	.time = expiration /* absolute time */
};

err = time_add_timeout(&timers, &timeout);
```

The above adds a new timeout which will call `timer_cb()`
with `data` as its argument in 1 second from now.

If you want a timeout to be fired "insta-asynchronously" (instantly, but once
it's processed, so not necessarily **now**), **DO NOT** put small integers like
`0` as `time`. Instead, use the constant `time_immediately`. For intervals, use
`time_get_time()`, see below.

Additionally, if you want to launch a few timers with `time_immediately` as
their time, but still want to order them somehow, **DO NOT** add any arbitrary
values like `1` to their time. Instead, use `time_step`, which is the smallest
distinguishable time difference the library can handle.

Timers are executed in the time manager's thread. Thus, to not delay other
timers, if the function does a lot of stuff, it might be worth considering
to create multiple time managers or offload the execution to a thread pool
or by creating a new thread.

Timers are executed under a non-cancellable environment. Any cancellation
requests for the time manager's thread will be handled after the current timer
is dealt with as to not corrupt it. You can change that explicitly from the
timer's callback, see [threads.md](./threads.md).

Timers are executed outside of their time manager's lock.

The time manager's thread may already start waiting for the
timer specified in `time_add_timeout` even if a lock is held.

```c
time_lock(&timers);

assert(!time_add_timeout_raw(&timers, &timeout));

usleep(999999999);

time_unlock(&timers);
```

In the above scenario, the time manager will start waiting for the timer as soon
as it can after `time_add_timeout_raw()` even though the lock is not owned by it
at the moment. However, it will not be able to execute the timer's function if
the lock will still be held at that point.

To be able to cancel a timeout, i.e. remove it from its time
manager before it is run, another member of a timer must be set:

```c
struct time_timer timer;

timeout =
(struct time_timeout)
{
	.func = timer_cb,
	.data = data,
	.time = expiration,

	.ref = &timer /* <-- */
};
```

The `struct time_timer` variable acts as a pointer to the node which holds
the timeout. It also holds the timeout's state, so that it can "fail fast"
when trying to cancel an already executed (or cancelled) timer. It's only
4 bytes in size.

After calling `time_add_timeout()` for the above timeout, it can be cancelled:

```c
err = time_cancel_timeout(&timers, &timer);

if(err)
{
	/* Already cancelled or already executed */
}
```

The `timer` reference to the real timeout is valid
as long as it resides in memory. It never "expires".

Timers can also be "edited" on the fly before being executed. The
application can change any members of a timer (**excluding** `ref`):

```c
struct time_timeout* timeout_ = time_open_timeout(&timers, &timer);

if(timeout)
{
	timeout_.time = time_immediately;
	timeout_.func = pthread_exit;
	timeout_.data = NULL;

	time_close_timeout(&timers, &timer);
}
else
{
	/* Already cancelled or already executed. */
	/* DO NOT use time_close_timeout(&timers, &timer) here! */
}
```

Upon closing a timer, its position in its heap will be
reevaluated to correct for any time changes. You **MUST
NOT** close the timer if `time_open_timeout()` fails.

It is not possible to open a timeout currently being executed, because it
is marked as executed before, not after execution. Besides, it wouldn't
make sense to do so (and if you think otherwise, your application design
is probably broken). However, it is possible with intervals.

Opening a timer explicitly under a lock should look like this:

```c
time_lock(&timers);

timeout_ = time_open_timeout_raw(&timers, &timer);

if(timeout)
{
	/* Cool */

	time_close_timeout(&timers, &timer);
}

time_unlock(&timers);
```

Which is basically the same as the locked variant.

### Intervals

All of the above functions and structures can be used for intervals by simply
substituting `timeout` with `interval`. However, the structure of intervals has
different time members: `base_time`, `interval`, and `count` instead of `time`.

The next expiration of an interval is then `base_time + interval * count`.
It is entirely up to you to set these 3 members however you see fit.

This functionality is analogous to the Linux kernel's `interval` and `value`
fields of `struct itimerspec`. The beginning timeout (`value`) can be specified
as `base_time + some_delay` or by increasing `count` above `0`, and the further
`interval` is set using `interval`.

`count` is increased by 1 everytime its interval expires. Since it's
a 64bit value, there's no need to worry about it overflowing to 0.

```c
struct time_timer timer;

struct time_interval interval =
(struct time_interval)
{
	.func = timer_cb,
	.data = data,

	/* 1 second from now,
		then every 50ms */
  	.base_time = time_get_sec(1),
  	.interval = time_ms_to_ns(50),

  	/* the same as above:
  		.base_time = time_get_time(),
  		.interval = time_ms_to_ns(50),
  		.count = 20,
  	*/

	.ref = &timer
};
```

Any delays are corrected automatically by the underlying code. For instance,
if an interval is to be executed `20ms` from now, but it expires in `30ms` from
now, the next expiration will be in `40ms` from now to account for the delay.

`struct time_timer` is the same for both timeouts and intervals.

**DO NOT** set `base_time` to `time_immediately` to make the interval
be executed semi-instantly. Set it to `time_get_time()` instead.

If you do set it to `time_immediately`, the interval will be on a rampage,
executing constantly in a loop. However, this might be desirable if you plan
on cancelling the interval anyway, so that the extra call to fetch current
time in `time_get_time()` is redundant.

Intervals and timeouts do not have any barriers in between them. Intervals
can use any kind of timeout functions (create, open, close, cancel) from
within their callback and vice versa. There are no limitations.
