# The network abstraction

The goals of this module are:
1. Rename constants to be more intuitive,
2. Use the `error` module to create "safe" versions of
	some networking routines like `connect()` or `bind()`,
3. Remove ipv4 vs ipv6 dependency of some functions,
4. Implement an asynchronous DNS lookup functionality,
5. Make it easier to access socket properties, such as address and port.

## Dependencies

None.

## Dev dependencies

- [`error.md`](./error.md)

## Constants

From [`net.h`](../../include/shnet/net.h):

```c
enum net_consts
{
	/* FAMILIES */
	NET_FAMILY_ANY = AF_UNSPEC,
	NET_FAMILY_IPV4 = AF_INET,
	NET_FAMILY_IPV6 = AF_INET6,

	/* SOCKTYPES */
	NET_SOCK_ANY = 0,
	NET_SOCK_STREAM = SOCK_STREAM,
	NET_SOCK_DATAGRAM = SOCK_DGRAM,
	NET_SOCK_RAW = SOCK_RAW,
	NET_SOCK_SEQPACKET = SOCK_SEQPACKET,

	/* PROTOCOLS */
	NET_PROTO_ANY = 0,
	NET_PROTO_TCP = IPPROTO_TCP,
	NET_PROTO_UDP = IPPROTO_UDP,
	NET_PROTO_UDP_LITE = IPPROTO_UDPLITE,

	/* FLAGS */
	NET_FLAG_WANTS_SERVER = AI_PASSIVE,
	NET_FLAG_WANTS_CANONICAL_NAME = AI_CANONNAME,
	NET_FLAG_NUMERIC_HOSTNAME = AI_NUMERICHOST,
	NET_FLAG_NUMERIC_SERVICE = AI_NUMERICSERV,
	NET_FLAG_WANTS_OWN_IP_VERSION = AI_ADDRCONFIG,
	NET_FLAG_WANTS_ALL_ADDRESSES = AI_ALL,
	NET_FLAG_WANTS_MAPPED_IPV4 = AI_V4MAPPED,

	/* CONSTANTS */

	NET_CONST_IPV4_STRLEN = 16,
	NET_CONST_IPV6_STRLEN = 46,
	NET_CONST_UNIX_STRLEN = 109,
	NET_CONST_IP_MAX_STRLEN = NET_CONST_UNIX_STRLEN,

	NET_CONST_IPV4_SIZE = sizeof(struct sockaddr_in),
	NET_CONST_IPV6_SIZE = sizeof(struct sockaddr_in6),
	NET_CONST_UNIX_SIZE = sizeof(struct sockaddr_un),
	NET_CONST_ADDR_MAX_SIZE = sizeof(struct sockaddr_storage)
};
```

The last category, under `CONSTANTS`, means the following:

- `NET_CONST_IPV4_STRLEN`: the maximum length of an IPv4
	string, including the ending NULL terminator,

- `NET_CONST_IPV6_STRLEN`: the above, but for IPv6,

- `NET_CONST_UNIX_STRLEN`: the above, but for UNIX sockets,

- `NET_CONST_IP_MAX_STRLEN`: the maximum of the above 3, suitably aligned,

- `NET_CONST_IPV4_SIZE`: size of an IPv4 address, suitably aligned,

- `NET_CONST_IPV6_SIZE`: the above, but for IPv6,

- `NET_CONST_UNIX_SIZE`: the above, but for UNIX sockets,

- `NET_CONST_ADDR_MAX_SIZE`: the maximum of the above 3, suitably aligned.

Additionally, besides of the maximum address size, the module defines
a new type `net_address_t` that is capable of holding any address.

## Naming convention

To functions operating on sockets, either their name
or their argument names will not be randomly named:

- `socket`: client or server,

- `client`: only client,

- `server`: only server.

These names give you a hint about what the function expects to work correctly.
Sometimes, a function can work on both, but it doesn't really make sense for it
to be called for the other type - these corner cases will be mentioned in the
documentation.

