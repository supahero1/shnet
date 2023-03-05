#ifndef _shnet_test_h_
#define _shnet_test_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_PTHREAD_H) || defined(_DLFCN_H)
#error shnet/test.h must be included first.
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include <shnet/error.h>

#define TEST_FILE_MAGIC 's'



extern void
test_seed_random(void);



extern void
test_begin(const char* str);


extern void
test_end(void);



extern void
test_wait(void);


extern void
test_mutex_wait(void);


extern void
test_wake(void);


extern void
test_mutex_wake(void);


extern void
test_sleep(uint64_t ms);



extern void
test_expect_segfault(const void* ptr);


extern void
test_expect_no_segfault(const void* ptr);



extern void
test_preempt_off(void);


extern void
test_preempt_on(void);



#define test_error_errno(name)	_test_fail_errno_##name
#define test_error_retval(name)	_test_fail_retval_##name
#define test_error_delay(name)	_test_fail_delay_##name
#define test_error_count(name)	_test_fail_count_##name
#define test_error_calls(name) _test_fail_calls_##name

#define test_error(name)	\
test_error_delay(name) = 0;	\
test_error_count(name) = 1

#define test_real(name) _test_real_##name

#define test_register(ret, name, types, args, arg_vals, retval, _errno)		\
int test_error_count(name) = 0;												\
int test_error_delay(name) = 0;												\
int test_error_errno(name) = _errno;										\
ret test_error_retval(name) = (ret) retval;									\
ret (*test_real(name)) types = NULL;										\
int test_error_calls(name) = 0;												\
																			\
