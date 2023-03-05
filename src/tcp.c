#include <shnet/tcp.h>
#include <shnet/error.h>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <linux/tcp.h>
#include <sys/sendfile.h>


void
tcp_lock(struct tcp_socket* const socket)
{
	(void) pthread_mutex_lock(&socket->lock);
}


void
tcp_unlock(struct tcp_socket* const socket)
{
	(void) pthread_mutex_unlock(&socket->lock);
}


uint16_t
tcp_get_local_port(const struct tcp_socket* const server)
{
	net_address_t addr;

	net_socket_get_local_address(server->fd, &addr);

	return net_address_to_port(&addr);
}


void
tcp_cork_on(const struct tcp_socket* const client)
{
	(void) net_socket_setopt_true(client->fd, NET_PROTO_TCP, TCP_CORK);
}


void
tcp_cork_off(const struct tcp_socket* const client)
{
	(void) net_socket_setopt_false(client->fd, NET_PROTO_TCP, TCP_CORK);
}


void
tcp_nodelay_on(const struct tcp_socket* const client)
{
	(void) net_socket_setopt_true(client->fd, NET_PROTO_TCP, TCP_NODELAY);
}


void
tcp_nodelay_off(const struct tcp_socket* const client)
{
	(void) net_socket_setopt_false(client->fd, NET_PROTO_TCP, TCP_NODELAY);
}


void
tcp_keepalive_on_explicit(const struct tcp_socket* const client,
	const int idle_time, const int reprobe_time, const int retries)
{
	(void) setsockopt(client->fd, NET_PROTO_TCP,
		TCP_KEEPIDLE, &idle_time, sizeof(int));

	(void) setsockopt(client->fd, NET_PROTO_TCP,
		TCP_KEEPINTVL, &reprobe_time, sizeof(int));

	(void) setsockopt(client->fd, NET_PROTO_TCP,
		TCP_KEEPCNT, &retries, sizeof(int));

	(void) setsockopt(client->fd, NET_PROTO_TCP, TCP_USER_TIMEOUT,
		(int[]){ (idle_time + reprobe_time * retries) * 1000 }, sizeof(int));

	(void) net_socket_setopt_true(client->fd, SOL_SOCKET, SO_KEEPALIVE);
}


void
tcp_keepalive_on(const struct tcp_socket* const client)
{
	tcp_keepalive_on_explicit(client, 1, 1, 10);
}


void
tcp_keepalive_off(const struct tcp_socket* const client)
{
	(void) setsockopt(client->fd, NET_PROTO_TCP,
		TCP_USER_TIMEOUT, (int[]){ 0 }, sizeof(int));

	(void) net_socket_setopt_false(client->fd, SOL_SOCKET, SO_KEEPALIVE);
}


void
tcp_free_common(struct tcp_socket* const socket)
{
	if(socket->on_event && socket->opened)
	{ // TODO TEST CASE FOR THIS
		(void) socket->on_event(socket, TCP_DEINIT);
	}

	if(socket->fd != -1)
	{
		(void) close(socket->fd);

		socket->fd = -1;
	}

	(void) pthread_mutex_destroy(&socket->lock);
	data_storage_free(&socket->queue);

	if(socket->alloc_loop)
	{
		async_loop_shutdown(socket->loop, ASYNC_FREE | ASYNC_PTR_FREE);

		socket->loop = NULL;
		socket->alloc_loop = 0;
	}

	socket->opened = 0;
	socket->confirmed_free = 0;
	socket->closing = 0;
	socket->close_guard = 0;
	socket->closing_fast = 0;

	uint8_t free_ptr = socket->free_ptr;
	socket->free_ptr = 0;

	if(socket->on_event)
	{
		(void) socket->on_event(socket, TCP_FREE);
	}

	if(free_ptr)
	{
		free(socket);
	}
}


void
tcp_free(struct tcp_socket* const socket)
{
	tcp_lock(socket);

	if(socket->confirmed_free)
	{
		tcp_unlock(socket);

		tcp_free_common(socket);
	}
	else
	{
		socket->confirmed_free = 1;

		tcp_unlock(socket);
	}
}


