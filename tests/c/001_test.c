#include <shnet/test.h>

#include <errno.h>
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

	test_error_set(pipe, 2);
	test_error_set_retval(pipe, -1);

	assert(test_error_get(pipe) == 2);
	assert(pipe(fds) != -1);

	close(fds[0]);
	close(fds[1]);

	assert(test_error_get(pipe) == 1);
	assert(pipe(fds) == -1);
	assert(errno == ECANCELED);

	errno = 0;
	assert(pipe(fds) != -1);
	assert(errno == 0);

	close(fds[0]);
	close(fds[1]);

	test_end();

	return 0;
}
