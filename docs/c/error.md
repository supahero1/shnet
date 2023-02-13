# The error facility

Ultimately, it is not up to the library to make decisions for the whole
application - if it encounters any error that stops it from moving forward,
it lets the application know of it. At first glance, this alone is already
flawed - some parts of the library, like the `threads` module, have
heavily-computational functions, and them failing just to be retried later
after the application attempts to fix any errors hurts performance.

Additionally, even if the application has some way of attempting to fix errors
(freeing memory, file descriptors, etc.), it might not be aesthetic to add big
`if` statements after every function to report the error, or even worse,
a `while` loop to continuously retry.

Moreover, some applications might not want to continue if any error occurs. For
instance, if it is a purely computational program that only allocates a few
bytes in one place, and the allocation fails, there's no point retrying (because
the application doesn't have any memory to free, so it is not its fault), unless
spinning for free memory until it succeeds.

All of the above issues are addressed by this module.

## Dependencies

None.

## Usage

You can define the error handling function `int error_handler(int, int)`
somewhere in your code, an example implementation is below. Furthermore,
a default is provided, which retries all `EINTR` and `0` error codes, and
fails on any other ones. It's defined weakly, so that your own definition
can overwrite it, if needed.

```c
int
error_handler(int err, int count)
{
	switch(err)
	{

	case ENOMEM:
	{
		free_memory();

		return 0; /* Continue */
	}

	case EINTR: return 0;

	/* ... cases for other errors ... */

	default: return -1; /* Fail by default */

	}
}
```

This module also defines a macro `safe_execute(expr, bool, err_code)`. The
`expr` is evaluated at first. Then, if `bool` is true (anything other than `0`),
`error_handler` is called with the given `err_code` (which doesn't need to be a
constant - might as well be `errno`) and an error count denoting how many times
this particular error has occured in a row, counting from `0`. If the handler
returns `0`, `safe_execute()` will repeat the `expr` expression, or quit
otherwise.

`expr` is only used once in the `safe_execute()` macro, however as long as it
resolves to false and `error_handler()` returns `0`, `expr` will be executed
multiple times. To prevent undefined behavior, refrain from using macro-unsafe
expressions like `x++`.

The following:

```c
while(1)
{
	void* ptr = malloc(1024);

	if(!ptr)
	{
		free_memory();

		continue;
	}

	break;
}
```

Can be written as the following using this module:

```c
void* ptr;

/*
 * Can't declare the variable inside of the macro.
 * It won't be visible outside, because the macro
 * implements a scope.
 */
safe_execute(ptr = malloc(1024), ptr == NULL, errno);

if(!ptr)
{
	/* Impossible with how error_handler() is written above */
}

/* use ptr */
```

In the example above, it is impossible for `ptr` to be `NULL` after the
`safe_execute()` call, because `error_handler()` (defined above) never returns
failure on `ENOMEM`. The 2 lines of code (besides the redundant `if` statement)
are equal to the first part of the example with the `while(1)` loop.

This:

```c
safe_execute(void* ptr = ..., ..., ...);

/* use ptr */
```

... is invalid - `ptr` is limited to the scope of
`safe_execute` and is not visible elsewhere.

If `free_memory()` has a chance of not freeing memory, or if
the application doesn't want to spin waiting for memory to
become available, it can do the following to break out:

```c
int
error_handler(int err, int count)
{
	switch(err)
	{

	case ENOMEM:
	{
		/* Assuming it returns 0 on success, other on failure */

		return try_free_memory();
	}

	/* ... cases for other errors ... */

	default: return -1; /* Fail by default */

	}
}


void* ptr;

safe_execute(ptr = malloc(1024), ptr == NULL, errno);

if(!ptr)
{
	/* Oops */
}
```

Note that with this approach, it is naive to call the function again - in the
end, the only way for `safe_execute()` to return is if it succeeds or if
`try_free_memory()` doesn't have any memory to free. If the call fails, the
application can be sure there is nothing more that can be done, and can stop
doing whatever it has been doing. This way, only the problematic call is
retried, and not the whole function potentially containing other expensive
calls.

In an application that can't afford errors, the following approach
can be used in conjunction with the `error_handler()` from above:

```c
void* ptr;

safe_execute(ptr = malloc(1024), ptr == NULL, errno);

assert(ptr);
```

This is not the same as if only using the assertion, because your application
might allocate excess memory that it doesn't ever clean up, and when it comes
to allocating another chunk, you first need to do a cleanup, after which you
can do the allocation and make sure it succeeded.

If you want to give up retrying after X times, you can use the second argument:

```c
int
error_handler(int err, int count)
{
	switch(err)
	{

	case ENOMEM:
	{
		if(count == 2)
		{
			/* Failed 3 times, no hope */

			return -1;
		}

		free_memory();

		return 0;
	}

	default: return -1;

	}
}
```

## Valgrind

For tests, it's often good to also test erroneous code paths that normally
aren't executed. However, Valgrind, which is used to test for memory leaks and
other issues in this library, wraps a lot of functions into its own functions
that check for correctness and issues. These functions basically can't be
wrapped by the library again so that an invalid value could be returned to test
a different code path. So instead, this module exposes a few functions that wrap
these functions beforehand, but instead of wrapping them with the same function
name, a different function is used, so that Valgrind does not override it and
ruin everything. Later on, the test module `<shnet/test.h>` can be used to
trigger errors in these functions:

```c
void*
shnet_malloc(const size_t);

void*
shnet_calloc(const size_t, const size_t);

void*
shnet_realloc(void* const, const size_t);
```

There are a lot of other functions that Valgrind overrides too, but these ones
prove to be crucial in error paths, or the other ones never return any errors.
You absolutely do not need to use these wrappers, but just know that they are
used in the source code to achieve higher test coverage.

These functions also use `safe_execute()` whenever applicable, so them returning
an error code means no further chance of successfully calling them again (with a
well configured `error_handler`).
