# Code of conduct

This directory contains documentation of all actively used modules in the
library. Documentation of WIP modules might not be here yet. Symbols that
aren't supposed to be used by the application might not be documented.

Each module has a list of dependencies somewhere at the top of its
documentation (or it doesn't, meaning it's a leaf). You should get
to know them first or otherwise you might not underestand what the
documentation explains.

**Unless specified otherwise...**

- ... before using any structure, zero it first.

	```c
	struct some thing = {0};
	```

- ... functions return 0 on success and other values
    on failure, setting errno to the error's code.

	```c
	/*
	assert(! error );
	assert(no error);
	*/
	assert(! func());

	if(func()) /* if(error occured){} */
	{
		/* ... */
	}
	```

- ... structures can be reused immediatelly after a call to an appropriate
    function freeing them (if any). They can only be reused if they will
    be used the same way they were before. Otherwise, full zeroing is needed.

	```c
	struct data_type data = {0};

	data_type_init(&data);

	/* ... use it ... */

	data_type_free(&data);

	/* ready to be used again, without zeroing */
	data_type_init(&data);

	data_type_free(&data);

	/* can't reuse for a different purpose without zeroing */
	data = {0};

	other_data_type_init(&data);
	```

- ... structures and functions are thread-unsafe by default.

	```c
	/* thread 1: */
	do_something(&structure); /* OK */

	/* thread 2: */
	do_something_else(&structure); /* UNDEFINED BEHAVIOR */
	```

- ... functions that end with the word "raw" are always thread-unsafe. The
    documentation will mention if there exists a pair of functions where
    one is raw and the other is not. In that case, the non-raw function
    is always thread-safe and there must exist a locking function for
    the data type to allow the usage of raw functions.

	```c
	/* explicitly stated in the docs that some_function has a raw equivalent */

	/* thread-safe */
	some_function(some_structure);

	/* ... */

	some_structure_lock(some_structure);

	/* thread-unsafe, but under a lock, so ok */
	some_function_raw(some_structure);

	some_other_function_raw(some_structure);

	some_structure_unlock(some_structure);
	```

# Style

Indentation: tabs, size 4.

Line width limit: 80.

C-style comments, no `//`.

Braces on the next line, no exceptions.

Always put a newline between expressions of different type or meaning:

```c
if()
{}
/* *required* newline here */
void* ptr = NULL;
/* *maybe* here */
int counter = 0;
/* *required* before comments. *maybe* after too. */
/* This is probably not needed. */
int second_counter = 0;
/* *not* here */
int third_counter = 0;

/* braces count as newlines */
if(expr)
{
	int g = 0;

	if(expr2)
	{
		int h = 0;
	}

	return g * h;
}
```

```c
if()
{}

void* ptr = NULL;
int counter = 0;

/* This is probably not needed. */

int second_counter = 0;
int third_counter = 0;
```

Different `if`, `for`, `while`, `do..while`, and `switch` statements are always
"different" in the sense that they should have a newline separating them if they
occur more than once in a row.

Switch statement's cases should not be indented. They should contain an
additional newline at the beginning and end of the switch statement:

```c
switch(expr)
{

case 5:
{
	break;
}

case 123: return -1;
case 124: return -2;
case 125: return -3;

default: return 0;

}
```

There must be 2 newlines separating definitions, like function declarations
and definitions, as well as structs, and distinct `#define`'s. There may be
more to clearly state a group of definitions is distinct from some other group.

In source files, creating structs must follow the following style:

```c
struct something something =
(struct something)
{
	/* something */
};
```

..., or:

```c
something(&(
(struct something)
{
	/* something */
}
));
```

Functions must have any C keywords appear on the line prior. Any extensions,
like attributes, must appear on the line prior to that:

```c
__attribute__((unused))
extern int
some_function(void);


int
some_function(void)
{
	/* body */
}
```

Single-line `if`, `while`, `do..while`, and `for` statements are not allowed:

```c
if(a)
	break; /* *not* allowed */

if(a) break; /* *not* allowed */

if(a)
{
	break; /* that's the way */
}
```
