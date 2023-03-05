#include <shnet/udp.h>
#include <shnet/error.h>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


static int
udp_bind(struct udp_socket* const socket, const struct addrinfo* info)
{
	uint8_t ers = 0;

	while(1)
	{
		if(socket->fd != -1)
		{
			(void) close(socket->fd);

			socket->fd = -1;
		}

		if(info == NULL)
		{
			return -1;
		}

		socket->fd = net_socket_get(info);

		if(socket->fd == -1)
		{
			return -1;
		}

		net_socket_default_options(socket->fd);

		errno = 0;

		if(socket->type == UDP_CLIENT)
		{
			(void) net_socket_connect(socket->fd, info);
		}
		else
		{
			(void) net_socket_bind(socket->fd, info);
		}

		switch(errno)
		{

		case 0:
		case EINTR:
		case EINPROGRESS:
		{
			if(socket->on_event && async_loop_add(socket->loop,
				&socket->fd, EPOLLET | EPOLLIN))
			{
				info = NULL;

				continue;
			}

			socket->udp_lite = info->ai_protocol == NET_PROTO_UDP_LITE;

			return 0;
		}

		case EPIPE:
		case ECONNRESET:
		{
			if(++ers != 3)
			{
				continue;
			}

			ers = 0;

			shnet_fallthrough();
		}

		default:
		{
			info = info->ai_next;

			continue;
		}

		}
	}
}

static int
udp_init(struct udp_socket* const socket, const struct addrinfo* const info)
{
	if(info == NULL)
	{
		errno = EINVAL;

		return -1;
	}

	if(socket->on_event && socket->loop == NULL)
	{
		socket->loop = shnet_calloc(1, sizeof(*socket->loop));

		if(socket->loop == NULL)
		{
			return -1;
		}

		if(udp_async_loop(socket->loop))
		{
			goto goto_loop_1;
		}

		if(async_loop_start(socket->loop))
		{
			goto goto_loop_2;
		}

		socket->alloc_loop = 1;

		goto goto_out;


		goto_loop_2:

		async_loop_free(socket->loop);

		goto_loop_1:

		free(socket->loop);

		socket->loop = NULL;

		return -1;
	}

	goto_out:

	socket->fd = -1;

	if(udp_bind(socket, info) && socket->alloc_loop)
	{
		async_loop_shutdown(socket->loop, ASYNC_FREE | ASYNC_PTR_FREE);

		socket->loop = NULL;
		socket->alloc_loop = 0;
	}

	return 0;
}


int
udp_client(struct udp_socket* const client, const struct addrinfo* const info)
{
	client->type = UDP_CLIENT;

	return udp_init(client, info);
}


int
udp_server(struct udp_socket* const server, const struct addrinfo* const info)
{
	server->type = UDP_SERVER;

	return udp_init(server, info);
}


void
udp_send(const struct udp_socket* socket, const void* data, const size_t len)
{
	size_t offset = 0;
	ssize_t bytes;

	while(1)
	{
		errno = 0;

		safe_execute(
			bytes = send(
				socket->fd,
				data + offset,
				len - offset,
				MSG_NOSIGNAL
			),
			bytes == -1,
			errno
		);

		if(bytes <= 0)
		{
			if(errno == EINTR)
			{
				continue;
			}

			break;
		}

		offset += bytes;
	}
}


static __thread net_address_t udp_address;


ssize_t
udp_read(const struct udp_socket* const socket, void* out,
	const size_t out_len, struct addrinfo* const out_info)
{
	if(out_len == 0)
	{
		errno = 0;

		return 0;
	}

	socklen_t len = sizeof(udp_address);
	ssize_t bytes;

	while(1)
	{
		errno = 0;

		safe_execute(
			bytes = recvfrom(
				socket->fd,
				out,
				out_len,
				0,
				(void*) &udp_address,
				&len
			),
			bytes == -1,
			errno
		);

		if(errno == EINTR)
		{
			continue;
		}

		break;
	}

	out_info->ai_family = net_address_to_family(&udp_address);
	out_info->ai_socktype = NET_SOCK_DATAGRAM;
	out_info->ai_protocol =
		socket->udp_lite ? NET_PROTO_UDP_LITE : NET_PROTO_UDP;
	out_info->ai_flags = 0;
	out_info->ai_addr = (void*) &udp_address;
	out_info->ai_addrlen = len;
	out_info->ai_canonname = NULL;
	out_info->ai_next = NULL;

	return bytes;
}


void
udp_free(struct udp_socket* const socket)
{
	if(socket->fd != -1)
	{
		(void) close(socket->fd);

		socket->fd = -1;
	}

	if(socket->alloc_loop)
	{
		async_loop_shutdown(socket->loop, ASYNC_FREE | ASYNC_PTR_FREE);

		socket->loop = NULL;
		socket->alloc_loop = 0;
	}
}


void
udp_onevent(struct async_loop* loop, int* event_fd, uint32_t events)
{
	(void) loop;

	struct udp_socket* const socket = (void*) event_fd;

	if((events & EPOLLIN) && socket->on_event)
	{
		socket->on_event(socket);
	}
}


int
udp_async_loop(struct async_loop* const loop)
{
	loop->on_event = udp_onevent;

	return async_loop(loop);
}
