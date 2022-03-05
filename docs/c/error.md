# The error facility

This header-only module is used by most, if not all, other modules in the library to make it easier to handle errors.

## Motivation

Ultimately, it is not up to the library to make decisions for the whole application - if it encounters any error that stops it from moving forward, it lets the application know of it. At first glance, this alone is already flawed - some parts of the library, like the `threads` module, have heavily-computational functions, and them failing just to be retried later after the application attempts to fix any errors hurts performance.

Additionally, even if the application has some way of attempting to fix errors (freeing memory, file descriptors, etc.), it might not be aesthetic to add big `if` statements after every function to report the error, or even worse, make a `while` loop to continuously retry.

Moreover, some applications might not want to continue if any error occurs. For instance, if it is a purely computational program that only allocates a few bytes in one place, and the allocation fails, there's no point retrying (because the application doesn't have any memory to free, so it is not its fault), unless spinning for free memory until it succeeds. Both cases are covered by this module.

## Usage

The application must define a function `int error_handler(int)` that  takes in an errno code, returns `0` if it wants the caller to retry the problematic function, or any other value to make the caller stop. Not defining the function will throw a compiler error. The function **MUST NOT** change `errno`, i.e. it might do it, but it must be the same on exit as on start of the function.

```c
int error_handler(int err) {
  switch(err) {
    case ENOMEM: {
      free_memory();
      return 0; /* Continue */
    }
    /* ... cases for other errors ... */
    default: return -1; /* Fail by default */
  }
}
```

The module defines a macro `safe_execute(expr, bool, err_code)`. If `bool` argument is true (anything other than `0`), `error_handler` is called with the given `err_code` (which doesn't need to be a constant - might as well be `errno`). If the handler returns `0`, `safe_execute()` will repeat the `expr` expression, or quit otherwise.

`expr` is only used once in the `safe_execute()` macro, however as long as it resolves to false and `error_handler()` returns `0`, `expr` will be executed multiple times. To prevent undefined behavior, refrain from using macro-unsafe expressions like `x++`.

The following:

```c
while(1) {
  void* ptr = malloc(1024);
  if(!ptr) {
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
 * Can't declare the variable inside of the macro!
 * It won't be visible outside!
 */
safe_execute(ptr = malloc(1024), ptr == NULL, errno);
if(!ptr) {
  /* Impossible with the above code */
}
/* use ptr */
```

In the example above, it is impossible for `ptr` to be `NULL` after the `safe_execute()` call, because `error_handler()` (defined above) never returns failure on `ENOMEM`. The 2 lines of code (besides the redundant `if` statement) are equal to the first part of the example with the `while(1)` loop.

This:

```c
safe_execute(void* ptr = ..., ..., ...);
/* use ptr */
```

... is undefined behavior - `ptr` is limited to the scope of `safe_execute` and is not visible elsewhere.

If `free_memory()` has a chance of not freeing memory, or if the application doesn't want to spin waiting for memory to become available, it can do the following to break out:

```c
int error_handler(int err) {
  switch(err) {
    case ENOMEM: {
      /* Assuming it returns 0 on success, other on failure */
      return try_free_memory();
    }
    /* ... cases for other errors ... */
    default: return -1; /* Fail by default */
  }
}

void* ptr;
safe_execute(ptr = malloc(1024), ptr == NULL, errno);
if(!ptr) {
  /* Oops */
}
```

Note that with this approach, it is naive to call the function again - in the end, the only way for `safe_execute()` to return is if it succeeds or if `try_free_memory()` doesn't have any memory to free. If the call fails, the application can be sure there is nothing more that can be done, and can stop doing whatever it has been doing. This way, only the problematic call is retried, and not the whole function potentially containing other expensive calls.

In an application that can't afford errors, the following approach can be used in conjunction with the `error_handler()` from above:

```c
void* ptr;
safe_execute(ptr = malloc(1024), ptr == NULL, errno);
assert(ptr);
```