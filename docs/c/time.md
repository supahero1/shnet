# The time facility

The objective of this module is to bring kernel timers to the userspace, making them from tens to thousands of times faster.

As of now (v5.17), the Linux kernel is using a red-black tree for storing timers, and managing them involves system calls, which can be costly. Additionally, 2 calls are required to initialise a timer (create & set time), which further decreases performance. This module boils down that cost to only 1 kernel timer per an unlimited number of userspace timers.

Moreover, this module is using a very efficient heap implementation, which has a little modification that allows deletions of known items to be `O(1)`, where "known items" means the application has a pointer to an item it wants to delete. To learn more about this modified heap, see `src_archive/refheap.c`, `docs/archive/refheap.md`, `tests_archive/refheap.c`, and the benchmark below.

On top of that, timers are small in size, and intervals (which are bigger than timeouts) have a separate heap so that timeouts don't need to be as big as them to share the same heap.

Run `make build-tests && bin/test_time_bench` to see how this module performs compared with native kernel timers. On my PC it yields the following:

```
Testing native timers creation... done
9765us
Testing native timers deletion... done
166886us
Testing native timers ALL... done
365397us
Testing shnet timers creation... done
830us
Testing shnet timers deletion... done
324us
Testing shnet timers ALL... done
1009us
Testing libuv timers creation... done
265us
Testing libuv timers deletion... done
1353us
Testing libuv timers ALL... done
1610us
```

Run `make build-tests CFLAGS="-DLIBUV -luv"` if you also want to test against libuv like above.

`ALL` means creation + expiration + deletion (implicit or explicit). The benchmark is using immediately-expiring timers.

When running the benchmark, you might also notice kernel timers are super unreliable and yield unpredictable timings everytime the benchmark is run. This module's timers don't have such a problem.

The benchmark is configured to run 10000 timers. You can modify that number using the `TEST_NUM` macro at the beginning of the file.

## Basic knowledge

Time is always expressed as a `uint64_t`.

This module features a lot of conversion functions in the format `time_x_to_y(uint64_t)`. Available values of `x` and `y` are `ns`, `us`, `ms`, and `sec`. `time_x_to_x()` functions do not exist.

Timers and time-fetching routines are using the `CLOCK_REALTIME` clock. It is not possible to change that. Changing it in the source code will lead to very unpleasant results.

Time can be fetched using the `time_get_time()` function. Returns a `uint64_t` in nanoseconds.

One can write `time_get_x(uint64_t)` instead of `time_get_time() + time_x_to_ns(uint64_t)`.

No time caching is done. All time-fetching functions return fresh values.

## Initialisation

```c
struct time_timers timers = {0};

int err = time_timers(&timers);

/* ... later, after stopping its thread ... */
time_timers_free(&timers);
```

Throughout the rest of this documentation, `struct time_timers` will be refered to as **time manager**.

The time manager needs a thread to keep track of timers:

```c
err = time_timers_start(&timers);
```

The thread can be manipulated as specified in `threads.md` via `timers.thread`:

```c
time_timers_stop(&timers); /* pthread_cancel_sync(timers.thread) */

time_timers_stop_async(&timers); /* pthread_cancel_async(timers.thread) */

time_timers_stop_joinable(&timers); /* pthread_cancel(timers.thread) */
```

Functions in the following section have thread-safe and unsafe versions. To run the raw versions in bulk, the time manager needs to be locked, and unlocked afterwards:

```c
time_lock(&timers);
/* ... time_xxx_raw(&timers) ... */
/* ... time_yyy_raw(&timers) ... */
/* ... time_zzz_raw(&timers) ... */
time_unlock(&timers);
```

## Timers

