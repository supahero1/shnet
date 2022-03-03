# TCP

This module is an abstraction layer of Linux TCP sockets. It implements both client and server. In benchmarks, it's around the speed of iPerf3 and Nginx.

For examples of usage, see `tests/tcp.c` and `tests/tcp_bench.c`.

Run `make build-tests && bin/test_tcp_bench` to benchmark the module. The executable has a lot of options to customize.

## Rules

This module was made with a few assumptions that you must follow:
1. A socket may only be in 1 event loop at a time,
2. A socket may only be used by at most 1 user thread (besides from within event loop, i.e. from event handler).

All sockets use edge-triggered pooling. Because of that, if there is data to be read, you must either read it all or keep in mind that you must read it later. Data is accessible even after the connection is terminated.

In event handlers, only one event at a time may occur.

## Servers

You can think about servers as of simple factories which create new sockets. There isn't anything else they can do. The life cycle of such created sockets does not depend on the server's life cycle.

It is not possible to "limit incoming connections". If you want to create such a limit, use firewalls or other means - this module does not do that.

```c
struct tcp_socket*
handler(struct tcp_server* serv, enum tcp_event event, struct tcp_socket* sock) {
  switch(event) {
    /* ... */
    default: break;
  }
  return sock;
}

struct tcp_server server = {0};
server.on_event = handler;
/* server.loop = (struct async_loop); */
```

The `on_event` callback **must** be specified by the application. Possible events are:
- `tcp_open` when a new connection has arrived,
- `tcp_close` when `tcp_server_close()` is completed,
- `tcp_free` being the last event ever reported on the server. Since it's not accessed ever again by the underlying code, you can `free()` the server if it was allocated.

When `tcp_open` occurs, `sock` is non-NULL and contains the connection's file descriptor at `sock->core.fd`. You **MUST** allocate space for the new socket. **DO NOT** write anything to the newly allocated space - instead, write changes to the `sock` pointer. The underlying code will populate the returned pointer using `sock`.

Returning `NULL` from the callback will result in the connection being terminated on the spot. This is only true for the `tcp_open` event - all other events ignore the return value. You can use the return value to filter incoming connections, letting only the chosen ones through, however it's still better to do it using a firewall or other tools specifically designed for that.

You can initialise the socket by setting `sock->on_event` and other members mentioned below in the sockets' documentation.

Returning `sock` is a special case scenario which will make the underlying code allocate space for the socket. Upon freeing the socket with `tcp_socket_free()`, the socket will automatically be `free()`'d.

You can only safely call `tcp_server_free()` once the `tcp_close` event occurs on the server.

```c
struct addrinfo* info = /* ... DNS lookup ... */;
struct tcp_server_options options = (struct tcp_server_options) {
  .info = info,
  .hostname = "localhost",
  .port = "8080",
  .family = net_family_ipv4,
  .flags = net_flag_numeric_service,
  .backlog = 16
};

int err = tcp_server(&server, &options);
```

The above function can fail with errno `EINVAL` for any of the following reasons:
- `options` is `NULL`,
- `server.on_event` is `NULL`,
- `options.info` and `options.hostname` and `options.port` are all `NULL`,
- A double initialisation was detected.

`info` is optional if any of `hostname` and `port` are set and vice versa. If `info` is `NULL`, a synchronous DNS lookup will be performed based on the values of `hostname`, `port`, `family`, and `flags`.

If `backlog` is `0`, the default value of `32` will be set instead.

If `server.loop` is `NULL`, the underlying code will create a new event loop. It will be automatically freed upon `tcp_server_free()`.

If `options.port` is `"0"`, the operating system will pick a free port for the server. After the `tcp_server()` function is completed, you can retrieve the server's port using `tcp_server_get_port(&server)`.

The server's resources can be freed with `tcp_server_free(&server)`.

The server can be terminated using:

```c
int err = tcp_server_close(&server);
```

The function may fail if there's insufficient memory.

## Clients

