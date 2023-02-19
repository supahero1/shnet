# Event loop

This module implements a wrapper around Linux's `epoll`.

Event loops are used for asynchronous notifications. Whatever supports `epoll`
will perform better with it than if it was blocking in a separate thread (and
that is why Nginx is faster than Apache), because `epoll` can gather state of
thousands of file descriptors simultaneously. The same would require thousands
of context switches in an implementation using threads (and more memory).

## Dependencies

- [`error.md`](./error.md)

## Usage

Initialisation:

```c
void
evt(struct async_loop* loop, uint32_t events, struct async_event* event)
{
	/* ... */
}

struct async_loop loop = {0};
loop.on_event = evt;
loop.events_len = 32;

int err = async_loop(&loop);
```

The `async_loop_free()` function does not
reset `loop.on_event` and `loop.events_len`.

The `loop.events_len` member specifies how many results the epoll can
return in a batch. If the value is not provided (its `0`), the default
of `64` is set instead. You should use higher values (in the thousands)
if you plan on using many file descriptors (tens, hundreds of thousands).
The default is only good for lots of very inactive sockets, or hundreds
of active ones.

An async loop needs to be started:

```c
err = async_loop_start(&loop);
```

It can also be run manually via `(void) async_loop_thread(&loop)`. In that case,
the loop can be broken out of using the shutdown function mentioned below.

If the loop has not been started yet, but the `async_loop()` function was
invoked, the proper way of freeing the structure is to call `async_loop_free()`.
However, if the loop has been already started, `async_loop_shutdown()` must be
used.

The function accepts flags as it's second argument.
You can choose for the shutdown to:

- `async_sync` - return synchronously only after everything has been dealt with,

- `async_free` - free the structure during the shutdown,

- `async_ptr_free` - free the pointer (`&loop`) during the shutdown.

You can also omit any flags. In that case, the loop will
only be stopped, and no further action will be taken:

```c
async_loop_shutdown(&loop, 0);
```

This is also how you should break out of `async_loop_thread()` function.

To specify multiple flags, OR them together.

The loop will only be terminated when all events have been
dealt with. It cannot be stopped while it is dealing with them.

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
