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
int main() {
  test_begin("xyz() subsection A");
  /* ... test ... */
  test_end();
}

/* Standard output:
 *
 * Testing xyz() subsection A... done
 */
```

## Error handling

The module defines a very simple `error_handler()`:

```c
int error_handler(int e, int c) {
  if(e == EINTR || e == 0) return 0;
  return -1;
}
```

## Randomness

If the usage of `rand()` is desired, it can be seeded
with current time using `test_seed_random()`.

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
test_register(ret type, name, (args with types and names), (args with only names))
*/
test_register(int, pipe, (int a[2]), (a))
```

The erroneous behavior consists of returning a predefined
value, setting errno to an arbitrary value, and eventually
also defining after how many calls to return the error:

```c
test_error_get_errno(pipe);
test_error_set_errno(pipe, ENFILE);

test_error_get_retval(pipe);
test_error_set_retval(pipe, -1);

/* Throw the retval and errno at first call */
test_error(pipe);

/* Specify after how many calls to throw */
test_error_set(pipe, 1); /* Same as above */
/* First call is normal, second errors out */
test_error_set(pipe, 2);

/* Retrieve in how many calls to throw. 0 = never */
test_error_get(pipe);
```

Note that the above calls are all not thread-safe, and also that
`test_register()` is limited to the scope it was defined in. The
best idea is to call the function at the top-level, before `main()`.
Calling an overrided function is not thread-safe too, unless it
already threw an error and `test_error()` & `test_error_set()`
have not been called since.

To make sure that a function was overrided successfully,
`test_error_check()` may be used like so:

```c
test_register(int, pipe, (int a[2]), (a))

int main() {
  test_error_check(int, pipe, ((int*) 0xdeadbeef));
  
  return 0;
}
```

If the override was successful, then no matter what the arguments given are (the
third argument to the `test_error_check()` macro), nothing will happen. The
arguments are necessary so that the compiler does not throw warnings or errors.
