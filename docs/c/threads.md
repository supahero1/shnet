# Thread, threads, thread pools

This module deals with everything thread-related.

Internally, the module is split into 3 parts - thread, threads, and thread pools.

The thread part deals with single-thread structure implementation. That is very simple, doesn't require barriers, a list of thread IDs, etc.

The threads part implements a structure that's capable of handling any number of threads.

The thread pool part abstracts away the difference between the 2 above and implements a queue of jobs to do.

## Warning

You should not use any functionality besides thead pools if you aren't familiar with threads, or otherwise you will very possibly run into multiple problems and memory leaks. Visit the [pthreads documentation](https://man7.org/linux/man-pages/man7/pthreads.7.html) to learn more about them and about API used in this module.

## Thread

If you only need to manage one thread, use this part of the module. There's little to none abstraction provided.

There exist 2 functions that are basically `pthread_create()`, but their return values are conforming to the library (0 on success, -1 and errno on errors), and they make use of the error facility to eliminate any potential errors:

```c
void* data = NULL;

void func(void* data) {
  /* ... do something ... */
}

pthread_t thread;

int err = pthread_start(&thread, func, data);
/* Or, with thread attributes */
err = pthread_start_explicit(&thread, &attr, func, data);
```

The thread will start in the background, as normally expected from `pthread_create()`. If you need to be sure that the thread is active at the time your code is running, you can use pthread synchronization techniques to wait for the thread to start. This module doesn't take care of that.

If the thread is meant to stop itself, meaning you don't need to hold a reference to it using `pthread_t`, you can drop the first argument:

```c
err = pthread_start(NULL, func, data);
```

There are 3 ways to stop that thread (from any thread, even **THE** thread). Pick the most suitable one:

- If you need the thread gone immediatelly and have it release its resources:

  ```c
  pthread_cancel_sync(thread);
  ```
  
  Just note that the call doesn't actually make the thread disappear in an instant - it blocks waiting for it to terminate. In **the** thread, it instantly shuts it down and does not return.
  
- If you don't need to wait for its termination, but still want to release its resources:

  ```c
  pthread_cancel_async(thread);
  ```
  
  Using this, the thread will be stopped in the background by the kernel. The function doesn't block. In **the** thread, this function **MAY** return, since it's asynchronous.
  
- If you don't need to wait for its termination and don't want to free its resources, or if you want to deal with the resources later:

  ```c
  pthread_cancel(thread);
  ```
  
  You will need to call either `pthread_join()` or `pthread_detach()` to free the thread.

All of the above functions also work for the `threads` part, and any threads overall, even if they weren't created using this module.

The above functions will only work if the target thread is cancellable and has cancellation points or has asynchronous cancellation turned on.

Note that the following is undefined behavior:

```c
pthread_t self = pthread_self();
pthread_stop_async(self);
pthread_testcancel();
```

That is because the cancellation request isn't guaranteed to be delivered instantly. Instead, use:

```c
pthread_t self = pthread_self();
pthread_stop_sync(self);
```

## Threads

This part of the module exposes API that deals with multiple threads at the same time. It is possible to manage just 1 thread like the `thread` section, but it will cost more memory, which is completely unnecessary when only in need of 1 thread. **NONE** of the following structures and functions are thread safe.

The structure is made to be very similar to the single thread part, althrough it needs to be zeroed:

```c
pthreads_t threads = {0};
```

The list of threads that the structure contains might not always be full, because it is not resized if threads are removed. You can periodically adjust the list to save memory using the following:

```c
uint32_t new_size = threads.used;
int no_mem = pthreads_resize(&threads, new_size);
```

The procedure of creating new threads is very similar to single thread management:

```c
uint32_t number = 5;
int err = pthreads_start(&threads, func, data, number);
/* pthreads_start_explicit(&threads, &attr, func, data, number); */
```

To ensure safety and proper code flow, these functions **block** until the requested number of threads are spawned. It is not possible to change this behavior. It is very unwise to try removing threads (not the ones that are being added) at the same time, although it is possible (by calling `pthreads_resize(&threads, thread.used + number)` before the above and not adjusting size of the list when deleting any threads).

