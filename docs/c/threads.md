# Thread, threads, thread pools

This module deals with everything thread-related.

Internally, the module is split into 3
parts - thread, threads, and thread pools.

The thread part deals with single-thread structure
implementation. That is very simple, doesn't require
barriers, a list of thread IDs, etc.

The threads part implements a structure that's
capable of handling any number of threads.

The thread pool part abstracts away the difference between
the 2 above and implements a thread-safe queue of jobs to do.

## Dependencies

- [`error.md`](./error.md)

## Thread

If you only need to manage one thread, use this part of the module.

There are 2 functions that are basically `pthread_create()`, but their return
values are conforming to the library (0 on success, -1 and errno on errors),
and they make use of the error facility to eliminate any potential errors:

```c
void* data = NULL;

void
func(void* data)
{
	/* ... do something ... */
}

pthread_t thread_id;

int err = pthread_start(&thread_id, func, data);

/* Or, with thread attributes */
pthread_attr_t attr = /* something */;

err = pthread_start_explicit(&thread_id, &attr, func, data);
```

The thread will start in the background, as normally expected from
`pthread_create()`. If you need to be sure that the thread is active
at the time your code is running, you can use pthread synchronization
techniques to wait for the thread to start. This part of the module
doesn't cover that, however you can achieve this implicitly by using
the `threads` section explained below.

If the thread is meant to stop itself, meaning you don't need to hold a
reference to it using `pthread_t`, you can drop the first argument:

```c
err = pthread_start(NULL, func, data);
```

In this case, you won't be able to use the 2 functions mentioned below.

There are 2 ways to stop a thread (from any thread, including **the** thread).
Pick the most suitable one:

- If you need the thread gone immediatelly and have it release its resources:

  ```c
  pthread_cancel_sync(thread_id);
  ```

  Just note that the call doesn't actually make the thread disappear in an
  instant - it blocks waiting for it to terminate. In **the** thread, it
  instantly shuts it down and does not return.

- If you don't need to wait for its termination, but still want to release
  its resources:

  ```c
  pthread_cancel_async(thread_id);
  ```

  Using this, the thread will be stopped in the background by the kernel. The
  function doesn't block. In **the** thread, this function **MAY** return,
  since it's asynchronous.

The use of `pthread_cancel()` is strongly disencouraged. If you ever need to
retrieve some kind of status or return value from a thread, instead of capturing
its return value via `pthread_join()`, consider passing a pointer to the thread
via the data argument, and then setting contents of the pointer to some value.

The above functions will only work if the target thread is cancellable
and has cancellation points or has asynchronous cancellation turned on.

Note that the following is undefined behavior:

```c
pthread_t self = pthread_self();

pthread_cancel_async(self);
pthread_testcancel();
```

That is because the cancellation request isn't guaranteed
to be delivered instantly. Instead, use:

```c
pthread_t self = pthread_self();

pthread_cancel_sync(self);
```

Cancellability of a thread can be manipulated with:

```c
pthread_cancel_on();
pthread_cancel_off();
```

If cancellability is turned off, `pthread_cancel_*()` functions will no longer
be able to cancel the thread. This may prove useful to guard critical sections
of a thread's code (so that for instance memory isn't corrupted or leaked).

You can also decide when a thread is cancelled:

```c
pthread_async_on();
pthread_async_off();
```

