# The network abstraction

The goals of this module are:
1. Rename constants to be more intuitive,
2. Use the `error` module to create "safe" versions of
   some networking routines like `connect()` or `bind()`,
3. Remove ipv4 vs ipv6 dependency of some functions,
4. Implement an asynchronous DNS lookup functionality.

Not all constants and functions are covered. This module
is more of a bridge between the library and the operating
system rather than the application and the library.

## Dependencies

- `error.md`

## Constants

See `include/shnet/net.h`.

## Safe functions

```c
/* socket() */
int net_socket_get(struct addrinfo* addr);

/* bind() */
int net_socket_bind(int sfd, struct addrinfo* addr);

/* connect() */
int net_socket_connect(int sfd, struct addrinfo* addr);

/* setsockopt() */
int net_socket_setopt_true(int sfd, int level, int option);

/* setsockopt() */
int net_socket_setopt_false(int sfd, int level, int option);

/* getaddrinfo() */
struct addrinfo* net_get_address(char* host, char* port, struct addrinfo* hints);

/* freeaddrinfo() */
net_free_address(struct addrinfo* info);

/* Self explanatory */

void net_socket_reuse_addr(int sfd);

void net_socket_dont_reuse_addr(int sfd);

void net_socket_reuse_port(int sfd);

void net_socket_dont_reuse_port(int sfd);

int net_socket_get_family(int sfd);

int net_socket_get_socktype(int sfd);

int net_socket_get_protocol(int sfd);

/*
struct sockaddr_in6 addr;
void* addr_ptr = &addr;
*/

void net_socket_get_peer_address(int sfd, void* addr_ptr);

void net_socket_get_local_address(int sfd, void* addr_ptr);

void net_socket_dont_block(int sfd);

void net_socket_default_options(int sfd);
/* equivalent to:
net_socket_reuse_addr(sfd);
net_socket_reuse_port(sfd);
net_socket_dont_block(sfd); */
```

## Other functions

```c
char ip[net_const_ip_max_strlen];
struct sockaddr_in6 addr;

net_address_to_string(&addr, ip);

sa_family_t fam = net_address_to_family(&addr);

uint16_t port = net_address_to_port(&addr);

uint8_t* ipv6/*[16]*/ = net_address_to_ip(&addr);
```

## Asynchronous DNS

```c
void handler(struct net_async_address* async, struct addrinfo* info) {
  /* ... */
  net_free_address(info);
}

struct addrinfo hints = net_get_addr_struct(family, socktype, protocol, flags);

struct net_async_address async = (struct net_async_address) {
  .hostname = "localhost",
  .port = "8080",
  .hints = &hints,
  .data = NULL,
  .callback = handler
};

int err = net_get_address_async(&async);

/* sync version
int err = net_get_address("localhost", "8080", &hints);
*/
```
