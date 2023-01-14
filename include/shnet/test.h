#ifndef _shnet_test_h_
#define _shnet_test_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include <shnet/error.h>


#ifndef __RANDOM__
#define __RANDOM__ 0
#endif

#define TEST_FILE_MAGIC 's'



extern void
test_seed_random(void);



extern void
test_begin(const char* const);


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
test_sleep(const uint64_t);



extern void
test_expect_segfault(const void* const);


extern void
test_expect_no_segfault(const void* const);



#define test_error_get_errno(name) _test_fail_##name##_errno
#define test_error_set_errno(name, code) test_error_get_errno(name) = code

#define test_error_get_retval(name) _test_fail_##name##_retval
#define test_error_set_retval(name, code) test_error_get_retval(name) = code

#define test_error_get(name) _test_fail_##name##_count
#define test_error_set(name, num) test_error_get(name) = num
#define test_error(name) test_error_set(name, 1)

#define test_real(name) _test_##name##_real

#define test_register(ret, name, types, args, arg_vals)						\
int test_error_set(name, 0);												\
int test_error_set_errno(name, ECANCELED);									\
ret test_error_set_retval(name, (ret) -1);									\
ret (*test_real(name)) types = NULL;										\
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
				puts(dlerror());											\
				assert(!"shnet dlsym error for function " #name);			\
			}																\
		}																	\
	}																		\
																			\
	if(test_error_get(name) != 0 && !--test_error_get(name))				\
	{																		\
		errno = test_error_get_errno(name);									\
		return test_error_get_retval(name);									\
	}																		\
																			\
	return test_real(name) args ;											\
}																			\
																			\
__attribute__((constructor))												\
static void																	\
_test_##name##_init (void)													\
{																			\
	int err_save = test_error_get(name);									\
																			\
	test_error(name);														\
																			\
	ret retval_save = test_error_get_retval(name);							\
																			\
	test_error_set_retval(name, (ret) -0xdababe);							\
																			\
	int errno_save = errno;													\
	int err_errno_save = test_error_get_errno(name);						\
																			\
	test_error_set_errno(name, -0xdebeba);									\
																			\
	assert(name arg_vals == (ret) -0xdababe);								\
	assert(errno == -0xdebeba);												\
																			\
	test_error_set_retval(name, retval_save);								\
																			\
	errno = errno_save;														\
																			\
	test_error_set_errno(name, err_errno_save);								\
	test_error_set(name, err_save);											\
}

#define test_use_shnet_malloc()	\
test_register(					\
	void*,						\
	shnet_malloc,				\
	(const size_t a),			\
	(a),						\
	(0xbad)						\
)

#define test_use_shnet_calloc()			\
test_register(							\
	void*,								\
	shnet_calloc,						\
	(const size_t a, const size_t b),	\
	(a, b),								\
	(0xbad, 0xbad)						\
)

#define test_use_shnet_realloc()		\
test_register(							\
	void*,								\
	shnet_realloc,						\
	(void* const a, const size_t b),	\
	(a, b),								\
	((void*) 0xbad, 0xbad)				\
)

#define test_use_pipe() \
test_register(			\
	int,				\
	pipe,				\
	(int a[2]),			\
	(a),				\
	(NULL)				\
)

#define test_use_mmap()									\
test_register(											\
	void*,												\
	mmap,												\
	(void* a, size_t b, int c, int d, int e, off_t f),	\
	(a, b, c, d, e, f),									\
	((void*) 0xbad, 0xbad, 0xbad, 0xbad, 0xbad, 0xbad)	\
)

#ifdef __cplusplus
}
#endif

#endif /* _shnet_test_h_ */