If creation of any of the requested thread fails, all other threads spawned within the same function will be destroyed **before** executing their start routine (the `func` parameter). Other threads in the structure, if any, will not be destroyed.

Similarly to single thread part, there exist 3 functions which remove threads from the structure: `pthreads_cancel()`, `pthreads_cancel_sync()`, and `pthreads_cancel_async()`. Their semantics are equal to their single thread counterparts, see above. However, they do not remove *all* threads from the structure - only given number of them:

```c
/* Remove 5 threads from the structure */
pthreads_cancel_sync(&threads, 5);
```

It is undefined behavior to provide a value that exceeds `threads.used` at the time of executing the function.

The requested number of threads are removed **from the end of the list of threads**. It is not directly possible to remove any threads you would like. Moreover, it can be done indirectly by you:

```c
/* Removing the 4th thread */

pthread_t thread = threads.ids[3];
threads.ids[3] = threads.ids[--threads.used];
/* Do whatever you like with `thread` now */
```

In the above example, the fourth's thread ID is first saved to a variable to not lose it, following the last thread from the list being moved to the location of the fourth to close the gap, finishing it off by decreasing the number of used threads. It is fine to change the order in which threads appear in the list, however `pthreads_cancel_*()` might have a different effect. To negate this effect, at the cost of increased computational requirements, you can do:

```c
/* Safely removing the 4th thread */

pthread_t thread = threads.ids[3];
memmove(threads.ids + 3, threads.ids + 4, threads.used - 4);
--threads.used;
```

The above example will preserve the order in which threads appear, making `pthreads_cancel_*()` functions behave correctly, if needed.

To remove all threads from the structure, you can use `threads.used` as the second argument to `pthreads_cancel_*()` functions, or use `pthreads_shutdown_*()` functions, which are equal to their `cancel` counterparts, but do not contain the second argument - they remove all threads at the same time:

```c
pthreads_shutdown_sync(&threads);
/* You can be sure all of the threads
are not running anymore now. */
```

Finally, to free the list of threads and zero usage counters, you can use:

```c
pthreads_free(&threads);
```

It is OK to call `pthreads_cancel_async()` instead of the synchronous counterpart right before `pthreads_free()`.

## Thread pool

The structure must be properly initialised first:

```c
struct thread_pool pool = {0};
int err = thread_pool(&pool);

/* Later on */
thread_pool_free(&pool);
```

The structure does not feature any thread or threads inside of itself. Instead, you need to add new workers to the pool by spawning them with specific start routine and data, or executing functions mentioned below to work manually:

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

Traditionally, the list of jobs to do is not resized upon dequeuing jobs. You can do that using:

```c
uint32_t size = pool.used;
int no_mem = thread_pool_resize(&pool, size);
```

To queue a new job, you can do:

```c
no_mem = thread_pool_add(&pool, func, data);
```

There is no function that bulk adds multiple jobs to the pool, use the above instead. If you need to queue multiple jobs all the time, consider putting a loop in the jobs' function so that it's performed multiple times. To further improve performance, acquire the pool's lock and execute `thread_pool_add_raw()` multiple times, after which unlock the pool's lock.

Jobs are executed outside of their pool's lock. Additionally, when they are executed, they will already be gone from the beginning of their pool's list of jobs.

To remove all jobs:

```c
thread_pool_clear(&pool);
```

You can also manually adjust how many jobs you want there to be:

```c
thread_pool_lock(&pool);
pool.used = 5;
thread_pool_unlock(&pool);
```

Provided there were more before, this will shrink the number of jobs down to 5. After this, you can still change the limit upwards, up to the old limit, while under the lock. If you leave the lock, you **MUST NOT** increase the number of available jobs to do. Use `thread_pool_add()` instead.

If you are not sure whether there are any jobs to do, do:

```c
thread_pool_try_work(&pool);
```

The above will not block waiting for new jobs. If you want to wait for new jobs, do:

```c
thread_pool_work(&pool);
```

The blocking that the function performs (if there are no jobs) is a cancellation point and can be disturbed by a signal.

The above will only execute one job.

You can infinitely work using:

```c
while(1) {
  thread_pool_work(&pool);
}
```

The above is also how `thread_pool_thread` works.