static void
tcp_free_internal(struct tcp_socket* const socket)
{
	if(socket->on_event)
	{
		(void) socket->on_event(socket, TCP_CLOSE);
	}

	tcp_lock(socket);

	if(socket->confirmed_free)
	{
		tcp_unlock(socket);

		tcp_free_common(socket);
	}
	else
	{
		socket->confirmed_free = 1;

		(void) async_loop_remove(socket->loop, &socket->fd);

		tcp_unlock(socket);
	}
}


void
tcp_close(struct tcp_socket* const socket)
{
	tcp_lock(socket);

	socket->closing = 1;

	if(
		socket->opened &&
		data_storage_is_empty(&socket->queue) &&
		!socket->close_guard
	)
	{
		socket->close_guard = 1;

		int how;

		if(socket->type == TCP_CLIENT)
		{
			how = SHUT_WR;
		}
		else
		{
			how = SHUT_RDWR;
		}

		(void) shutdown(socket->fd, how);
	}

	tcp_unlock(socket);
}


void
tcp_terminate(struct tcp_socket* const client)
{
	if(client->type == TCP_SERVER)
	{
		tcp_close(client);

		return;
	}

	tcp_lock(client);

	client->closing_fast = 1;

	data_storage_free(&client->queue);

	if(client->opened && !client->close_guard)
	{
		client->close_guard = 1;

		(void) shutdown(client->fd, SHUT_RDWR);
	}

	tcp_unlock(client);
}


static ssize_t
tcp_send_common(const struct tcp_socket* const client,
	const struct data_frame* const frame)
{
	ssize_t bytes;

	errno = 0;

	if(frame->file)
	{
		off_t off = frame->offset;

		safe_execute(
			bytes = sendfile(
				client->fd,
				frame->fd,
				&off,
				frame->len - frame->offset
			),
			bytes == -1,
			errno);
	}
	else
	{
		safe_execute(
			bytes = send(
				client->fd,
				frame->data + frame->offset,
				frame->len - frame->offset,
				MSG_NOSIGNAL
			),
			bytes == -1,
			errno
		);
	}

	return bytes;
}


int
tcp_send_buffered(struct tcp_socket* const client)
{
	while(!data_storage_is_empty(&client->queue))
	{
		const ssize_t bytes = tcp_send_common(client, client->queue.frames);

		if(bytes == -1)
		{
			switch(errno)
			{

			case EINTR: continue;

			case EPIPE:
			case ECONNRESET:
			{
				client->closing_fast = 1;

				data_storage_free(&client->queue);

				errno = EPIPE;

				return -2;
			}

			case EAGAIN:
			{
				data_storage_finish(&client->queue);

				return -1;
			}

			default: return -1;

			}
		}

		data_storage_drain(&client->queue, bytes);
	}

	if(!client->close_guard && client->closing)
	{
		(void) shutdown(client->fd, SHUT_WR);

		errno = 0;

		return -2;
	}

	return 0;
}


int
tcp_send(struct tcp_socket* const client, const struct data_frame* const frame)
{
	tcp_lock(client);

	if(client->closing || client->closing_fast)
	{
		errno = EPIPE;

		goto goto_err;
	}

	const int err = tcp_send_buffered(client);

	if(err == -2)
	{
		errno = EPIPE;

		goto goto_err;
	}

	if(err == -1 || !client->opened)
	{
		errno = 0;

		if(data_storage_add(&client->queue, frame))
		{
			goto goto_err;
		}

		tcp_unlock(client);

		return 0;
	}

	struct data_frame data = *frame;

	while(1)
	{
		const ssize_t bytes = tcp_send_common(client, &data);

		if(bytes == -1)
		{
			switch(errno)
			{

			case EINTR: continue;

			case EPIPE:
			case ECONNRESET:
			{
				client->closing_fast = 1;

				goto goto_err;
			}

			default:
			{
				const int ret = data_storage_add(&client->queue, &data);

				tcp_unlock(client);

				return ret;
			}

			}

			break;
		}

		data.offset += bytes;

		if(data.offset == data.len)
		{
			tcp_unlock(client);

			data_frame_free(frame);

			errno = 0;

			return 0;
		}
	}


	goto_err:

	tcp_unlock(client);

	data_frame_free_err(frame);

	return -1;
}


