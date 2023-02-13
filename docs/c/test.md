# The testing module

The highest possible test coverage is very important in every open-source
project. It guarantees to anyone viewing the source code that this is indeed
working all of the time under any circumstance and without any memory leaks.

The case with shnet was no different. This module was born to provide many
helpful tools to detect any errors or cause them, and to help with setting up
a suitable environment for testing.

## Dependencies

None.

## Marking boundaries of a test

Since infinite loops are not very uncommon in new code, sometimes it could be a
nuisance to write thousands of lines of legitimate tests just to find a bug in
the tested code that triggers an infinite loop or a deadlock. Do you go over
all of the thousands lines of code to find out where it really is?

To solve this problem and possibly others too (better organisation?), when a
given function or subsection of a function is being tested, some text can be
written to the standard output to notify the user what is being tested and
when it's finished:

```c
int
main()
{
	test_begin("xyz() subsection A");

	/* ... test ... */

	test_end();

	return 0;
}

/* Standard output:
 *
 * Testing xyz() subsection A... done
 */
```

## Error handling

The error module by default defines a very simple `error_handler()` that allows
an infinite number of `EINTR` errors to pass, as well as no errors (code `0`).
The rest are handled as if they can't be resolved.

If a test suite needs it's own error handling function, it can simply define
one. The default definition of the function is weak - it can be overwritten.

## Randomness

If the usage of `rand()` is desired, it can be seeded
with current time using `test_seed_random()`.

Additionally (configured in `Makefile`'s), all tests receive a
preprocessor macro `__RANDOM__` filled with a random 16bit number.

## Waiting and sleeping

Any thread in a test may sleep for an arbitrary number of
milliseconds using the `test_sleep(uint64_t)` function.

Additionally, if a test suite has a linear flow, `test_wait()` and
`test_wake()` maybe used to wait until an action is done asynchronously:

```c
/* main */
init_test();
test_wait();

/* test thread */
do_something();
test_wake();
```

`test_wake()` may be executed multiple times before calling `test_wait()`.
In this case, `test_wait()` will quit immediatelly (for as long as many
`test_wake()` calls were performed).

`test_wait()` and `test_wake()` are based on a semaphore, and as you may or may
not know, semaphores are a cancellation point AND they can return `EINTR`.
Because of that, tests that rely on thread cancellation or such can't use these
functions. Instead, they should use `test_mutex_wait()` and `test_mutex_wake()`,
which are equivalent to the above, with the difference that they are not
recursive - you can't wake multiple times like with `test_wake()`. These
functions are resistant to any cancellation requests and signals.

## Segmentation faults

If you want to make sure a pointer triggers a segmentation fault
without quite literally throwing from the program, you can use
`test_expect_segfault(const void*)`. If a segfault is triggered,
the program flow will continue as if nothing happened and the
function will return. Otherwise, the program will `assert(0)`
with a meaningful message to help debugging.

Its counterpart is the `test_expect_no_segfault(const void*)`
function that expects there is no segmentation fault. If one
does occur, the program will halt.

Note that checking for a segmentation fault on pointers returned by `malloc()`
doesn't really make sense, because that memory is not guaranteed to segfault.
On the other hand, after calling `munmap()`, the referenced memory region must
segfault, according to the specification. It is fine to check for memory leaks
from `malloc()` by simply using AddressSanitizer or Valgrind.

## Overriding functions

To increase code coverage, it is often desired to also trigger the erroneous
paths. That is rather hardly achievable without specially prepared code, and
besides it's way easier and better to just throw an error from a function the
code depends on.

First of all, to trigger erroneous behavior in
a function, it first needs to be "registered":

```c
/*
test_register(
	return type,
	function name,
	(args with types and names),
	(args with only names),
	(test arguments to mock with)
)
*/
test_register(
	int,
	pipe,
	(int a[2]),
	(a),
	(NULL)
)
```

The mock argument values can be pretty much anything,
excluding the "usual" values the arguments might take.

The erroneous behavior consists of returning a predefined value, setting errno
to an arbitrary value, erroring out a number of times in a row, and having a
delay before starting to error the given function.

```c
test_error_errno(pipe) = ENFILE;

test_error_retval(pipe) = -1;

/* Throw the retval and errno at first call only */
test_error(pipe);

/* Specify after how many calls to throw */
test_error_delay(pipe) = 0; /* Same as above */

/* First call is normal, second errors out */
test_error_delay(pipe) = 1;

/* The number of times to fail */
test_error_count(pipe) = 2; /* Fail twice in a row after the given delay */
```

Note that the above calls are all not thread-safe, and also that
`test_register()` is limited to the scope it was defined in. The
best idea is to call the function at the top-level, before `main()`.

There are also a bunch of predefined `test_register` macros for functions
that commonly appear across tests. The syntax for them is `test_use_xxx()`.
A few examples include"

```c
test_use_shnet_malloc()

test_use_pipe()
```

These calls before `main()` in the global scope will
register the functions "shnet_malloc" and "pipe", and
mock test them to make sure the overriding has succeeded.

There are many more of these - for a full list see the header
file `test.h`. If there isn't a macro for a function, you will
need to explicitly use `test_register()` as specified above.
