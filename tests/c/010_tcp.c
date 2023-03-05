#include <shnet/test.h>
#include <shnet/tcp.h>

#define test_unexpected_event()				\
{											\
	fprintf(								\
		stderr,								\
		"Unexpected event in '%s()': %d\n",	\
		__FUNCTION__, event					\
	);										\
											\
	abort();								\
}


uint8_t old_closing;
uint8_t old_closing_fast;


void
test_set_socket_not_closed(struct tcp_socket* socket)
{
	old_closing = socket->closing;
	socket->closing = 0;

	old_closing_fast = socket->closing_fast;
	socket->closing_fast = 0;
}


void
test_restore_socket_closed(struct tcp_socket* socket)
{
	socket->closing = old_closing;
	socket->closing_fast = old_closing_fast;
}


void
test_buffer_data(struct tcp_socket* socket, const struct data_frame* frame)
{
	assert(!data_storage_add(&socket->queue, frame));
}


struct tcp_socket clients[10] = {0};
struct tcp_socket servers[10] = {0};

int expected_errno = -1;
uint64_t expected_read = UINT64_MAX;

char recv_buf[524288];
uint64_t recv_buf_len = 0;

char send_buf[524288];
uint64_t send_buf_len = 0;

int server_ret_null = 0;
int client_stop_reconnecting = 0;

struct tcp_options options = {0};


struct tcp_socket*
test_client_cb_close_free_only(struct tcp_socket* client, enum tcp_event event)
{
	switch(event)
	{

	case TCP_CLOSE:
	{
		if(errno != expected_errno)
		{
			fprintf(
				stderr,
				"Mismatch in errno, expected %d but got %d\n",
				expected_errno, errno
			);

			abort();
		}

		errno = 0;
		expected_errno = 0;

		tcp_free(client);

		break;
	}

	case TCP_FREE:
	{
		test_wake();

		break;
	}

	default: test_unexpected_event()

	}

	return NULL;
}


struct tcp_socket*
test_server_close(struct tcp_socket* client, enum tcp_event event)
{
	struct tcp_socket* server = tcp_get_server(client, event);

	switch(event)
	{

	case TCP_CLOSE:
	{
		assert(errno == 0);

		tcp_free(server);

		break;
	}

	case TCP_DEINIT:
	{
		break;
	}

	case TCP_FREE:
	{
		test_wake();

		break;
	}

	default: test_unexpected_event()

	}

	return NULL;
}


struct tcp_socket*
test_client_idle(struct tcp_socket* client, enum tcp_event event)
{
	if(event == TCP_CLOSE)
	{
		tcp_free(client);
	}

	return NULL;
}


struct tcp_socket*
test_server_idle(struct tcp_socket* client, enum tcp_event event)
{
	struct tcp_socket* server = tcp_get_server(client, event);

	switch(event)
	{

	case TCP_OPEN:
	{
		test_mutex_wait();

		tcp_free(client);

		break;
	}

	case TCP_CLOSE:
	{
		tcp_free(server);

		break;
	}

	case TCP_DEINIT:
	{
		break;
	}

	case TCP_FREE:
	{
		test_wake();

		break;
	}

	default: test_unexpected_event()

	}

	if(server_ret_null)
	{
		server_ret_null = 0;

		return NULL;
	}

	return client;
}


struct tcp_socket*
test_client_reconnect_idle(struct tcp_socket* client, enum tcp_event event)
{
	switch(event)
	{

	case TCP_OPEN:
	{
		test_mutex_wake();
		test_wake();

		break;
	}

	case TCP_CLOSE:
	{
		tcp_free(client);

		break;
	}

	case TCP_FREE:
	{
		if(client_stop_reconnecting)
		{
			break;
		}

		assert(!tcp_client(client, &options));

		break;
	}

	default: break;

	}

	return NULL;
}


test_use_shnet_malloc()
test_use_shnet_calloc()
test_use_pthread_mutex_init()
test_use_net_socket_get()
test_use_net_socket_bind()
test_use_net_socket_listen()
test_use_net_socket_connect()
test_use_net_socket_accept()
test_use_net_get_address_sync()
test_use_net_get_address_async()
test_use_async_loop()
test_use_async_loop_start()
test_use_async_loop_add()
test_use_recv()
test_use_send()