uint64_t
tcp_read(struct tcp_socket* const client, void* out, uint64_t out_len)
{
	if(out_len == 0)
	{
		errno = 0;

		return 0;
	}

	const uint64_t all = out_len;

	while(1)
	{
		ssize_t bytes;

		errno = 0;

		safe_execute(
			bytes = recv(client->fd, out, out_len, 0),
			bytes == -1,
			errno
		);

		if(bytes == -1)
		{
			if(errno == EINTR)
			{
				continue;
			}

			break;
		}
		else if(bytes == 0)
		{
			errno = EPIPE;

			break;
		}

		out_len -= bytes;

		if(out_len == 0)
		{
			errno = 0;

			break;
		}

		out = (char*) out + bytes;
	}

	return all - out_len;
}


uint64_t
tcp_socket_send_buf_len(const struct tcp_socket* const client)
{
	struct tcp_info info;
	uint64_t sum = client->queue.bytes;

	int err = getsockopt(client->fd, IPPROTO_TCP,
		TCP_INFO, &info, &(socklen_t){ sizeof(info) });

	if(!err)
	{
		sum += info.tcpi_notsent_bytes;
	}

	return sum;
}


uint64_t
tcp_socket_recv_buf_len(const struct tcp_socket* const socket)
{
	struct tcp_info info;

	int err = getsockopt(socket->fd, IPPROTO_TCP,
		TCP_INFO, &info, &(socklen_t){ sizeof(info) });

	if(err)
	{
		return 0;
	}

	return info.tcpi_bytes_received;
}


