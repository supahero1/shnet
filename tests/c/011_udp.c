#include <shnet/test.h>
#include <shnet/udp.h>

#include <unistd.h>
#include <signal.h>


void
handle_sigint(int sig)
{
	(void) sig;
}


size_t total = 0;


void
udp_event(struct udp_socket* socket)
{
	ssize_t read;
	char ip[NET_CONST_IP_MAX_STRLEN];
	struct addrinfo info;
	uint8_t read_buf[1400];

	do
	{
		read = udp_read(socket, read_buf, sizeof(read_buf), &info);

		if(read == -1)
		{
			break;
		}

		total += read;

		net_address_to_string(net_get_address(&info), ip);

		printf("got packet length %lu from %s\n", read, ip);
	}
	while(read > 0);

	if(total == 1400)
	{
		test_wake();
	}
}


int
main()
{
	signal(SIGINT, handle_sigint);

	struct addrinfo hints = net_get_addr_struct(
		NET_FAMILY_IPV4,
		NET_SOCK_DATAGRAM,
		NET_PROTO_UDP,
		NET_FLAG_WANTS_SERVER
	);
	struct addrinfo* info = net_get_address_sync("127.0.0.1", "4334", &hints);

	struct udp_socket client = {0};
	struct udp_socket server = {0};
	server.on_event = udp_event;

	assert(!udp_client(&client, info));
	assert(!udp_server(&server, info));

	uint8_t data[1400] = {0};

	udp_send(&client, data, sizeof(data));

	test_wait();

	udp_free(&client);
	udp_free(&server);

	net_free_address(info);

	test_sleep(100);

	return 0;
}
