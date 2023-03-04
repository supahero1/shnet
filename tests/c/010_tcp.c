#include <shnet/test.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <shnet/tcp.h>

test_register(void*, shnet_malloc, (const size_t a), (a))
test_register(void*, shnet_calloc, (const size_t a, const size_t b), (a, b))
test_register(int, pthread_mutex_init, (pthread_mutex_t* restrict a, const pthread_mutexattr_t* restrict b), (a, b))
test_register(int, net_socket_get, (const struct addrinfo* const a), (a))
test_register(int, net_socket_bind, (const int a, const struct addrinfo* const b), (a, b))
test_register(int, net_socket_connect, (const int a, const struct addrinfo* const b), (a, b))
test_register(struct addrinfo*, net_get_address, (const char* const a, const char* const b, const struct addrinfo* const c), (a, b, c))
test_register(int, net_get_address_async, (struct net_async_address* const a), (a))
test_register(int, async_loop, (struct async_loop* const a), (a))
test_register(int, async_loop_start, (struct async_loop* const a), (a))
test_register(int, async_loop_add, (const struct async_loop* const a, struct async_event* const b, const uint32_t c), (a, b, c))
test_register(int, listen, (int a, int b), (a, b))
test_register(int, accept, (int a, struct sockaddr* restrict b, socklen_t* restrict c), (a, b, c))
test_register(ssize_t, recv, (int a, void* b, size_t c, int d), (a, b, c, d))
test_register(ssize_t, send, (int a, const void* b, size_t c, int d), (a, b, c, d))

uint8_t old_closing;
uint8_t old_closing_fast;

void test_set_socket_not_closed(struct tcp_socket* const socket) {
	old_closing = socket->closing;
	socket->closing = 0;
	old_closing_fast = socket->closing_fast;
	socket->closing_fast = 0;
}

void test_restore_socket_closed(struct tcp_socket* const socket) {
	socket->closing = old_closing;
	socket->closing_fast = old_closing_fast;
}

void test_buffer_data(struct tcp_socket* const socket, const struct data_frame* const frame) {
	assert(!data_storage_add(&socket->queue, frame));
}

struct tcp_socket sockets[10] = {0};
struct tcp_server servers[10] = {0};

int expected_errno = -1;
uint64_t expected_read = UINT64_MAX;

char recv_buf[524288];
uint64_t recv_buf_len = 0;

char send_buf[524288];
uint64_t send_buf_len = 0;

void free_only_strict(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open: assert(0);
		case tcp_close: {
			if(expected_errno != -1) {
				assert(errno == expected_errno);
				expected_errno = -1;
			}
			break;
		}
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
}

void free_only(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
}

void close_only(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_close: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
}

void idle_only(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_data: {
			if(expected_read == UINT64_MAX) break;
			assert(tcp_read(sock, NULL, 0) == 0);
			test_error(recv);
			while(1) {
				const uint64_t available = sizeof(recv_buf) / sizeof(recv_buf[0]) - recv_buf_len;
				uint64_t read = tcp_read(sock, recv_buf + recv_buf_len, available);
				expected_read -= read;
				recv_buf_len += read;
				if(expected_read == 0 || read < available) {
					break;
				}
			}
			if(expected_read == 0) {
				test_wake();
				expected_read = UINT64_MAX;
			}
			break;
		}
		case tcp_close: {
			char s = 's';
			assert(!data_storage_add(&sock->queue, &((struct data_frame) {
				.data = &s,
				.len = 1,
				.dont_free = 1,
				.read_only = 1,
				.free_onerr = 0
			})));
			assert(tcp_send(sock, &((struct data_frame) {
				.data = &s,
				.len = 1,
				.dont_free = 1,
				.read_only = 1,
				.free_onerr = 0
			})));
			tcp_socket_free(sock);
			break;
		}
		default: break;
	}
}

void open_free_only(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_close: {
			tcp_lock(sock);
			assert(sock->opened);
			tcp_unlock(sock);
			if(expected_errno != 0) {
				assert(errno == expected_errno);
				expected_errno = 0;
			}
			break;
		}
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
}

void open_free(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open:
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
}