```c
void sock_evt(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    /* ... */
    default: break;
  }
}

struct tcp_socket socket = {0};
socket.on_event = sock_evt;
/* socket.loop = (struct async_loop); */
```

Unlike servers, it's **NOT REQUIRED** to have the event callback set. This will be further explained below.

Possible events are:
- `tcp_open` when the connection is ready,
- `tcp_data` when new data arrives from the peer. This event also occurs if a `FIN` arrives, meaning end of data, so `tcp_read()` **MAY** return `0` bytes,
- `tcp_can_send` when it's possible to `tcp_send()` more data. In practice this event **SHOULD NOT** be used, because `tcp_send()` buffers any data that it can't send. This event is only meaningful for TCP extensions, such as TLS,
- `tcp_readclose` when the peer has sent a `FIN`, closing their channel. It is still possible to send data after this event,
- `tcp_close` when the connection is terminated,
- `tcp_free` being the last event ever reported on the socket. Since it's not accessed ever again by the underlying code, you can `free()` the socket if it was allocated.

It is possible for `tcp_close` to occur without `tcp_open` prior to it. This will happen when the connection couldn't be established and errno will be set to the reason why. If it's `0` or `EAGAIN`, the connection termination was likely graceful and controlled.

Inside of the event handler, it's possible to know if the socket is a client or not (created by a server) using `socket.core.server`. If it's `1`, it was created by a server. This is **NOT** possible during the `tcp_free` event, because the socket can't be touched after it occurs, so instead, `socket.core.socket` can be used. If it's `0`, the socket was created by a server. This single bit is not cleared upon a socket destruction, but it does not influence `tcp_socket()` so it does not matter - structure reuse is still possible.

If `socket.dont_close_onreadclose` is `1`, the connection will automatically be `tcp_socket_close()`'d upon the `tcp_readclose` event.

If `socket.dont_send_buffered` is `1`, any data that `tcp_send()` couldn't send will not be resent by the event loop when available. This doesn't mean the data will be deleted or such - it will remain untouched. This option is only meaningful with TCP extensions, such as TLS.

If `socket.dont_autoclean` is `1`, the data storage that `tcp_send()` is using to buffer unsent data will not be resized to eliminate empty space at the end of it. See `docs/c/storage.md` for more information.

```c
struct addrinfo* info = /* ... DNS lookup ... */;
struct tcp_socket_options options = (struct tcp_socket_options) {
  .info = info,
  .hostname = "localhost",
  .port = "8080",
  .family = net_family_ipv4,
  .flags = net_flag_numeric_service
};

int err = tcp_socket(&socket, &options);
```

The above function can fail with errno `EINVAL` for any of the following reasons:
- `options` is `NULL`,
- `options.info` and `options.hostname` and `options.port` are all `NULL`.

`info` is optional if any of `hostname` and `port` are set and vice versa. If `info` is `NULL`, an asynchronous DNS lookup will be performed based on the values of `hostname`, `port`, `family`, and `flags`.

If `socket.loop` is `NULL`, the underlying code will create a new event loop. It will be automatically freed upon the socket's destruction.

It is not guaranteed that the socket is connected after the `tcp_socket()` function, however you can already start queueing up data using `tcp_send()`:

```c
struct data_frame frame = (struct data_frame) {
  /* See docs/c/storage.md for more information */
};
int err = tcp_send(&socket, &frame);
```

Frames not declared with `dont_free` member set to `1` will be freed when they are no longer needed. That can happen when they were fully processed, or if the socket is destroyed.

The above function is thread-safe and can be called at the same time from the socket's event handler and from a user thread. If it fails in either, the data will be sent later in the same order `tcp_send()` was called.

You can probably ignore the return value of the function. If any fatal error occurs, the connection will be terminated. The only realistic error to watch out for is `ENOMEM`. Note that if it occurs, and since `error_handler` (described in `docs/c/error.md`) is retrying until it's unsuccessful, the connection **SHOULD** be closed, since data is lost in a sense. On top of that, if you are using `tcp_send()` from both event handler and user thread, the connection **MUST** be closed, because order of data may have been corrupted, unless you account for that.