Additionally, there's a slight distinction between `hints` and `info` throughout
this module. Namely, `hints` refers to a `struct addrinfo` that **does not**
contain a valid address and port, while `info` refers to a fully initialised
structure that one can use to retrieve the address, port, and perhaps also next
addresses in the singly linked list.

## DNS queries

This module supports both sync and async DNS queries.
But first of all, you will require a `hint`:

```c
struct addrinfo hints = net_get_addr_struct(
	/* Family */	NET_FAMILY_IPV4,
	/* Socktype */	NET_SOCK_STREAM,
	/* Protocol */	NET_PROTO_TCP,
	/* Flags */		NET_FLAG_WANTS_SERVER
);
```

Hints give the underlying code a clue about what kind of address to return.

Next, you can request a DNS query synchronously:

```c
struct addrinfo* info = net_get_address_sync(
	/* Hostname */	"127.0.0.1",
	/* Port */		"80",
	/* Hints */		&hints
);
```

You are then ready to use the returned `info` in other `net` functions.

The returned `struct addrinfo*` was allocated by the underlying
code. Once you are done using it, you must free it:

```c
net_free_address(info);
```

An asynchronous DNS query does not block you while it's
executing, but you need to specify a callback function:

```c
void
async_cb(struct net_async_address* async, struct addrinfo* info)
{
	/* User data is at async->data */

	/* ... do something with info ... */

	net_free_address(info); /* Optional, it doesn't have to be here */
}

net_async_t cb = async_cb; /* Callback type */
```

After that, you can perform the query:

```c
struct net_async_address addr =
(struct net_async_address)
{
	.hostname = "127.0.0.1",
	.port = "80",

	.hints = &hints,

	.data = NULL, /* User data */
	.callback = cb
};

int err = net_get_address_async(&addr);

if(err)
{
	/* Oopsie */
}
```

## Socket operations

The following don't really implement anything new, they only
wrap some safety measures around the original functions:

```c
/* socket() */
int
net_socket_get(const struct addrinfo* hints);


/* bind() */
int
net_socket_bind(int sfd, const struct addrinfo* info);


/* connect() */
int
net_socket_connect(int sfd, const struct addrinfo* info);


/* setsockopt() */
int
net_socket_setopt_true(int sfd, int level, int option_name);


/* setsockopt() */
int
net_socket_setopt_false(int sfd, int level, int option_name);


void
net_socket_reuse_addr(int sfd);


void
net_socket_dont_reuse_addr(int sfd);


void
net_socket_reuse_port(int sfd);


void
net_socket_dont_reuse_port(int sfd);


net_family_t
net_socket_get_family(int sfd);


int
net_socket_get_socktype(int sfd);


int
net_socket_get_protocol(int sfd);


void
net_socket_dont_block(int sfd);


void
net_socket_default_options(int sfd);
```

## Address manipulation

Once you get a hold on a socket file descriptor, for instance by using:

```c
int sfd = net_socket_get(&hints);
```

, you can get both the local and remote (peer) addresses for the socket:

```c
net_address_t local, peer;

net_socket_get_local_address(sfd, &local);
net_socket_get_peer_address(sfd, &peer);
```

After you obtain actually useful addresses, you can do some more operations:

```c
net_family_t family = net_address_to_family(&peer);


uint16_t port = net_address_to_port(&peer);


char str[NET_CONST_IP_MAX_STRLEN];

net_address_to_string(&peer, str);


if(family == NET_FAMILY_IPV4)
{
	uint32_t raw_addr = *(uint32_t*) net_address_to_ip(&peer);
	uint32_t addr = ntohl(raw_addr);
} /* Similar case with IPv6 */
```

Beware that `net_address_to_port()` and `net_address_to_ip()` return `0` and set
`errno` to `ENOTSUP` if they can't get the required info. That will happen for
instance if unsupported family is passed to the functions, or if you call
`net_address_to_port()` on a UNIX socket, since their address does not contain
information about their port.

Additionally, if an unsupported socket is passed to `net_address_to_string()`,
it will not modify the given buffer in any way, and will set `errno` as above.

`net_address_to_family()` never fails.
