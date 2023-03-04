#include <shnet/test.h>
#include <shnet/net.h>

#include <unistd.h>
#include <string.h>


int
main()
{
	test_begin("net sfd");

	const struct addrinfo hints = net_get_addr_struct(
		NET_FAMILY_IPV4,
		NET_SOCK_DATAGRAM,
		NET_PROTO_UDP,
		NET_FLAG_WANTS_SERVER
	);

	const int fd = net_socket_get(&hints);

	assert(fd != -1);

	assert(net_socket_get_family(fd) == NET_FAMILY_IPV4);
	assert(net_socket_get_socktype(fd) == NET_SOCK_DATAGRAM);
	assert(net_socket_get_protocol(fd) == NET_PROTO_UDP);

	test_end();


	test_begin("net addr");

	net_socket_default_options(fd);

	struct addrinfo* info = net_get_address_sync("127.0.0.1", "0", &hints);

	assert(info && "You probably don't have any unused ports.");
	assert(!net_socket_bind(fd, info));

	net_address_t addr;

	net_socket_get_local_address(fd, &addr);

	void* ip = net_address_to_ip(&addr);

	assert(ntohl(*(uint32_t*) ip) == 0x7F000001);

	char str[NET_CONST_IPV4_STRLEN];

	net_address_to_string(&addr, str);

	assert(strcmp(str, "127.0.0.1") == 0);

	test_end();


	test_begin("net cleanup");

	net_free_address(info);

	close(fd);

	test_end();


	return 0;
}
