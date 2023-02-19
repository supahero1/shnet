#include <shnet/net.h>
#include <shnet/error.h>
#include <shnet/threads.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <linux/ip.h>
#include <arpa/inet.h>


struct addrinfo
net_get_addr_struct(const int family, const int socktype,
	const int protocol, const int flags)
{
	return
	(struct addrinfo)
	{
		.ai_family = family,
		.ai_socktype = socktype,
		.ai_protocol = protocol,
		.ai_flags = flags
	};
}


struct addrinfo*
net_get_address(const char* const hostname,
	const char* const port, const struct addrinfo* const hints)
{
	struct addrinfo* addr;
	int err;

	safe_execute(
		err = getaddrinfo(hostname, port, hints, &addr),
		err,
		err == EAI_SYSTEM ? errno : err
	);

	if(err != 0)
	{
		if(err != EAI_SYSTEM)
		{
			errno = err;
		}

		return NULL;
	}

	return addr;
}



static void*
net_get_address_thread(void* net_get_address_thread_data)
{
	struct net_async_address* const addr = net_get_address_thread_data;

	addr->callback(
		addr,
		net_get_address(addr->hostname, addr->port, addr->hints)
	);

	(void) pthread_detach(pthread_self());

	return NULL;
}


int
net_get_address_async(struct net_async_address* const addr)
{
	return pthread_start(NULL, net_get_address_thread, addr);
}


void
net_free_address(struct addrinfo* const info)
{
	freeaddrinfo(info);
}



void
net_address_to_string(const void* const addr, char* const buffer)
{
	if(net_address_to_family(addr) == net_family_ipv4)
	{
		(void) inet_ntop(
			net_family_ipv4,
			&((struct sockaddr_in*) addr)->sin_addr.s_addr,
			buffer,
			net_const_ipv4_size
		);
	}

	(void) inet_ntop(
		net_family_ipv6,
		((struct sockaddr_in6*) addr)->sin6_addr.s6_addr,
		buffer,
		net_const_ipv6_size
	);
}


sa_family_t
net_address_to_family(const void* const addr)
{
	return ((struct sockaddr*) addr)->sa_family;
}


uint16_t
net_address_to_port(const void* const addr)
{
	if(net_address_to_family(addr) == net_family_ipv4)
	{
		return ntohs(((struct sockaddr_in*) addr)->sin_port);
	}

	return ntohs(((struct sockaddr_in6*) addr)->sin6_port);
}


void*
net_address_to_ip(const void* const addr)
{
	if(net_address_to_family(addr) == net_family_ipv4)
	{
		return &((struct sockaddr_in*) addr)->sin_addr.s_addr;
	}

	return ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr;
}



int
net_socket_get(const struct addrinfo* const info)
{
	int err;

	safe_execute(
		err = socket(info->ai_family, info->ai_socktype, info->ai_protocol),
		err == -1,
		errno
	);

	return err;
}


int
net_socket_bind(const int sfd, const struct addrinfo* const info)
{
	int err;
	socklen_t len = net_const_ipv4_size;

	if(info->ai_family == net_family_ipv6)
	{
		len = net_const_ipv6_size;
	}

	safe_execute(err = bind(sfd, info->ai_addr, len), err == -1, errno);

	return err;
}


int
net_socket_connect(const int sfd, const struct addrinfo* const info)
{
	int err;
	socklen_t len = net_const_ipv4_size;

	if(info->ai_family == net_family_ipv6)
	{
		len = net_const_ipv6_size;
	}

	safe_execute(err = connect(sfd, info->ai_addr, len), err == -1, errno);

	return err;
}


int
net_socket_setopt_true(const int sfd, const int level, const int option_name)
{
	int err;
	int true = 1;

	safe_execute(
		err = setsockopt(sfd, level, option_name, &true, sizeof(int)),
		err == -1,
		errno
	);

	return err;
}


int
net_socket_setopt_false(const int sfd, const int level, const int option_name)
{
	int err;
	int false = 0;

	safe_execute(
		err = setsockopt(sfd, level, option_name, &false, sizeof(int)),
		err == -1,
		errno
	);

	return err;
}


void
net_socket_reuse_addr(const int sfd)
{
	(void) net_socket_setopt_true(sfd, SOL_SOCKET, SO_REUSEADDR);
}


void
net_socket_dont_reuse_addr(const int sfd)
{
	(void) net_socket_setopt_false(sfd, SOL_SOCKET, SO_REUSEADDR);
}


void
net_socket_reuse_port(const int sfd)
{
	(void) net_socket_setopt_true(sfd, SOL_SOCKET, SO_REUSEPORT);
}


void
net_socket_dont_reuse_port(const int sfd)
{
	(void) net_socket_setopt_false(sfd, SOL_SOCKET, SO_REUSEPORT);
}


int
net_socket_get_family(const int sfd)
{
	int ret;
	socklen_t optlen =
	(socklen_t)
	{
		sizeof(int)
	};

	(void) getsockopt(sfd, SOL_SOCKET, SO_DOMAIN, &ret, &optlen);

	return ret;
}


int
net_socket_get_socktype(const int sfd)
{
	int ret;
	socklen_t optlen =
	(socklen_t)
	{
		sizeof(int)
	};

	(void) getsockopt(sfd, SOL_SOCKET, SO_TYPE, &ret, &optlen);

	return ret;
}


int
net_socket_get_protocol(const int sfd)
{
	int ret;
	socklen_t optlen =
	(socklen_t)
	{
		sizeof(int)
	};

	(void) getsockopt(sfd, SOL_SOCKET, SO_PROTOCOL, &ret, &optlen);

	return ret;
}


void
net_socket_get_peer_address(const int sfd, void* const address)
{
	socklen_t len =
	(socklen_t)
	{
		sizeof(struct sockaddr_storage)
	};

	(void) getpeername(sfd, address, &len);
}


void
net_socket_get_local_address(const int sfd, void* const address)
{
	socklen_t len =
	(socklen_t)
	{
		sizeof(struct sockaddr_storage)
	};

	(void) getsockname(sfd, address, &len);
}


void
net_socket_dont_block(const int sfd)
{
	(void) fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
}


void
net_socket_default_options(const int sfd)
{
	net_socket_reuse_addr(sfd);
	net_socket_reuse_port(sfd);
	net_socket_dont_block(sfd);
}