static int
tcp_connect(struct tcp_socket* const socket,
	const struct tcp_options* const options)
{
	uint8_t ers = 0;
	const struct addrinfo* info = options->info;

	while(1)
	{
		tcp_lock(socket);

		if(socket->fd != -1)
		{
			(void) close(socket->fd);

			socket->fd = -1;
		}

		if(
			info == NULL ||
			socket->closing_fast ||
			(socket->closing && data_storage_is_empty(&socket->queue))
		)
		{
			goto goto_err;
		}

		socket->fd = net_socket_get(info);

		if(socket->fd == -1)
		{
			goto goto_err;
		}

		net_socket_default_options(socket->fd);

		tcp_unlock(socket);

		errno = 0;

		if(socket->type == TCP_CLIENT)
		{
			(void) net_socket_connect(socket->fd, info);
		}
		else if(!net_socket_bind(socket->fd, info))
		{
			const int backlog = options->backlog == 0 ? 32 : options->backlog;

			net_socket_listen(socket->fd, backlog);
		}

		switch(errno)
		{

		case 0:
		case EINTR:
		case EINPROGRESS:
		{
			if(async_loop_add(socket->loop, &socket->fd,
				EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT))
			{
				info = NULL;

				continue;
			}

			if(socket->type == TCP_SERVER)
			{
				socket->opened = 1;

				if(socket->closing)
				{
					tcp_close(socket);
				}
			}

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


	goto_err:

	tcp_unlock(socket);

	return -1;
}


struct tcp_async_data
{
	struct net_async_address addr;

	struct tcp_options options;
};


static void
tcp_connect_async(struct net_async_address* addr, struct addrinfo* info)
{
	struct tcp_socket* const socket = addr->data;

	struct tcp_async_data* const data = (void*) addr;

	if(info == NULL)
	{
		tcp_free_internal(socket);
	}
	else
	{
		data->options.info = info;

		if(tcp_connect(socket, &data->options))
		{
			tcp_free_internal(socket);
		}

		net_free_address(info);
	}

	free(addr);
}


static int
tcp_init(struct tcp_socket* const socket,
	const struct tcp_options* const options)
{
	if(
		options == NULL ||
		(
			options->info == NULL &&
			options->hostname == NULL &&
			options->port == NULL
		)
	)
	{
		errno = EINVAL;

		return -1;
	}

	int err;

	safe_execute(err = pthread_mutex_init(&socket->lock, NULL), err, err);

	if(err)
	{
		errno = err;

		return -1;
	}

	if(socket->loop == NULL)
	{
		socket->loop = shnet_calloc(1, sizeof(*socket->loop));

		if(socket->loop == NULL)
		{
			goto goto_mutex;
		}

		if(tcp_async_loop(socket->loop))
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

		goto goto_mutex;
	}

	goto_out:

	socket->fd = -1;

	if(options->info)
	{
		if(tcp_connect(socket, options))
		{
			goto goto_loop;
		}
	}
	else
	{
		struct addrinfo* info;

		const size_t hostname_len =
			options->hostname == NULL ? 0 : (strlen(options->hostname) + 1);

		const size_t port_len =
			options->port == NULL ? 0 : (strlen(options->port) + 1);

		struct tcp_async_data* const data = shnet_malloc(
			sizeof(*data) + sizeof(*info) + hostname_len + port_len);

		if(data == NULL)
		{
			goto goto_loop;
		}

		void* past_data = data + 1;

		info = past_data;
		info->ai_family = options->family ? options->family : NET_FAMILY_IPV4;
		info->ai_socktype = NET_SOCK_STREAM;
		info->ai_protocol = NET_PROTO_TCP;
		info->ai_flags = options->flags;

		if(socket->type == TCP_SERVER)
		{
			info->ai_flags |= NET_FLAG_WANTS_SERVER;
		}

		data->addr.hints = past_data;
		past_data = (char*) past_data + sizeof(*data->addr.hints);

		data->addr.data = socket;
		data->addr.callback = tcp_connect_async;

		if(hostname_len)
		{
			(void) memcpy(past_data, options->hostname, hostname_len);

			data->addr.hostname = past_data;
			past_data = (char*) past_data + hostname_len;
		}

		if(port_len)
		{
			(void) memcpy(past_data, options->port, port_len);

			data->addr.port = past_data;
			past_data = (char*) past_data + port_len;
		}

		data->options = *options;

		if(net_get_address_async(&data->addr))
		{
			free(data);

			goto goto_loop;
		}
	}

	return 0;


	goto_loop:

	if(socket->alloc_loop)
	{
		async_loop_shutdown(socket->loop, ASYNC_FREE | ASYNC_PTR_FREE);

		socket->loop = NULL;
		socket->alloc_loop = 0;
	}

	goto_mutex:

	(void) pthread_mutex_destroy(&socket->lock);

	return -1;
}


static struct tcp_socket*
tcp_client_onevent(struct tcp_socket* const client, uint32_t events)
{
	int code = 0;

	if(events & EPOLLERR)
	{
		(void) getsockopt(client->fd, SOL_SOCKET,
			SO_ERROR, &code, &(socklen_t){ sizeof(int) });
	}
	else
	{
		if(!client->opened && (events & EPOLLOUT))
		{
			tcp_lock(client);

			client->opened = 1;

			tcp_unlock(client);

			if(client->on_event)
			{
				(void) client->on_event(client, TCP_OPEN);
			}

			tcp_lock(client);

			if(!client->close_guard)
			{
				if(client->closing_fast)
				{
					data_storage_free(&client->queue);

					client->close_guard = 1;

					tcp_unlock(client);

					(void) shutdown(client->fd, SHUT_RDWR);

					events |= EPOLLHUP;
				}
				else if(
					client->closing &&
					data_storage_is_empty(&client->queue)
				)
				{
					client->close_guard = 1;

					tcp_unlock(client);

					(void) shutdown(client->fd, SHUT_WR);
				}
				else
				{
					tcp_unlock(client);
				}
			}
			else
			{
				tcp_unlock(client);
			}

			(void) getsockopt(client->fd, SOL_SOCKET,
				SO_ERROR, &code, &(socklen_t){ sizeof(int) });
		}

		if((events & EPOLLIN) && client->on_event)
		{
			(void) client->on_event(client, TCP_DATA);
		}
	}

	if((events & EPOLLHUP) || code)
	{
		errno = code;

		tcp_free_internal(client);

		return NULL;
	}

	if(events & EPOLLOUT)
	{
		if(client->on_event)
		{
			(void) client->on_event(client, TCP_CAN_SEND);
		}
		else
		{
			tcp_lock(client);

			if(!client->closing_fast)
			{
				(void) tcp_send_buffered(client);
			}

			tcp_unlock(client);
		}
	}

	if(events & EPOLLRDHUP)
	{
		if(client->on_event)
		{
			(void) client->on_event(client, TCP_READCLOSE);
		}
		else
		{
			tcp_close(client);
		}
	}

	return NULL;
}


struct tcp_server_data
{
	struct tcp_socket client;

	struct tcp_socket* server;
};


struct tcp_socket*
tcp_get_server(struct tcp_socket* const client, const enum tcp_event event)
{
	switch(event)
	{

	case TCP_OPEN:
	{
		const struct tcp_server_data* const data = (void*) client;

		return data->server;
	}

	case TCP_CLOSE:
	case TCP_DEINIT:
	case TCP_FREE:
	{
		return client;
	}

	default:
	{
		errno = EINVAL;

		return NULL;
	}

	}
}


static struct tcp_socket*
tcp_server_onevent(struct tcp_socket* const server, uint32_t events)
{
	struct tcp_server_data data;
	data.server = server;

	if(events & EPOLLHUP)
	{
		tcp_free_internal(server);

		return NULL;
	}

	while(1)
	{
		int sfd = net_socket_accept(server->fd);

		if(sfd == -1)
		{
			switch(errno)
			{

			case EINTR:
			case EPIPE:
			case EPERM:
			case EPROTO:
			case ECONNRESET:
			case ECONNABORTED: continue;

			default: return NULL;

			}
		}

		data.client = (struct tcp_socket){0};
		data.client.fd = sfd;

		net_socket_default_options(sfd);

		struct tcp_socket* socket = server->on_event(&data.client, TCP_OPEN);

		if(socket == NULL)
		{
			goto goto_sock;
		}

		if(socket == &data.client)
		{
			void* const ptr = shnet_malloc(sizeof(*socket));

			if(ptr == NULL)
			{
				goto goto_sock;
			}

			socket = ptr;
			data.client.free_ptr = 1;
		}

		*socket = data.client;

		if(socket->loop == NULL)
		{
			socket->loop = server->loop;
		}

		int err;

		safe_execute(err = pthread_mutex_init(&socket->lock, NULL), err, err);

		if(err)
		{
			errno = err;

			goto goto_open;
		}

		if(async_loop_add(socket->loop, &socket->fd,
			EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT))
		{
			goto goto_mutex;
		}

		continue;


		goto_mutex:

		(void) pthread_mutex_destroy(&socket->lock);

		goto_open:

		if(socket->free_ptr)
		{
			free(socket);
		}

		goto_sock:

		(void) close(sfd);
	}
}


int
tcp_client(struct tcp_socket* const client,
	const struct tcp_options* const options)
{
	client->type = TCP_CLIENT;

	return tcp_init(client, options);
}


int
tcp_server(struct tcp_socket* const server,
	const struct tcp_options* const options)
{
	server->type = TCP_SERVER;

	return tcp_init(server, options);
}


void
tcp_onevent(struct async_loop* loop, int* event_fd, uint32_t events)
{
	(void) loop;

	struct tcp_socket* const socket = (void*) event_fd;

	if(socket->type == TCP_CLIENT)
	{
		tcp_client_onevent(socket, events);
	}
	else
	{
		tcp_server_onevent(socket, events);
	}
}


int
tcp_async_loop(struct async_loop* const loop)
{
	loop->on_event = tcp_onevent;

	return async_loop(loop);
}
