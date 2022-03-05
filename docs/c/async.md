# Event loop

This module implements a flexible event loop implementation.

Event loops are used for asynchronous notifications. Whatever supports `epoll` will perform better with it than if it was blocking in a separate thread (and that is why Nginx is faster than Apache), because `epoll` can gather state of thousands of file descriptors simultaneously. The same would require thousands of context switches in an implementation using threads.

This module's event loop implementation also has a way of "simulating" events for file descriptors, including the event loop itself. This mechanism is used to asynchronously stop the event loop's execution and to make sure a file descriptor's event is executed "safely", after all other file descriptors have been processed.

## Usage

Initialisation:

```c
void on_event(struct async_loop* loop, uint32_t events, struct async_event* event) {
  /* ... */
}

struct async_loop loop = {0};
loop.on_event = on_event;

int err = async_loop(&loop);

/* ... after stopping the loop ... */
async_loop_free(&loop);
```

The `async_loop_free()` function does not reset `loop.on_event`.

An async loop needs a dedicated thread for it to run:

```c
err = async_loop_start(&loop);
```

It can also be run manually via `(void) async_loop_thread(&loop)`. In that case, the loop can be broken out of using asynchronous stopping techniques mentioned below. Synchronous ones **WILL NOT** do the trick, because they will terminate the thread instead.

The loop's thread can be terminated using `async_loop_stop(&loop)`. This is a synchronous call - upon return, the thread will no longer exist and its resources will be freed to the underlying operating system.

An asynchronous termination that allows for `async_loop_thread()` to return gracefully is highly customizable:

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

If `async_loop_commit()` is called without any of the functions prior to it, it will release the loop thread's resources and stop the loop from executing. All of the functions can be used at the same time in any combination, but `async_loop_commit()` must be called last.

Under the hood, the function simply creates a special event for the loop. The exception is handled once all events are dealt with.

You can use `async_loop_reset(&loop)` to reset any commits and pushes. This is necessary if you want to do something like:

```c
/* ... init a loop ... */
async_loop_push_joinable(&loop);
async_loop_commit(&loop);
/* async_loop_reset(&loop); */
async_loop_thread(&loop);
/* ... blocking ... */

/* from other thread */
async_loop_commit(&loop);
```

Without the function, the last commit from the above code will also detach the loop's thread, which is not desired. With the function, the last pushes and commits are reset, so that the loop can still be used without freeing it and reinitialising.

A loop disables cancellability for itself. It is quite impossible to save the state and type of cancellability for the thread calling `async_loop_thread()` and then expecting the function to restore that and returning without getting cancelled, so instead, if the application desires, it should save the state and type of cancellability prior to calling the function, and restoring them after it returns.

This module operates on `struct async_event`'s rather than pure file descriptors:

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
err = async_loop_remove(&loop, &event2 /* or &event, because same fd */);
```

The event's pointer **MUST NOT** change after being added while the loop is running, unless you account for that by stopping the loop, modifying the event to set the new pointer, and restarting the loop, OR if you create a new event that will then edit the event's pointer (because then the exception will be handled after all other events, so no race condition). The pointer will be passed to the loop's `on_event` handler.

Events can be "simulated" after every other event has been processed:

```c
err = async_loop_create_event(&loop, &event);
```

In theory, any pointer can be passed in place of `struct async_event*`, but the underlying code is only utilising events, so there was never a need for `void*` pointers.

It is entirely up to the `on_event()` handler of the loop to decide how to deal with simulated events. Such an event will always have `events` set to `0`, `event` will be set to whatever was passed to `async_loop_create_event()`, and it will only be executed once.

Asynchronous means of stopping a loop rely on this mechanism to some degree, although they do not call the above function.

Multiple events can be added to the loop using:

```c
struct async_event events[2];
events[0] = event;
events[1] = event2;
err = async_loop_create_events(&loop, events, 2);
```