The application can know how much data is unsent using:

```c
int unsent = tcp_socket_unsent_data(&socket);
```

However note that the above isn't necessarily meaningful. There may be multiple reasons why data might be still in the socket's buffer. Additionally, the above function only reports data from the kernel's socket buffer, not the library's data storage.

Data can be read from the socket via the following:

```c
uint8_t buffer[4096];
uint64_t read = tcp_read(&socket, buffer, sizeof(buffer));
```

Errors may occur within the above function, but they are not reported to the application. If you really must, you can check for them by setting errno to `0` before the call and checking it afterwards. In any case, no reading error is really dangerous. If any error occured and it was severe enough, the connection will simply be closed - there is no way of preventing that or retrying the function in such a case.

Reading data from the same socket from both event handler and user thread might lead to undefined behavior, because this operation is thread-unsafe and not protected by any lock, and so both threads may receive not consecutive data. Reading itself is completely thread-safe, unless the same `buffer` is used.

The application can know how much data there is to read using:

```c
int to_read = tcp_socket_unread_data(&socket);

read = tcp_read(&socket, buffer, to_read);
```

There are plenty of miscellaneous abstractions:

```c
tcp_socket_cork_on(&socket);
tcp_socket_cork_off(&socket);

tcp_socket_nodelay_on(&socket);
tcp_socket_nodelay_off(&socket);

/* 10 retries, 1sec reprobe_time, 1sec idle_time */
tcp_socket_keepalive(&socket);

tcp_socket_keepalive_explicit(&socket, idle_time, reprobe_time, retries);

/* Reject any more incoming data */
tcp_socket_dont_receive_data(&socket);
```

The connection can be closed gracefully using `tcp_socket_close(&socket)`. This function waits until all buffered data is sent and then sends a `FIN` to the peer, closing the channel. Afterwards, it waits for the peer to send back a `FIN`.

The above means that after `tcp_socket()`, data can be queued using `tcp_send()` and `tcp_socket_close()` can be called immediatelly afterwards. It will gracefully wait for all of the data to be sent before closing the connection.

If currently buffered data and data still in flight aren't important, the connection can be closed forcibly using `tcp_socket_force_close(&socket)`. This function will not wait for anything. It's likely to simply send a `RST` to the peer, terminating the connection.

You can indicate to the library that you will no longer use the socket in any way by calling `tcp_socket_free(&socket)`. It is **NOT** guaranteed that the socket's resources are released after this function completes. This is only a hint for the underlying code about when it can destroy the socket.

When called before the `tcp_close` event occurs (and not within it), the socket **WILL NOT** be freed. It will only be freed once the library finishes dealing with the socket and releases its connections with it.

When called from inside of or after the `tcp_close` event, the socket **WILL** be freed and ready to be used again once the function exits:

```c
void sock_on_event(/* ... */) {
  switch(event) {
    case tcp_close: {
      tcp_socket_free(sock);
      /*
      int err = tcp_socket(sock, &options);
      */
      break;
    }
    default: break;
  }
}
```

The asynchronous nature of both `tcp_socket_close()` and `tcp_socket_free()` can be used in conjunction to eliminate the dependency of `socket.on_event`:

```c
struct tcp_socket socket = {0};
assert(!tcp_socket(&socket, &((struct tcp_socket_options) {
  .info = info
})));
assert(!tcp_send(&socket, &((struct data_frame) {
  .data = ...,
  .len = ...
})));
tcp_socket_close(&socket);
tcp_socket_free(&socket);
```

The above code will connect, deliver the enqueued data, terminate the connection and free the socket, in this exact order. If a connection error occurs at any point of the code, the socket will still be freed.

The `tcp_socket_free()` function does not zero `socket.on_event`, `socket.dont_close_onreadclose`, `socket.dont_send_buffered`, and `socket.dont_autoclean`. Upon reuse, these will stay the same.