# TCP

This module is an abstraction layer for Linux TCP sockets. It implements
both client and server. In benchmarks, it's around the native speed.

For examples of usage, see `tests/c/009_tcp.c`.

## Dependencies

- [`storage.md`](./storage.md)
- [`async.md`](./async.md)
- [`net.md`](./net.md)

## Dev dependencies

- [`error.md](./error.md)
- [`threads.md](./threads.md)

## Rules

This module was made with a few assumptions that you must follow:

1. A socket may only be in 1 event loop at a time,
2. A socket may only be used by at most 1 user thread
(besides from within event loop, i.e. from event handler).

All sockets (besides servers) use edge-triggered pooling. Because of that, if
there is data to be read, you must either read it all or keep in mind that you
must read it later. Data is accessible even after the connection is terminated.

In event handlers, only one event at a time may occur.

Events are handled using the `async` module (see `docs/c/async.md`). If you
want to use an event loop for TCP clients and servers, use `tcp_async_loop()`
for initialisation rather than `async_loop()`. The function sets the loop's
`on_event` callback and initialises it like normally.

The above also means that once memory for a server or a socket is allocated,
the pointer cannot change. Thus, it is impossible to have a `realloc()`-able
array of them. An array of pointers to them is fine.

## Clients

To use any socket, you must first know where to connect it to:

```c
struct tcp_socket_options {
  struct addrinfo* info;
  const char* hostname;
  const char* port;
  int family;
  int flags;
};

struct tcp_socket_options options = {0};
```

You have 2 choices:

- Let the underlying code do all the dirty job for you by simply initialising
  `hostname`, `port`, `family` (optional), and `flags` (optional) to something:

  ```c
  options.hostname = "github.com";
  options.port = "443";
  options.family = net_family_ipv4;
  /* Not required on newer kernels. */
  options.flags = net_flag_numeric_service;
  ```

- Do a DNS lookup by yourself. This has the advantage that you may then reuse
  the resulting `struct addrinfo` address elsewhere as well:

  ```c
  const struct addrinfo hints = net_get_addr_struct(net_family_ipv4,
                                                    net_sock_stream,
                                                    net_proto_tcp,
                                                    net_flag_numeric_service);
  struct addrinfo* info = net_get_address("github.com", "443", &hints);
  options.info = info;
  ```

After you make your choice, you then need to learn about events.

A TCP socket is notified about anything via its event handler - the
`on_event` member of a TCP structure. It is a function like such:

```c
void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    /* ... */
    default: break;
  }
}

struct tcp_socket socket = {0};
socket.on_event = sock_evt;
```

Clients are not required to have it set (it may be NULL), because
async ways of closing and freeing a socket exist in this library.
Let's assume the easiest case scenario and not set it for now:

```c
socket.on_event = NULL;

int err = tcp_socket(&socket, &options);
if(err) {
  /* Bad stuff. */
}
```

The above function initialises and connects the socket. It is an
async function - it does not block waiting for the socket to connect.

The function will fail, setting `errno` to `EINVAL`, if `info` and
`hostname` and `port` were all `NULL` or if `&options` was `NULL`.

The function also creates a new event loop if it was not specified.
You can create your own event loop for the purposes of TCP like so:

```c
struct async_loop loop = {0};
err = tcp_async_loop(&loop);
err = async_loop_start(&loop);

socket.loop = &loop;
/* tcp_socket() */
```

Note the `tcp_async_loop()` instead of the usual `async_loop()`.

A socket, even if it does not have `on_event` set, must have an
event loop, because internal code still responds to the socket's
events. That's why it's automatically created even if not specified.

If you plan on using many sockets and/or servers in your application, it
is in your best interest to reuse just a few event loops for all of them,
instead of creating one for each (which will waste lots of resources).

An eventless socket's only restriction is that it may not read data,
since it has no way of knowing when any will arrive. However, it may
still send things, close itself, and free itself.

Even though right after the call to `tcp_socket()` the socket will
most likely not be connected yet, the library acts like it is and
lets you do everything you could normally do. For instance:

```c
/* tcp_socket() */
struct data_frame frame = { /* see docs/c/storage.md */ };
err = tcp_send(&socket, &frame);
```