void open_close_free(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_close: {
			tcp_lock(sock);
			assert(sock->opened);
			tcp_unlock(sock);
			if(expected_errno != 0) {
				assert(errno == expected_errno);
				expected_errno = 0;
			}
			test_mutex_wake();
			break;
		}
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
}

void close_at_open(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open: {
			tcp_socket_close(sock);
			test_wake();
			break;
		}
		case tcp_close: {
			tcp_lock(sock);
			assert(sock->opened);
			tcp_unlock(sock);
			if(expected_errno != 0) {
				assert(errno == expected_errno);
				expected_errno = 0;
			}
			break;
		}
		case tcp_free: {
			test_wake();
			break;
		}
		default: break;
	}
}

void force_close_at_open(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open: {
			tcp_socket_force_close(sock);
			test_wake();
			break;
		}
		case tcp_close: {
			tcp_lock(sock);
			assert(sock->opened);
			tcp_unlock(sock);
			if(expected_errno != 0) {
				assert(errno == expected_errno);
				expected_errno = 0;
			}
			break;
		}
		case tcp_free: {
			test_wake();
			break;
		}
		default: break;
	}
}

void send_a_file(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open: {
			const int fd = memfd_create("shnet_test2", 0);
			assert(fd >= 0);
			assert(!ftruncate(fd, 1));
			assert(write(fd, (uint8_t[]){ 's' }, 1) == 1);
			assert(!tcp_send(sock, &((struct data_frame) {
				.fd = fd,
				.file = 1,
				.offset = 0,
				.len = 1,
				.dont_free = 0,
				.read_only = 1,
				.free_onerr = 1
			})));
			break;
		}
		case tcp_close: {
			tcp_socket_free(sock);
			break;
		}
		case tcp_free: {
			test_wake();
			break;
		}
		default: break;
	}
}

void send_a_msg(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open: {
			send_buf[0] = 's';
			assert(!tcp_send(sock, &((struct data_frame) {
				.data = send_buf,
				.len = 1,
				.dont_free = 1,
				.read_only = 1,
				.free_onerr = 0
			})));
			break;
		}
		case tcp_close: {
			tcp_socket_free(sock);
			break;
		}
		case tcp_free: {
			test_wake();
			break;
		}
		default: break;
	}
}

void read_something(struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_data: {
			assert(tcp_read(sock, recv_buf, 1) == 1);
			assert(recv_buf[0] == 's');
			tcp_socket_close(sock);
			tcp_socket_free(sock);
			sock->on_event = free_only;
			break;
		}
		default: break;
	}
}

struct tcp_socket* reject_only(struct tcp_server* serv, struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_close: {
			tcp_server_free(serv);
			break;
		}
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
	return NULL;
}

int server_crash_stage = 0;
int server_onevt = 0;

struct tcp_socket* return_self_only(struct tcp_server* serv, struct tcp_socket* sock, enum tcp_event event) {
	switch(event) {
		case tcp_open: {
			switch(server_onevt) {
				case 0: {
					sock->on_event = idle_only;
					break;
				}
				case 1: {
					sock->on_event = send_a_file;
					break;
				}
				case 2: {
					sock->on_event = send_a_msg;
					break;
				}
				default: break;
			}
			switch(server_crash_stage) {
				case 1: {
					test_error(shnet_malloc);
					++server_crash_stage;
					break;
				}
				case 2: {
					test_error(pthread_mutex_init);
					++server_crash_stage;
					break;
				}
				case 3: {
					test_error(async_loop_add);
					++server_crash_stage;
					break;
				}
				default: {
					server_crash_stage = 0;
					break;
				}
			}
			break;
		}
		case tcp_close: {
			tcp_server_free(serv);
			break;
		}
		case tcp_free: {
			test_mutex_wake();
			break;
		}
		default: break;
	}
	return sock;
}