The kernel does not divide timers into timeouts and intervals. That is different for this module. Even though function semantics are basically the same for both, memory footprint plays a role here (an interval is 16 bytes larger than a timeout. That's 1.5x larger for 64bit, 2x for 32bit).

All timeout functions are exactly the same as their interval counterparts. Simply substitute `timeout` with `interval` and you are ready to go.

### Timeouts

When a timer is cancelled, and thus deleted from its underlying heap, the heap is not shrunk. To do so, or to set the heap's size to any arbitrary value, use:

```c
uint32_t new_size = timers.timeouts_used;
err = time_resize_timeouts(&timers, new_size);
```

The above shrinks the heap holding timeouts to the smallest size possible (so that there's no spare space).

A timer can be added to the time manager like so:

```c
void timer_cb(void* data) {
  /* ... do something cool with the data ... */
}

uint64_t expiration = time_get_sec(1);
void* data = &timers;

struct time_timeout timeout = (struct time_timeout) {
  .func = timer_cb,
  .data = data,
  .time = expiration /* absolute time */
};

err = time_add_timeout(&timers, &timeout);
```

The above adds a new timeout which will call `timer_cb()` with `data` as its argument in 1 second from now.

If you want a timeout to be fired "insta-asynchronously" (instantly, but once it's processed, so not necessarily **now**), **DO NOT** put small integers like `0` as `time`. Instead, use the macro `TIME_IMMEDIATELY`. For intervals, use `time_get_time()`, see below.

Timers are executed in the time manager's thread. Thus, to not delay other timers, if the function does a lot of stuff, it might be worth considering to create multiple time managers or offload the execution to a threadpool or by creating a new thread.

Timers are executed under a non-cancellable environment. Any cancellation requests for the time manager's thread will be handled after the current timer is dealt with as to not corrupt it.

Timers are executed outside of their time manager's lock.

To be able to cancel a timeout, i.e. remove it from its time manager before it is run, another member of a timer must be set:

```c
struct time_timer timer;

timeout = (struct time_timeout) {
  .func = timer_cb,
  .data = data,
  .time = expiration,
  
  .ref = &timer /* <-- */
};
```

The `struct time_timer` variable acts as a pointer to the node which holds the timeout. It also holds the timeout's state, so that it can "fail fast" when trying to cancel an already executed (or cancelled) timer. It's only 4 bytes in size.

After doing `time_add_timeout()` for the above timeout, it can be cancelled:

```c
err = time_cancel_timeout(&timers, &timer);
if(err) {
  /* Already cancelled or already executed */
}
```

Timers can also be "edited" on the fly before being executed. The application can change any members of a timer (**besides** `ref`):

```c
struct time_timeout* timeout_ = time_open_timeout(&timers, &timer);
if(timeout) {
  timeout_.time = TIME_IMMEDIATELY;
  timeout_.func = pthread_exit;
  timeout_.data = NULL;
  time_close_timeout(&timers, &timer);
}/*else {
  Already cancelled or already executed
}*/
```

Upon closing a timer, its position in its heap will be reevaluated to correct for any time changes. You **MUST NOT** close the timer if `time_open_timeout()` fails.

It is not possible to open a timeout currently being executed, because it is marked as executed before, not afterwards. Besides, it wouldn't make sense to do so. However, it is possible with intervals.

### Intervals

All of the above functions and structures can be used for intervals by simply substituting `timeout` with `interval`. However, the structure of intervals has different time members: `base_time`, `interval`, and `count` instead of `time`.

The next expiration of an interval is then `base_time + interval * count`. It is entirely up to you to set these 3 members however you see fit.

This functionality is analogous to the Linux kernel's `interval` and `value` fields of `struct itimerspec`. The beginning timeout (`value`) can be specified as `base_time + some_delay` or by increasing `count` above `0`, and the further `interval` is set using `interval`.

`count` is increased by 1 everytime its interval expires.

Any delays are corrected automatically by the underlying code. For instance, if `interval` is 20ms, but it expires in 30ms, the next expiration will be in 40ms to account for the delay.

`struct time_timer` is the same for both timeouts and intervals.

**DO NOT** set `base_time` to `TIME_IMMEDIATELY`, instead set it to `time_get_time()`.