The above will queue up the data frame to be sent once the socket is connected.
You can queue as many data frames as you want, there is no size or number limit.

The function only returns an error if further calls to it would result in an
error too. That is, if the socket is closed, or if there is no memory. In all
other scenarios, it will pretend like everything is fine, even if there is an
error pending on the socket. That is because all errors are eventually resolved
in the event loop, so resolving them in side functions like these does not make
sense.

In case of an error, if the data wasn't sent due to the connection being dead,
`errno` will always be set to `EPIPE` (and nothing else, such as `ECONNRESET`).
For other errors, other respective codes will be set. If `frame.free_onerr` was
set, the frame will additionally be freed in the function before returning. If
the return value is `0`, indicating no errors, `errno` must be `0` too.

The function is asynchronous, like most functions in
this module. It does not wait for the data to be sent.

You can also retrieve the number of bytes that are awaiting to be sent:

```c
uint64_t bytes = tcp_socket_send_buf_len(&socket);
```

The return value does not include the number of
bytes that were sent, but not yet ACKed by the peer.

Next up, you can close the socket:

```c
tcp_socket_close(&socket);
```

This call will trigger an asynchronous disconnect on the socket. You won't be
able to send any more data, but you will be able to receive data from the peer.
Similarily to `tcp_send()`, you can call this function right after connecting.
The function will first wait for all queued data to be sent and only then will
it actually close the connection. When the disconnection happens depends on the
peer, because the connection will be kept alive until the peer finishes sending
their data. This function has no effect if a connection error prevents the
socket from opening successfully.

Sometimes, waiting for all data to be sent might not be
wanted, so there exists a faster version of the function:

```c
tcp_socket_force_close(&socket);
```

This function does not wait for any data - it instantly disconnects the socket.
It is asynchronous too, so you need to wait for the appropriate event to be sure
the connection is no longer up.

After closing the connection, you will also want to free
data associated with the socket, created on `tcp_socket()`:

```c
tcp_socket_free(&socket);
```

Yet again, this function is asynchronous. Whether or not it frees the socket
depends on its current state. If the socket has not been closed yet (as in, the
appropriate event wasn't dispatched yet, officially ending the connection), it
will only mark the socket as "freeable", so that later on, when disconnection
occurs, it will be freed automatically by the underlying code. Otherwise, if
the socket is already past that point, the function will free it instantly.

Of course, in a real app situation you can't really count on either of those.
You **MUST** assume the worst case scenario (that the socket will be freed now)
and not access the socket after the function call completes.

You can choose not to free the socket right after the connection is closed,
for instance because there still may be data in the read buffer, which you
can use even after the socket is disconnected. The socket will not be freed
anywhere internally until you call the above function, so you can do whatever
you want to the socket before that happens.

Combining all of the above together, you can do something like the following:

```c
struct tcp_socket socket = {0};
assert(!tcp_socket(&socket, &((struct tcp_socket_options) {
  .hostname = "127.0.0.1",
  .port = "8080"
})));
char* data = "Hello, world!";
assert(!tcp_send(&socket, &((struct data_frame) {
  .data = data,
  .len = strlen(data),
  .dont_free = 1,
  .read_only = 1
})));
tcp_socket_close(&socket);
tcp_socket_free(&socket);
```

Note that the order of the functions matters. For instance, if you
close the connection before sending, you won't be able to send anymore.

That's pretty much the minimal example featuring this module's
TCP client. However, that's without any event handlers.

Let's go back to a code snippet from above:

```c
void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    /* ... */
    default: break;
  }
}

struct tcp_socket socket = {0};
socket.on_event = sock_evt;
```

To fully utilise the client, you will need to listen to some events. All of
them are prefixed with `tcp_`. Only one event may be reported at a time.

`tcp_open` is the first one. It is called when the peer agrees on the
connection. That is, it might not be called - if some error caused the
connection to not be established, this event will never be dispatched.
Use this event to initialise any data you want the socket to carry:

```c
struct game_client {
  struct tcp_socket tcp;

  float x;
  float y;

  uint8_t* packets;
};

void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  struct game_client* client = (struct game_client*) sock;
  switch(event) {
    case tcp_open: {
      client->packets = shnet_malloc(256);
      if(!client->packets) tcp_socket_force_close(sock);
      break;
    }
    case tcp_deinit: {
      free(client->packets);
      break;
    }
    default: break;
  }
}

struct game_client client = {0};
client.tcp.on_event = sock_evt;
int err = tcp_socket(&client.socket, /* ... */);
```

You should never initialise any data that you want to
be part of the socket outside of the `tcp_open` event.

Next up, the `tcp_deinit` event. It is basically the reverse of
`tcp_open`, letting you free whatever data you initialised in the
event. It doesn't have much other use. It is called right before
the socket is freed after `tcp_socket_free()` goes into effect.

The `tcp_close` event is called when the connection is officially dead. The
event will also be called when connecting fails, in which case `errno` should
be set to a meaningful value to help debug why the socket couldn't connect.
You will want to call `tcp_socket_free()` from this event if you don't plan
on using the socket after it is disconnected. You might want to plan on keep
using it if, for instance, you know there is some important data waiting for
you in the socket's read buffer. Often you will find HTTP clients sending you
a response and disconnecting right after, but you will still want to read the
response. That data will be available until you call `tcp_socket_free()`.

You can know when new data arrives at the socket by listening to the `tcp_data`
event. Beware though that the event may be dispatched even if, "apparently", no
data is available. That may happen for multiple reasons, and you should expect
it at least once during a socket's lifetime.

That's where a new function comes in:

```c
uint8_t buffer[4096];
uint64_t read = tcp_read(&socket, buffer, 4096);
```

The above function will copy max `4096` bytes of data to `buffer` from the given
socket's internal data queue. This function is absolutely not thread-safe (all
other ones mentioned above are thread-safe), so you must decide if you want to
call it from the event handler or a user thread (or implement locks and do it
both ways). You should keep calling the function until `read` is below the third
argument you passed to the function. Since all sockets in this module are edge-
triggered, if 2kb data arrives at the socket and you only read 1kb, there won't
be another event to remind you of the 1kb that's left - it's entirely up to you
to drain the socket correctly. For instance:

```c
uint8_t buffer[4096];
uint64_t buffer_len = 0;

void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_data: {
      while(1) {
        const uint64_t to_read = 4096 - buffer_len;
        const uint64_t read = tcp_read(sock, buffer, to_read);
        buffer_len += read;
        if(read < to_read) {
          break;
        }
      }
      break;
    }
    default: break;
  }
}
```

In the above example, if the socket ever receives more than `4095` bytes of
data, it will fall into an infinite loop, but if you don't expect more than
that, you can just close the socket with the reason of an invalid client:

```c
if(to_read <= 1) {
  tcp_socket_force_close(sock);
  break;
}
```

Otherwise, you might need to do other tricks
to make sure the socket is properly drained.

You can get the number of bytes waiting to be read:

```c
uint64_t bytes = tcp_socket_recv_buf_len(&socket);
```

..., and then use that number as an argument to `tcp_read()` to drain
everything in one call. However, note that the number might increase in
the meantime (between the call to the above function and `tcp_read()`),
so you must not assume you will always need only 1 read call.

Next, `tcp_free`. This event is the last event ever called on a socket. It only
exists so that you can `free()` the socket if it was allocated, or do anything
else that requires the underlying code not to access the code anymore:

```c
void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_close: {
      tcp_socket_free(sock);
      break;
    }
    case tcp_free: {
      free(sock);
      break;
    }
    default: break;
  }
}

struct tcp_socket* socket = shnet_calloc(1, sizeof(struct tcp_socket));
socket->on_event = sock_evt;
/* Initialise */
```

`tcp_readclose` and `tcp_can_send` are advanced events
that are to be used by experienced users only.

The first one occurs when the socket receives a FIN from the peer. By default,
a socket will automatically close itself when that event is received. To prevent
that behavior, you can set `socket.dont_close_onreadclose` to `1`. The event is
called before the socket is automatically closed. To close the socket under this
event, `tcp_socket_close()` is used to wait for any buffered data.