ret name types																\
{																			\
	if(test_real(name) == NULL)												\
	{																		\
		test_real(name) = ( ret (*) types ) dlsym(RTLD_NEXT, #name);		\
																			\
		if(test_real(name) == NULL)											\
		{																	\
			test_real(name) = ( ret (*) types ) dlsym(RTLD_DEFAULT, #name);	\
																			\
			if(test_real(name) == NULL)										\
			{																\
				fprintf(													\
					stderr,													\
					"Error overwriting '%s': %s\n",							\
					#name,													\
					dlerror()												\
				);															\
																			\
				abort();													\
			}																\
		}																	\
	}																		\
																			\
	if(test_error_delay(name) != 0)											\
	{																		\
		--test_error_delay(name);											\
																			\
		return test_real(name) args ;										\
	}																		\
																			\
	if(test_error_count(name) != 0)											\
	{																		\
		--test_error_count(name);											\
		++test_error_calls(name);											\
																			\
		errno = test_error_errno(name);										\
																			\
		return test_error_retval(name);										\
	}																		\
																			\
	return test_real(name) args ;											\
}																			\
																			\
__attribute__((constructor))												\
static void																	\
_test_init_##name (void)													\
{																			\
	int save_count = test_error_count(name);								\
	int save_delay = test_error_delay(name);								\
	int save_errno = test_error_errno(name);								\
	ret save_retval = test_error_retval(name);								\
	int cur_errno = errno;													\
																			\
	test_error(name);														\
	test_error_errno(name) = -0xdebeba;										\
	test_error_retval(name) = (ret) -0xdababe;								\
																			\
	assert(name arg_vals == (ret) -0xdababe);								\
	assert(errno == -0xdebeba);												\
																			\
	errno = cur_errno;														\
	test_error_retval(name) = save_retval;									\
	test_error_errno(name) = save_errno;									\
	test_error_delay(name) = save_delay;									\
	test_error_count(name) = save_count;									\
}																			\
																			\
__attribute__((destructor))													\
static void																	\
_test_dest_##name (void)													\
{																			\
	if(test_error_calls(name) <= 1)											\
	{																		\
		puts("> Test function " #name " not used.");						\
	}																		\
}


#define test_use_shnet_malloc()	\
test_register(					\
	void*,						\
	shnet_malloc,				\
	(size_t a),					\
	(a),						\
	(0xbad),					\
	NULL,						\
	ENOMEM						\
)


#define test_use_shnet_calloc()	\
test_register(					\
	void*,						\
	shnet_calloc,				\
	(size_t a, size_t b),		\
	(a, b),						\
	(0xbad, 0xbad),				\
	NULL,						\
	ENOMEM						\
)


#define test_use_shnet_realloc()	\
test_register(						\
	void*,							\
	shnet_realloc,					\
	(void* a, size_t b),			\
	(a, b),							\
	((void*) 0xbad, 0xbad),			\
	NULL,							\
	ENOMEM							\
)


#define test_use_pipe() \
test_register(			\
	int,				\
	pipe,				\
	(int a[2]),			\
	(a),				\
	(NULL),				\
	-1,					\
	ENFILE				\
)


#define test_use_mmap()									\
test_register(											\
	void*,												\
	mmap,												\
	(void* a, size_t b, int c, int d, int e, off_t f),	\
	(a, b, c, d, e, f),									\
	((void*) 0xbad, 0xbad, 0xbad, 0xbad, 0xbad, 0xbad),	\
	MAP_FAILED,											\
	ENOMEM												\
)


#define test_use_pthread_create()											\
test_register(																\
	int,																	\
	pthread_create,															\
	(pthread_t* a, const pthread_attr_t* b, void* (*c)(void*), void* d),	\
	(a, b, c, d),															\
	((void*) 0xbad, (void*) 0xbad, (void*) 0xbad, (void*) 0xbad),			\
	ENOMEM,																	\
	0																		\
)


#define test_use_sem_init()				\
test_register(							\
	int,								\
	sem_init,							\
	(sem_t* a, int b, unsigned int c),	\
	(a, b, c),							\
	((void*) 0xbad, 0xbad, 0xbad),		\
	-1,									\
	EINVAL								\
)


#define test_use_pthread_mutex_init()			\
test_register(									\
	int,										\
	pthread_mutex_init,							\
	(pthread_mutex_t* restrict a,				\
	const pthread_mutexattr_t* restrict b),		\
	(a, b),										\
	((void*) 0x1bad, (void*) 0x2bad),			\
	ENOMEM,										\
	0											\
)


#define test_use_eventfd()		\
test_register(					\
	int,						\
	eventfd,					\
	(unsigned int a, int b),	\
	(a, b),						\
	(0xbad, 0xbad),				\
	-1,							\
	ENOMEM						\
)


#define test_use_epoll_create1()	\
test_register(						\
	int,							\
	epoll_create1,					\
	(int a),						\
	(a),							\
	(0xbad),						\
	-1,								\
	ENOMEM							\
)


#define test_use_epoll_ctl()						\
test_register(										\
	int,											\
	epoll_ctl,										\
	(int a, int b, int c, struct epoll_event* d),	\
	(a, b, c, d),									\
	(0xbad, 0xbad, 0xbad, (void*) 0xbad),			\
	-1,												\
	ENOMEM											\
)


#define test_use_net_socket_get()	\
test_register(						\
	int,							\
	net_socket_get,					\
	(const struct addrinfo* a),		\
	(a),							\
	((void*) 0xbad),				\
	-1,								\
	ENOMEM							\
)


#define test_use_net_socket_bind()		\
test_register(							\
	int,								\
	net_socket_bind,					\
	(int a, const struct addrinfo* b),	\
	(a, b),								\
	(0xbad, (void*) 0xbad),				\
	-1,									\
	ENOMEM								\
)


#define test_use_net_socket_listen()	\
test_register(							\
	int,								\
	net_socket_listen,					\
	(int a, int b),						\
	(a, b),								\
	(0xbad, 0xbad),						\
	-1,									\
	EADDRINUSE							\
)


#define test_use_net_socket_connect()	\
test_register(							\
	int,								\
	net_socket_connect,					\
	(int a, const struct addrinfo* b),	\
	(a, b),								\
	(0xbad, (void*) 0xbad),				\
	-1,									\
	EPIPE								\
)


#define test_use_net_socket_accept()	\
test_register(							\
	int,								\
	net_socket_accept,					\
	(int a),							\
	(a),								\
	(0xbad),							\
	-1,									\
	EPIPE								\
)


#define test_use_net_get_address_sync()							\
test_register(													\
	struct addrinfo*,											\
	net_get_address_sync,										\
	(const char* a, const char* b, const struct addrinfo* c),	\
	(a, b, c),													\
	((void*) 0xbad, (void*) 0xbad, (void*) 0xbad),				\
	NULL,														\
	ENOMEM														\
)


#define test_use_net_get_address_async()	\
test_register(								\
	int,									\
	net_get_address_async,					\
	(struct net_async_address* a),			\
	(a),									\
	((void*) 0xbad),						\
	-1,										\
	ENOMEM									\
)


#define test_use_async_loop()	\
test_register(					\
	int,						\
	async_loop,					\
	(struct async_loop* a),		\
	(a),						\
	((void*) 0xbad),			\
	-1,							\
	ENOMEM						\
)


#define test_use_async_loop_start()	\
test_register(						\
	int,							\
	async_loop_start,				\
	(struct async_loop* a),			\
	(a),							\
	((void*) 0xbad),				\
	-1,								\
	ENOMEM							\
)


#define test_use_async_loop_add()						\
test_register(											\
	int,												\
	async_loop_add,										\
	(const struct async_loop* a, int* b, uint32_t c),	\
	(a, b, c),											\
	((void*) 0xbad, (void*) 0xbad, 0xbad),				\
	-1,													\
	ENOMEM												\
)


#define test_use_recv()						\
test_register(								\
	ssize_t,								\
	recv,									\
	(int a, void* b, size_t c, int d),		\
	(a, b, c, d),							\
	(0xbad, (void*) 0xbad, 0xbad, 0xbad),	\
	-1,										\
	EINTR									\
)


#define test_use_send()							\
test_register(									\
	ssize_t,									\
	send,										\
	(int a, const void* b, size_t c, int d),	\
	(a, b, c, d),								\
	(0xbad, (void*) 0xbad, 0xbad, 0xbad),		\
	-1,											\
	EINTR										\
)


#ifdef __cplusplus
}
#endif

#endif /* _shnet_test_h_ */