int main() {
	test_seed_random();
	for(unsigned i = 0; i < sizeof(send_buf) / sizeof(send_buf[0]); ++i) {
		send_buf[i] = (char)(rand() & 0xff);
	}

	test_begin("tcp check");
	test_error_check(void*, shnet_malloc, (0xbad));
	test_error_check(void*, shnet_calloc, (0xbad, 0xbad));
	test_error_check(int, pthread_mutex_init, ((void*) 0x1bad, (void*) 0x2bad));
	test_error_check(int, net_socket_get, ((void*) 0xbad));
	test_error_check(int, net_socket_bind, (0xbad, (void*) 0xbad));
	test_error_check(int, net_socket_connect, (0xbad, (void*) 0xbad));
	test_error_check(struct addrinfo*, net_get_address, ((void*) 0xbad, (void*) 0xbad, (void*) 0xbad));
	test_error_check(int, net_get_address_async, ((void*) 0xbad));
	test_error_check(int, async_loop, ((void*) 0xbad));
	test_error_check(int, async_loop_start, ((void*) 0xbad));
	test_error_check(int, async_loop_add, ((void*) 0xbad, (void*) 0xbad, 0xbad));
	test_error_check(int, listen, (0xbad, 0xbad));
	test_error_check(int, accept, (0xbad, (void*) 0x1bad, (void*) 0x2bad));
	test_error_check(ssize_t, recv, (0xbad, (void*) 0xbad, 0xbad, 0xbad));
	test_error_check(ssize_t, send, (0xbad, (void*) 0xbad, 0xbad, 0xbad));

	test_error_set_retval(shnet_malloc, NULL);
	test_error_set_retval(shnet_calloc, NULL);
	test_error_set_retval(pthread_mutex_init, ENOMEM);
	test_error_set_retval(net_get_address, NULL);
	test_error_set_errno(accept, EPIPE);
	test_error_set_errno(recv, EINTR);
	test_error_set_errno(send, EINTR);
	test_end();

	test_begin("tcp socket err 1");
	errno = 0;
	assert(tcp_socket(sockets, NULL));
	assert(errno == EINVAL);
	test_end();

	test_begin("tcp socket err 2");
	struct tcp_socket_options options = {0};
	errno = 0;
	assert(tcp_socket(sockets, &options));
	assert(errno == EINVAL);
	test_end();

	test_begin("tcp socket err 3");
	options.hostname = "127.0.0.1";
	test_error(pthread_mutex_init);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 4");
	test_error(shnet_calloc);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 5");
	test_error(async_loop);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 6");
	test_error(async_loop_start);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 7");
	test_error_set(shnet_malloc, 2);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 8");
	test_error(net_get_address_async);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 9");
	options.port = "80";
	test_error(net_get_address_async);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp socket err 10");
	options.hostname = NULL;
	test_error(net_get_address_async);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp dns");
	const struct addrinfo hints = net_get_addr_struct(net_family_ipv4, net_sock_stream, net_proto_tcp, 0);
	struct addrinfo* info = net_get_address("127.0.0.1", NULL, &hints);
	assert(info);
	net_free_address(info->ai_next);
	info->ai_next = NULL;
	options.info = info;
	test_end();

	test_begin("tcp connect err 1");
	test_error(net_socket_get);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp connect err 2");
	test_error(net_socket_connect);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp connect err 3");
	test_error(net_socket_connect);
	test_error_set_errno(net_socket_connect, EPIPE);
	test_error_set(async_loop_add, 2);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp connect err 4");
	test_error_set(async_loop_add, 2);
	assert(tcp_socket(sockets, &options));
	test_end();

	test_begin("tcp dns err 1");
	options.info = NULL;
	test_error(net_get_address);
	expected_errno = ENODEV;
	int save = test_error_get_errno(net_get_address);
	test_error_set_errno(net_get_address, expected_errno);
	sockets->on_event = free_only_strict;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_error_set_errno(net_get_address, save);
	test_end();

	test_begin("tcp dns err 2");
	assert(sockets->on_event == free_only_strict);
	test_error(net_socket_get);
	expected_errno = ENODEV;
	save = test_error_get_errno(net_socket_get);
	test_error_set_errno(net_socket_get, expected_errno);
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_error_set_errno(net_socket_get, save);
	test_end();

	test_begin("tcp open err");
	expected_errno = ECONNREFUSED;
	options.info = info;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_close(sockets);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_end();

	test_begin("tcp server err 1");
	assert(tcp_server(servers, NULL));
	test_end();

	test_begin("tcp server err 2");
	struct tcp_server_options serv_options = {0};
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server err 3");
	servers->on_event = reject_only;
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 1");
	serv_options.hostname = "127.0.0.1";
	test_error(net_get_address);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 2");
	test_error(shnet_calloc);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 3");
	test_error(async_loop);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 4");
	test_error(async_loop_start);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 5");
	test_error(net_socket_get);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 6");
	test_error(net_socket_bind);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 7");
	test_error(listen);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path err 8");
	test_error_set(async_loop_add, 2);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server dns path");
	assert(!tcp_server(servers, &serv_options));
	tcp_server_close(servers);
	test_mutex_wait();
	test_end();

	test_begin("tcp server non dns path err 1");
	serv_options.info = info;
	test_error(shnet_calloc);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path err 2");
	test_error(async_loop);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path err 3");
	test_error(async_loop_start);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path err 4");
	test_error(net_socket_get);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path err 5");
	test_error(net_socket_bind);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path err 6");
	test_error(listen);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path err 7");
	test_error_set(async_loop_add, 2);
	assert(tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp server non dns path");
	assert(!tcp_server(servers, &serv_options));
	test_end();

	test_begin("tcp dns setup");
	char port[7] = {0};
	assert(sprintf(port, "%hu", tcp_server_get_port(servers)) > 0);
	net_free_address(info);
	info = net_get_address("127.0.0.1", port, &hints);
	test_end();

	test_begin("tcp server open err 1");
	sockets->on_event = close_at_open;
	expected_errno = 0;
	options.info = info;
	test_error(accept);
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	test_wait();
	test_wait();
	test_end();

	test_begin("tcp server open err 2");
	assert(sockets->closing == 0);
	assert(sockets->closing_fast == 0);
	assert(sockets->close_guard == 0);
	assert(sockets->confirmed_free == 0);
	sockets->on_event = free_only;
	servers->on_event = return_self_only;
	server_crash_stage = 1;
	async_loop_stop(servers->loop);
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	assert(!async_loop_start(servers->loop));
	test_mutex_wait();
	test_end();

	test_begin("tcp server open err 3");
	assert(sockets->closing == 0);
	assert(sockets->closing_fast == 0);
	assert(sockets->close_guard == 0);
	assert(sockets->confirmed_free == 0);
	async_loop_stop(servers->loop);
	assert(!tcp_socket(sockets, &options));
	assert(!async_loop_start(servers->loop));
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_end();

	test_begin("tcp server open err 4");
	assert(sockets->closing == 0);
	assert(sockets->closing_fast == 0);
	assert(sockets->close_guard == 0);
	assert(sockets->confirmed_free == 0);
	async_loop_stop(servers->loop);
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	assert(!async_loop_start(servers->loop));
	test_mutex_wait();
	test_end();

	test_begin("tcp server open 1");
	assert(sockets->closing == 0);
	assert(sockets->closing_fast == 0);
	assert(sockets->close_guard == 0);
	assert(sockets->confirmed_free == 0);
	sockets->on_event = close_at_open;
	expected_errno = 0;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	test_wait();
	test_wait();
	test_end();

	test_begin("tcp server open 2");
	assert(sockets->closing == 0);
	assert(sockets->closing_fast == 0);
	assert(sockets->close_guard == 0);
	assert(sockets->confirmed_free == 0);
	sockets->on_event = force_close_at_open;
	expected_errno = 0;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_free(sockets);
	test_wait();
	test_wait();
	test_end();

	test_begin("tcp server open err 5");
	assert(sockets->closing == 0);
	assert(sockets->closing_fast == 0);
	assert(sockets->close_guard == 0);
	assert(sockets->confirmed_free == 0);
	/*
	 * Notice the close after DNS lookup.
	 * The socket is not opened. No errno.
	 */
	sockets->on_event = free_only_strict;
	options.info = NULL;
	options.hostname = "localhost";
	options.port = NULL;
	expected_errno = 0;
	test_preempt_off();
	assert(!tcp_socket(sockets, &options));
	tcp_socket_close(sockets);
	tcp_socket_close(sockets);
	tcp_socket_close(sockets);
	assert(!sockets->close_guard);
	tcp_socket_free(sockets);
	test_preempt_on();
	test_mutex_wait();
	test_end();

	test_begin("tcp server open err 6");
	expected_errno = 0;
	test_preempt_off();
	assert(!tcp_socket(sockets, &options));
	tcp_socket_force_close(sockets);
	tcp_socket_force_close(sockets);
	tcp_socket_force_close(sockets);
	assert(!sockets->close_guard);
	test_preempt_on();
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_end();

	test_begin("tcp server open err 7");
	/*
	 * Notice the close after the open event.
	 * The socket is opened. No errno.
	 */
	sockets->on_event = open_free_only;
	options.info = info;
	expected_errno = 0;
	test_preempt_off();
	assert(!tcp_socket(sockets, &options));
	tcp_socket_close(sockets);
	assert(!sockets->close_guard);
	tcp_socket_free(sockets);
	test_preempt_on();
	test_mutex_wait();
	test_end();

	test_begin("tcp server open err 8");
	expected_errno = 0;
	test_preempt_off();
	assert(!tcp_socket(sockets, &options));
	tcp_socket_force_close(sockets);
	assert(!sockets->close_guard);
	test_preempt_on();
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_end();

	test_begin("tcp send err");
	sockets->on_event = open_close_free;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_close(sockets);
	test_mutex_wait();
	void* ptr = mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(ptr != MAP_FAILED);
	/* This call "fails quick" without any syscalls */
	assert(tcp_send(sockets, &((struct data_frame) {
		.data = ptr,
		.mmaped = 1,
		.len = 1,
		.read_only = 1,
		.dont_free = 1,
		.free_onerr = 0
	})) == -1);
	assert(errno == EPIPE);
	errno = 0;
	test_expect_no_segfault(ptr);
	/* Now, trick the tcp_send() function into thinking the connection is alive */
	test_set_socket_not_closed(sockets);
	assert(tcp_send(sockets, &((struct data_frame) {
		.data = ptr,
		.mmaped = 1,
		.len = 1,
		.read_only = 1,
		.dont_free = 0,
		.free_onerr = 1
	})) == -1);
	assert(errno == EPIPE);
	errno = 0;
	test_expect_segfault(ptr);
	test_restore_socket_closed(sockets);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_end();

	test_begin("tcp send 1");
	sockets->on_event = free_only;
	expected_read = 524288 - 8192;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_cork_on(sockets);
	assert(!setsockopt(sockets->core.fd, SOL_SOCKET, SO_SNDBUF, (int[]){ 1024 }, sizeof(int)));
	test_error_set_errno(send, EINTR);
	test_error(send);
	for(int i = 8192 * 2; i <= 524288; i += 8192) {
		assert(!tcp_send(sockets, &((struct data_frame) {
			.data = send_buf,
			.offset = i - 8192,
			.len = i,
			.read_only = 1,
			.dont_free = 1,
			.free_onerr = 0
		})));
	}
	tcp_socket_cork_off(sockets);
	tcp_socket_close(sockets);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_wait();
	assert(!memcmp(send_buf + 8192, recv_buf, 524288 - 8192));
	test_end();

	test_begin("tcp send 2");
	expected_read = 65536 - 1024;
	memset(recv_buf, 0, recv_buf_len);
	recv_buf_len = 0;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_nodelay_on(sockets);
	tcp_socket_keepalive_on(sockets);
	assert(!setsockopt(sockets->core.fd, SOL_SOCKET, SO_SNDBUF, (int[]){ 1024 }, sizeof(int)));
	test_error_set_errno(send, EINTR);
	for(int i = 1024 * 2; i <= 65536; i += 1024) {
		test_error(send);
		assert(!tcp_send(sockets, &((struct data_frame) {
			.data = send_buf,
			.offset = i - 1024,
			.len = i,
			.read_only = 1,
			.dont_free = 1,
			.free_onerr = 0
		})));
	}
	tcp_socket_keepalive_off(sockets);
	tcp_socket_nodelay_off(sockets);
	tcp_socket_close(sockets);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_wait();
	assert(!memcmp(send_buf + 1024, recv_buf, 65536 - 1024));
	test_end();

	test_begin("tcp send 3");
	expected_read = 524288 - 8192;
	const int fd = memfd_create("shnet_test", 0);
	assert(fd >= 0);
	assert(!ftruncate(fd, 524288));
	assert(write(fd, send_buf, 524288) == 524288);
	memset(recv_buf, 0, recv_buf_len);
	recv_buf_len = 0;
	assert(!tcp_socket(sockets, &options));
	assert(!setsockopt(sockets->core.fd, SOL_SOCKET, SO_SNDBUF, (int[]){ 1024 }, sizeof(int)));
	tcp_socket_cork_on(sockets);
	for(int i = 8192 * 2; i <= 524288; i += 8192) {
		test_error(send);
		assert(!tcp_send(sockets, &((struct data_frame) {
			.fd = fd,
			.file = 1,
			.offset = i - 8192,
			.len = i,
			.read_only = 1,
			.dont_free = (i == 524288) ? 0 : 1,
			.free_onerr = 1
		})));
	}
	tcp_socket_cork_off(sockets);
	tcp_socket_close(sockets);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_wait();
	assert(!memcmp(send_buf + 8192, recv_buf, 524288 - 8192));
	test_end();

	test_begin("tcp graceful shutdown with buffered data");
	expected_read = 1;
	recv_buf_len = 0;
	recv_buf[0] = ~send_buf[0];
	sockets->on_event = open_free;
	sockets->dont_send_buffered = 1;
	assert(!tcp_socket(sockets, &options));
	test_mutex_wait();
	test_buffer_data(sockets, &((struct data_frame) {
		.data = send_buf,
		.len = 1,
		.read_only = 1,
		.dont_free = 1,
		.free_onerr = 0
	}));
	tcp_socket_close(sockets);
	assert(sockets->queue.used == 1);
	assert(tcp_send_buffered(sockets) == -2);
	assert(errno == 0);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_wait();
	assert(send_buf[0] == recv_buf[0]);
	test_end();

	test_begin("tcp send buffered err");
	sockets->on_event = open_free;
	assert(sockets->dont_send_buffered);
	assert(!tcp_socket(sockets, &options));
	test_mutex_wait();
	tcp_socket_close(sockets);
	/* After closing, sending must be disabled */
	assert(tcp_send(sockets, &((struct data_frame) {
		.data = NULL,
		.len = 1,
		.read_only = 1,
		.dont_free = 1,
		.free_onerr = 0
	})) == -1);
	assert(errno == EPIPE);
	errno = 0;
	test_buffer_data(sockets, &((struct data_frame) {
		.data = NULL,
		.len = 1,
		.read_only = 1,
		.dont_free = 1,
		.free_onerr = 0
	}));
	/* While having buffered data too */
	assert(tcp_send(sockets, &((struct data_frame) {
		.data = NULL,
		.len = 1,
		.read_only = 1,
		.dont_free = 1,
		.free_onerr = 0
	})) == -1);
	assert(errno == EPIPE);
	errno = 0;
	assert(tcp_send_buffered(sockets) == -2);
	assert(errno == EPIPE);
	errno = 0;
	assert(sockets->queue.used == 0);
	tcp_socket_free(sockets);
	test_mutex_wait();
	test_end();

	test_begin("tcp free after close");
	sockets->on_event = close_only;
	assert(!tcp_socket(sockets, &options));
	tcp_socket_close(sockets);
	test_mutex_wait();
	assert(tcp_send(sockets, &((struct data_frame) {
		.data = NULL,
		.len = 1,
		.dont_free = 1,
		.read_only = 1,
		.free_onerr = 0
	})));
	async_loop_stop(sockets->loop);
	struct async_loop* loop = sockets->loop;
	tcp_socket_free(sockets);
	async_loop_free(loop);
	free(loop);
	test_end();

	test_begin("tcp read file");
	sockets->on_event = read_something;
	server_onevt = 1;
	assert(!tcp_socket(sockets, &options));
	test_wait();
	test_mutex_wait();
	test_end();

	test_begin("tcp read msg");
	sockets->on_event = read_something;
	send_buf[0] = 'z';
	server_onevt = 2;
	assert(!tcp_socket(sockets, &options));
	test_wait();
	test_mutex_wait();
	test_end();

	test_begin("tcp free");
	tcp_server_close(servers);
	test_mutex_wait();
	net_free_address(info);
	test_end();

	return 0;
}