The second event is called whenever the kernel informs the userspace that the
socket's send queue has some free space. It can be called for really a bunch of
various reasons in a bunch of various scenarios, mostly to let the application
call `tcp_send()` on the socket to send any buffered data that couldn't be sent
previously due to size limit of the kernel's send buffer. If you don't want the
underlying code to send buffered data automatically upon this event, set the
`socket.dont_send_buffered` member to `1`. Morever, this event is used in TCP
extensions like TLS, and extensions may require you to set `dont_send_buffered`
to `1`, or they will set it by themselves and warn you not to change it. The
event, like `tcp_readclose`, is called before any action is taken. In this case,
before any buffered data is sent. You can use that to your advantage to modify
the buffered data.

A client's send queue is type of a queue that normally does not shrink itself
when data is removed from it, so that performance may be improved. However, by
default, that behavior is countered by this module by automatically cleaning up
the underlying queue's unused data chunks to decrease memory usage. If you know
what you are doing, you can set `socket.dont_autoclean` to `1` to disable this
behavior, possibly bringing some performance gains depending on the usage case.
The send data storage is located at `socket.queue`. You will likely need to
periodically clean it up by yourself. See `docs/c/storage.md` for more info.

There are also a few miscellaneous functions that clients can utilise.

When sending data, it might be preferable to send multiple frames instead of
trying to merge them in one. In that case, corking may be used to try to improve
the network performance by sending multiple TCP data frames at once:

```c
tcp_socket_cork_on(&socket);
/* Multiple tcp_send(&socket, ...) */
tcp_socket_cork_off(&socket);
```

Note that you should always pair `cork_on` with `cork_off`. If you
don't, you may introduce big delays to the connection, up to `200ms`.

In some precise usage cases, it might be worth sending data as soon as possible,
without bundling multiple pieces of data into one like corking does. For that,
you may do:

```c
tcp_socket_nodelay_on(&socket);
/* tcp_send(&socket, ...) */
tcp_socket_nodelay_off(&socket);
```

Generally, turning off delay (`nodelay_on`) has negative impact on a connection,
but if its only done for a few messages, it should be fine. For instance, you
may use it to speed up a TLS handshake by disabling delay for the first few
handshake messages and then reenabling it after the connection is established.

You may also want to make sure the peer does not suddenly hang up without
sending any FIN message to denote end of the connection. That may happen if
a user suddenly loses internet connection. To prevent that, you can enable
TCP's keepalive:

```c
assert(!tcp_socket(&socket, /* ... */));
tcp_socket_keepalive_on(&socket);
```

By default, that will set keepalive to send a probe every 1 second after
1 second of inactivity, with 10 probes being the maximum. It will also set
`TCP_USER_TIMEOUT` to help with closing a dead connection. You can set your
own settings using:

```c
const int idle_time_until_first_probe = 2; /* 2 seconds */
const int interval_between_probes = 1; /* send probes every 1 second */
const int retries = 5; /* close the connection after 5 probes */
tcp_socket_keepalive_on_explicit(&socket,
                                 idle_time_until_first_probe,
                                 interval_between_probes,
                                 retries);
```

If you wish to reuse a socket for many connections,
you may only do so after freeing the socket:

```c
void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_close: {
      tcp_socket_free(sock);
      break;
    }
    case tcp_free: {
      assert(!tcp_socket(sock, &((struct tcp_socket_options) {
        .hostname = "127.0.0.1",
        .port = "13579"
      })));
      break;
    }
    default: break;
  }
}
```

## Servers

A TCP server is basically a factory of TCP clients. The clients' lifetime
is not connected to the parent server's lifetime, they exist separately.

To run a server, you first need an address. The structure is pretty much the
same as `struct tcp_socket_options`, with the additional `backlog` member:

```c
struct tcp_server server = {0};
int err = tcp_server(&server, &((struct tcp_server_options) {
  .hostname = "127.0.0.1",
  .port = "8080",
  .backlog = 128
}));
```

`backlog` defines how big the server's client queue is. The larger it is, the
more connections it may accept in a given moment. If it's too small, clients
may need to retry multiple times before finally connecting to the server. If
it's not specified (specified `0`), the default value of `32` is used instead.

After the above function exits, you can then retrieve the server's port at
any point during its lifetime (before `tcp_socket_free()` is called) using:

```c
const uint16_t port = tcp_server_get_port(&server);
```

Servers, unlike clients, must have their event handler set, so the
`tcp_server()` function will actually fail with `errno` set to `EINVAL`.
A server's event handler is quite more complicated than a client's:

```c
struct tcp_socket*
evt(struct tcp_server* serv, struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    /* ... */
    default: break;
  }
  return sock;
}

server.on_event = evt;
```

After a server is initialised, it can be then closed using
`tcp_server_close(&server)`. This function is, yes, you guessed
it, asynchronous, and so you get to know about it via the event
handler:

```c
struct tcp_socket*
evt(struct tcp_server* serv, struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_close: {
      tcp_server_free(serv);
      break;
    }
    default: break;
  }
  return sock;
}
```

`tcp_server_free(&server)` is, yeah, you did not guess it, **NOT** async,
and it may not be called from anywhere else but the `tcp_close` event.

The `tcp_deinit` event is called before `tcp_server_free(&server)` goes into
effect. It's purpose is to let you clean up anything that was allocated for
the server before it is destroyed. `tcp_free` is called after everything is
gone. After the event is fired, the server will not be accessed again by the
underlying code. You may use it to your advantage to `free()` it if it was
dynamically allocated.

New connections are reported on the server via the `tcp_open` event. This event
is also why the event handler has a return value - it is ignored for every other
event besides `tcp_open`. That is because this event can do a whole lot of
things. First of all, you can return `NULL` straightforwardly like so:

```c
struct tcp_socket*
evt(struct tcp_server* serv, struct tcp_socket* sock, enum tcp_event event) {
  return NULL;
}
```

This way (let's ignore the fact that we can't free the server now), every new
connection will be dropped. It's way better to use firewall for this, but in
case you wanted to filter out connections, that's the way to do it. You can
access the new socket's file descriptor via `sock->core.fd`, and from there
you can pretty much access any information about it (see `docs/c/net.md`).

Next, you can choose to return something meaningful. In this case, you let the
connection proceed, and the underlying code will initialise the socket. However,
you have the choice to decide where the newly created socket will be in memory.
You have 2 options:

- Return `sock`, which is equivalent to letting the underlying code initialise
  the socket. In this case, the allocated memory will be automatically freed by
  the underlying code when `tcp_socket_free()` goes into effect on the socket.

- Return anything else but `NULL` and `sock`. In that case, the pointer must be
  a valid accessible memory area that the socket will be initialised in by the
  underlying code. You **MUST NOT** write anything to the newly allocated space.
  Instead, do necessary initialisations on the `sock` pointer (like setting its
  `on_event` handler or other constant members). If you allocated more than the
  socket needs so that you can use the extra space to carry any extra data you
  want, you **MUST NOT** initialise it in any way (besides setting **CONSTANT**
  members). You must then initialise the remaining things you did not initialise
  in **THE SOCKET'S** `tcp_open` event.

There are **NO GUARANTEES** that the newly created socket's `tcp_open` event
will be fired. If it won't be fired, no other event will be ever called to
notify about the connection disappearing. This may be caused by any internal
error that prevented initialisation of the socket. That's why it's important
to only initialise any external socket data in the `tcp_open` event. Because
otherwise, you won't have any way of freeing it if something goes south. If
no internal error happens, `tcp_open` is guaranteed to be dispatched on the
newly created socket.

You are **NOT** allowed to call any library functions on the `sock` socket
(meaning any of `tcp_send()`, `tcp_read()`, `tcp_socket_close()`, etc),
however you are allowed to get general information about the socket using
the `net` module's functions (eg. `net_socket_get_family(sock->core.fd)`),
since they are not affecting the memory the socket is occupying in any way.

After its creation, a socket is not bound to its server in any way. The server
may be closed and none of its child sockets will be affected and vice versa.

If, during a socket's initialisation in `tcp_open`, you don't set `sock->loop`,
it will automatically be set to `serv->loop` by the underlying code.

The newly created sockets **MUST** have an event callback `on_event` set,
because otherwise you won't be able to call `tcp_socket_free()` on it, which
will lead to memory leaks.
