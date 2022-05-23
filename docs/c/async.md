# Event loop

This module implements a wrapper around Linux's `epoll`.

Event loops are used for asynchronous notifications. Whatever supports `epoll`
will perform better with it than if it was blocking in a separate thread (and
that is why Nginx is faster than Apache), because `epoll` can gather state of
thousands of file descriptors simultaneously. The same would require thousands
of context switches in an implementation using threads.

## Dependencies

- `error.md`
- `threads.md`

## Usage

Initialisation:

```c
void on_event(struct async_loop* loop, uint32_t events, struct async_event* event) {
  /* ... */
}

struct async_loop loop = {0};
loop.on_event = on_event;
loop.events_len = 32;

int err = async_loop(&loop);

/* ... after stopping the loop ... */
async_loop_free(&loop);
```

The `async_loop_free()` function does not reset `loop.on_event`.

The `loop.events_len` member specifies how many results an epoll
can return in a batch. If the value is not provided (its `0`),
the default of `64` is set instead.

An async loop needs a dedicated thread for it to run:

```c
err = async_loop_start(&loop);
```

It can also be run manually via `(void) async_loop_thread(&loop)`. In that case,
the loop can be broken out of using asynchronous stopping techniques mentioned
below. Synchronous ones **WILL NOT** do the trick, because they will terminate
the thread instead.

In any other case where an external thread is running the loop,
the loop can be waited for using `pthread_join(loop.thread, NULL)`.

The loop's thread can be terminated using `async_loop_stop(&loop)`. This is
a synchronous call - upon return, the thread will no longer exist and its
resources will be freed to the underlying operating system.

An asynchronous termination that allows for `async_loop_thread()`
to return gracefully is highly customizable:

```c
/* Request that the thread doesn't
free its resources upon termination */
async_loop_push_joinable(&loop);

/* Request that the thread calls
async_loop_free(&loop) upon termination */
async_loop_push_free(&loop);

/* Request that the thread frees
the loop altogether using free(&loop) */
async_loop_push_ptr_free(&loop);

/* Push the changes */
async_loop_commit(&loop);
```

If `async_loop_commit()` is called without any of the functions prior to it,
it will release the loop thread's resources and stop the loop from executing,
without freeing the loop or its pointer.

All of the functions can be used at the same time in any
combination, but `async_loop_commit()` must be called last.

The loop's thread will only be terminated when all events have been dealt with.

The loop disables cancellability for itself.

This module operates on `struct async_event`'s
rather than pure file descriptors:

```c
struct async_event event = {0};
event.fd = open(...);

int err = async_loop_add(&loop, &event, EPOLLET | EPOLLIN);

/* ... */

struct async_event event2 = {0};
event2.fd = event.fd;
/* Set new pointer for it, along with new event mask */
err = async_loop_mod(&loop, &event2, EPOLLOUT);

/* ... */
err = async_loop_remove(&loop, &event2/*or &event, because same fd*/);
```

The event's pointer **MUST NOT** change after being added while the loop is
running, unless you account for that by stopping the loop, modifying the event
to set the new pointer, and restarting the loop. The pointer will be passed to
the loop's `on_event` handler and it is the only mean of carrying in user data
(besides of the file descriptor).
