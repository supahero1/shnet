#include <shnet/test.h>

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>


void
test_seed_random()
{
	/* Cannot rely on any modules, including the time module. */

	struct timespec tp;

	(void) clock_gettime(CLOCK_REALTIME, &tp);
	srand(tp.tv_nsec + tp.tv_sec * 1000000000);
}



void
test_begin(const char* const str)
{
	assert(printf("Testing %s... ", str) >= 0);
	assert(fflush(stdout) == 0);
}


void
test_end()
{
	assert(puts("\033[32m" "pass" "\033[0m") >= 0);
}



static pthread_once_t _test_waiting_once = PTHREAD_ONCE_INIT;
static sem_t _test_sem;
static pthread_mutex_t _test_mutex = PTHREAD_MUTEX_INITIALIZER;


static void
_test_waiting_destroy(void)
{
	assert(!sem_destroy(&_test_sem));
}


static void
_test_waiting_init(void)
{
	assert(!sem_init(&_test_sem, 0, 0));
	assert(!pthread_mutex_lock(&_test_mutex));
	assert(!atexit(_test_waiting_destroy));
}


void
test_wait()
{
	(void) pthread_once(&_test_waiting_once, _test_waiting_init);
	while(sem_wait(&_test_sem) != 0);
}


void
test_mutex_wait()
{
	(void) pthread_once(&_test_waiting_once, _test_waiting_init);
	(void) pthread_mutex_lock(&_test_mutex);
}


void
test_wake()
{
	(void) pthread_once(&_test_waiting_once, _test_waiting_init);
	assert(!sem_post(&_test_sem));
}


void
test_mutex_wake()
{
	(void) pthread_once(&_test_waiting_once, _test_waiting_init);
	(void) pthread_mutex_unlock(&_test_mutex);
}


void
test_sleep(const uint64_t ms)
{
	(void) nanosleep(&(
	(struct timespec) {
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000) * 1000000
	}
	), NULL);
}



static volatile char _test_var;
static jmp_buf _test_jump;
static pthread_once_t _test_segv_once = PTHREAD_ONCE_INIT;


static void
_test_sigsegv(int code)
{
	(void) code;

	siglongjmp(_test_jump, 1);
}


static void
_test_segv_init(void)
{
	assert(!sigaction(SIGSEGV, &(
	(struct sigaction) {
		.sa_handler = _test_sigsegv
	}
	), NULL));
}


void
test_expect_segfault(const void* const ptr)
{
	/*
	 * Valgrind (and most other tools) do not detect memory leaks
	 * created by mmap(). Fortunately though, man pages mention
	 * that further access to a munmap()'ed memory region will
	 * result in a SIGSEGV. As a result, this function triggers
	 * the signal, and if it succeeds, the program flow continues.
	 * If it fails for some reason (munmap() wasn't called), the
	 * program will throw.
	 */
	(void) pthread_once(&_test_segv_once, _test_segv_init);

	if(!sigsetjmp(_test_jump, 1))
	{
		_test_var = *(char*)ptr;

		assert(!"Expected a segfault, but it did not happen");
	}
}


void
test_expect_no_segfault(const void* const ptr)
{
	_test_var = *(char*)ptr;
}
