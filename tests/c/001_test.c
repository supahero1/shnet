#include <shnet/test.h>

#include <unistd.h>


test_use_shnet_malloc()
test_use_shnet_calloc()
test_use_shnet_realloc()
test_use_pipe()


int
main()
{
	test_begin("test register/use");

	int fds[2];

	assert(pipe(fds) != -1);

	close(fds[0]);
	close(fds[1]);

	test_error_delay(pipe) = 1;
	test_error_count(pipe) = 1;
	test_error_retval(pipe) = -1;

	assert(pipe(fds) != -1);

	close(fds[0]);
	close(fds[1]);

	assert(test_error_delay(pipe) == 0);
	assert(pipe(fds) == -1);
	assert(errno == ENFILE);

	errno = 0;
	assert(pipe(fds) != -1);
	assert(errno == 0);

	close(fds[0]);
	close(fds[1]);

	test_end();

	return 0;
}