int
main()
{
	test_begin("tcp test init");

	test_seed_random();

	for(uint64_t i = 0; i < sizeof(send_buf) / sizeof(send_buf[0]); ++i)
	{
		send_buf[i] = (char)(rand() & 0xff);
	}

	errno = 0;

	test_end();


	test_begin("tcp init err 1");

	assert(tcp_client(clients, NULL));
	assert(errno == EINVAL);

	errno = 0;

	test_end();


	test_begin("tcp init err 2");

	assert(tcp_client(clients, &options));
	assert(errno == EINVAL);

	errno = 0;

	test_end();


	test_begin("tcp init err 3");

	options.info = (void*) 0xbad;

	test_error(pthread_mutex_init);

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp init err 4");

	test_error(shnet_calloc);

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp init err 5");

	test_error(async_loop);

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp init err 6");

	test_error(async_loop_start);

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp connect err 1");

	test_error(net_socket_get);

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp connect err 2");

	struct addrinfo client_hints = net_get_addr_struct(
		NET_FAMILY_ANY,
		NET_SOCK_STREAM,
		NET_PROTO_TCP,
		0
	);
	struct addrinfo* client_info =
		net_get_address_sync("127.0.0.1", "0", &client_hints);
	options.info = client_info;

	test_error(net_socket_connect);
	test_error_count(net_socket_connect) = 3;

	assert(tcp_client(clients, &options));
	assert(errno == EPIPE);

	errno = 0;

	test_end();


	test_begin("tcp connect err 3");

	test_error(net_socket_connect);
	test_error_errno(net_socket_connect) = ENOMEM;

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;
	test_error_errno(net_socket_connect) = EPIPE;

	test_end();


	test_begin("tcp connect err 4");

	test_error(net_socket_bind);

	assert(tcp_server(servers, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp connect err 5");

	struct addrinfo server_hints = net_get_addr_struct(
		NET_FAMILY_ANY,
		NET_SOCK_STREAM,
		NET_PROTO_TCP,
		NET_FLAG_WANTS_SERVER
	);
	struct addrinfo* server_info =
		net_get_address_sync("127.0.0.1", "0", &server_hints);
	options.info = server_info;

	test_error(net_socket_listen);

	assert(tcp_server(servers, &options));
	assert(errno == EADDRINUSE);

	errno = 0;

	test_end();


	test_begin("tcp connect err 6");

	options.info = client_info;

	test_error(async_loop_add);
	test_error(net_socket_connect);
	test_error_errno(net_socket_connect) = 0;

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;
	test_error_errno(net_socket_connect) = EPIPE;

	test_end();


	test_begin("tcp async connect err 1");

	options.info = NULL;
	options.hostname = "127.0.0.1";
	options.port = "0";

	test_error(shnet_malloc);
	test_error_delay(shnet_malloc) = 1;

	assert(tcp_client(clients, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp async connect err 2");

	test_error(net_get_address_async);

	assert(tcp_server(servers, &options));
	assert(errno == ENOMEM);

	errno = 0;

	test_end();


	test_begin("tcp async connect err 3");

	test_error(net_get_address_sync);

	clients->on_event = test_client_cb_close_free_only;
	expected_errno = ENOMEM;

	assert(!tcp_client(clients, &options));

	test_wait();

	test_end();


	test_begin("tcp async connect err 4");

	test_error(net_socket_get);
	test_error_errno(net_socket_get) = ENFILE;

	expected_errno = ENFILE;

	assert(!tcp_client(clients, &options));

	test_wait();

	test_error_errno(net_socket_get) = ENOMEM;

	test_end();


	test_begin("tcp server close");

	options.info = server_info;
	servers->on_event = test_server_close;

	assert(!tcp_server(servers, &options));

	tcp_close(servers);

	test_wait();

	test_end();


	test_begin("tcp server close async");

	options.info = NULL;

	test_preempt_off();

	assert(!tcp_server(servers, &options));

	tcp_close(servers);

	test_preempt_on();

	test_wait();

	options.info = server_info;

	test_end();


	test_begin("tcp server conn err 1");

	servers->on_event = test_server_idle;

	assert(!tcp_server(servers, &options));

	const uint16_t server_port = tcp_get_local_port(servers);
	char port_str[6];

	assert(sprintf(port_str, "%hu", server_port) > 0);

	net_free_address(client_info);

	struct async_loop loop = {0};

	assert(!tcp_async_loop(&loop));
	assert(!async_loop_start(&loop));

	clients->loop = &loop;
	clients->on_event = test_client_reconnect_idle;

	client_info = net_get_address_sync("127.0.0.1", port_str, &client_hints);
	options.info = client_info;

	test_error(net_socket_accept);

	assert(!tcp_client(clients, &options));

	test_wait();

	test_end();


	test_begin("tcp server conn err 2");

	server_ret_null = 1;

	tcp_close(clients);

	test_wait();

	test_end();


	test_begin("tcp server conn err 3");

	test_error(shnet_malloc);

	tcp_close(clients);

	test_wait();

	test_end();


	test_begin("tcp server conn err 4");

	test_error(pthread_mutex_init);
	test_error_delay(pthread_mutex_init) = 1;

	tcp_close(clients);

	test_wait();

	test_end();


	test_begin("tcp server conn err 5");

	test_error(async_loop_add);
	test_error_delay(async_loop_add) = 1;

	tcp_close(clients);

	test_wait();

	test_end();









	test_begin("tcp cleanup");

	client_stop_reconnecting = 1;

	tcp_close(clients);
	tcp_close(servers);

	async_loop_shutdown(&loop, ASYNC_FREE | ASYNC_SYNC);

	net_free_address(client_info);
	net_free_address(server_info);

	test_sleep(500);

	test_end();

	return 0;
}
