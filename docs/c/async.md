# Event loop

This module implements a wrapper around Linux's `epoll`.

Event loops are used for asynchronous notifications. Whatever supports `epoll`
will perform better with it than if it was blocking in a separate thread (and
that is why Nginx is faster than Apache), because `epoll` can gather state of
thousands of file descriptors simultaneously. The same would require thousands
of context switches in an implementation using threads (and more memory).

## Dependencies

- `error.md`
- `threads.md`

## Usage

Initialisation:

```c
void evt(struct async_loop* loop, uint32_t events, struct async_event* event) {
  /* ... */
}

struct async_loop loop = {0};
loop.on_event = evt;
loop.events_len = 32;

int err = async_loop(&loop);

/* ... after stopping the loop ... */
async_loop_free(&loop);
```

The `async_loop_free()` function does not
reset `loop.on_event` and `loop.events_len`.

The `loop.events_len` member specifies how many results the epoll can
return in a batch. If the value is not provided (its `0`), the default
of `64` is set instead. You should use higher values (in the thousands)
if you plan on using many file descriptors (tens, hundreds of thousands).
The default is only good for lots of very inactive sockets, or hundreds
of active ones.

If you don't provide `loop.events`, the underlying code will allocate
`sizeof(*loop.events) * loop.events_len` bytes of memory for internal usage.
If you have some spare memory that you want to use here, you can set
`loop.events` to the pointer and `loop.events_len` to the number of slots
it has. Note that in this setup, after `async_loop_free(&loop)` is called,
the memory pointer you set will not be freed. Only memory allocated internally
by the library will be freed, if any.

An async loop needs a dedicated thread for it to run:

```c
err = async_loop_start(&loop);
```

It can also be run manually via `(void) async_loop_thread(&loop)`. In that case,
the loop can be broken out of using the shutdown function mentioned below.

The loop's thread can be terminated using `async_loop_stop(&loop)`. This is
a synchronous call - upon return, the thread will no longer exist and its
resources will be freed to the underlying operating system. Do not use it if
you are calling the `async_loop_thread()` function manually - see below.

```c
async_loop_shutdown(&loop, 0);
```

The above function will asynchronously order the loop to stop running.
It is the only way to return from a manual call to `async_loop_thread()`.

The second argument accepts the following flags:

- `async_joinable` prevents the thread from becoming detached upon shutdown,

- `async_free` calls `async_loop_free(&loop)` once the shutdown is complete,

- `async_ptr_free` frees the `&loop` pointer once the shutdown is complete.

You can mix the above flags in any way by `OR`ing them together:

```c
async_loop_shutdown(&loop, async_joinable | async_free | async_ptr_free);
```

The above call will stop and free the event loop, but it will leave
the thread available for `pthread_join()` or `pthread_detach()` calls,
perhaps so that you can `join()` with it to wait until it's non existent.

The loop's thread will only be terminated when all events have been
dealt with. It cannot be stopped while it is dealing with them.

The loop disables cancellability for itself. The only ways of stopping it are
the synchronous `async_loop_stop()` or the asynchronous `async_loop_shutdown()`.

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