Turning off async cancellation (the default) means the thread
may only be cancelled upon executing a "cancellation point",
a list of which is available at the [pthreads(7) documentation](
https://man7.org/linux/man-pages/man7/pthreads.7.html). Turning
on asynchronous cancellation enables a cancellation request to
interrupt the thread at any point (except when cancellation is
disabled with `pthread_cancel_off()`).

## Threads

This part of the module exposes API that deals with multiple threads at the
same time.  **NONE** of the following structures and functions are thread safe.

The structure is made to be very similar to the single
thread part, althrough it needs to be zeroed:

```c
pthreads_t threads = {0};
```

The procedure of creating new threads is
very similar to single thread management:

```c
uint32_t number = 5;

int err = pthreads_start(&threads, func, data, number);

/* pthreads_start_explicit(&threads, &attr, func, data, number); */
```

To ensure safety and proper code flow, these functions **block** until the
requested number of threads are spawned. It is not possible to change this
behavior.

If creation of any of the requested thread fails, all other threads spawned
within the same function will be destroyed **before** executing their start
routine (the `func` parameter). Other threads in the structure, if any, will
not be destroyed.

Similarly to single thread part, there exist 2 functions which remove threads
from the structure: `pthreads_cancel_sync()` and `pthreads_cancel_async()`.
Their semantics are equal to their single thread counterparts, see above.
However, they do not remove *all* threads from the structure - only a given
number of them:

```c
/* Remove 5 threads from the structure */
pthreads_cancel_sync(&threads, 5);
```

It is undefined behavior to provide a value that exceeds
`threads.used` at the time of executing the function.

The requested number of threads are removed **from the end of the list of
threads**. It is not directly possible to remove any threads you would like.
If you wish to do so, you need to use the single thread part of this module
multiple times.

Shutting down a thread by yourself (returning from it) will not remove it from
the structure, however you can return from some or all of them, and then call
any of the `pthreads_cancel_*()` functions. The threads will then be removed.

To remove all threads from the structure, you can use `threads.used`
as the second argument to `pthreads_cancel_*()` functions, or use
`pthreads_shutdown_*()` functions, which are equal to their `cancel`
counterparts, but do not contain the second argument - they remove
all threads at the same time:

```c
pthreads_shutdown_sync(&threads);

/* You can be sure all of the threads
are not running anymore now. */
```

Finally, to free the list of threads and zero usage counters, you can use:

```c
pthreads_free(&threads);
```

It is fine to call `pthreads_cancel_async()` instead of
the synchronous counterpart right before `pthreads_free()`.

## Thread pool

The structure must be properly initialised first:

```c
struct thread_pool pool = {0};

int err = thread_pool(&pool);

/* Later on */
thread_pool_free(&pool);
```

The structure does not feature any thread or threads inside of itself. Instead,
you need to add new workers to the pool by spawning them with specific start
routine and data, or executing functions mentioned below to work manually:

```c
pthreads_t some_other_threads = {0};
//                                        ------------------  -----
err = pthreads_start(&some_other_threads, thread_pool_thread, &pool, 2);
//                                        ^^^^^^^^^^^^^^^^^^  ^^^^^
```

A thread pool's lock can be manipulated using the following functions:

```c
thread_pool_lock(&pool);

/* ... execute some raw() functions ... */

thread_pool_unlock(&pool);
```

If under a lock already, executing non-raw functions will lead to a deadlock.

All of the below functions exist in both thread-safe and unsafe versions.

To queue a new job, you can do:

```c
no_mem = thread_pool_add(&pool, func, data);
```

There is no function that bulk adds multiple jobs to the pool, use the above
instead. If you need to queue multiple jobs all the time, consider putting a
loop in the jobs' function so that it's performed multiple times. To further
improve performance, acquire the pool's lock and execute `thread_pool_add_raw()`
multiple times, after which unlock the pool's lock.

Jobs are executed outside of their pool's lock. Additionally, when they are
executed, they will already be gone from the beginning of their pool's list
of jobs.

If you want to manually execute some jobs and you
are not sure whether there are any jobs to do, do:

```c
thread_pool_try_work(&pool);
```

The above will not block waiting for new jobs.
If you want to wait for new jobs, do:

```c
thread_pool_work(&pool);
```

The blocking that the function performs (if there are no jobs)
is a cancellation point and can be disturbed by a signal.

The above will only execute one job.

You can infinitely work using:

```c
while(1)
{
	thread_pool_work(&pool);
}
```

The above is also how `thread_pool_thread` works.
