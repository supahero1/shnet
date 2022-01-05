**Shnet** is an asynchronous networking library created in "modules", each of which can be used by the application independently.

Table of contents
- [Requirements](#requirements)
- [Building](#building)
- [Code of conduct](#code-of-conduct)
- [Documentation](#documentation)
  - [error.h](#error)
  - [debug.h](#debug)
  - [aflags.h](#aflags)
  - [storage.h](#storage)
  - [heap.h](#heap)
  - [refheap.h](#refheap)
  - [threads.h](#threads)
  - [time.h](#time)
  - [net.h](#net)
  - [tcp.h](#tcp)

# Requirements

- [Linux kernel](https://www.kernel.org/)
- [GCC](https://gcc.gnu.org/)
- [Make](https://www.gnu.org/software/make/)

To install the dependencies, one can do (debian):
```bash
sudo apt update -y
sudo apt install gcc libssl-dev make -y
```

..., or (fedora):
```bash
yum groupinstall "Development Tools" -y
yum install openssl-devel -y
```

# Building

```bash
git clone -b master https://github.com/supahero1/shnet
cd shnet
```

The library is available in static and dynamic releases. Additionally, if the user lacks sudo perms, a stripped install that doesn't copy the header files into `include` can be chosen, but then the files must be included manually in one's project.

```bash
sudo make dynamic
sudo make strip-dynamic
sudo make static
sudo make strip-static
```

To build without installing:
```bash
sudo make build
```

If full build was chosen, the user can then test the library:
```bash
sudo make test
```

To remove build files:
```bash
sudo make clean
```

To uninstall the library:
```bash
sudo make uninstall
```

To link it with your project, simply add the `-lshnet` flag.

Multiple `make` commands can be chained very simply. The following command performs a full dynamic reinstall and tests the library afterwards:
```bash
sudo make clean uninstall dynamic test
```

If a dynamic build doesn't work for you, try running `ldconfig` first to make sure the system knows about the library.

# Code of conduct

Before using any library structure, such as callbacks or settings, be sure to zero it. A simple `= {0}` should suffice.

Most library functions return `0` on success and negative values on failure, starting with `-1` and going down if there are different kinds of errors to report and `errno` is already in usage. If an error occured, check `errno` for details.

# Documentation

## error
This module exists to allow for greater flexibility of handling errors. The application must supply the library with a function that receives an `errno` code. What happens later is not any of the library's concern, but the application must return 0 on success (the underlying code will retry whatever it was doing) or any other number on failure (the underlying code will quit whatever it was doing). It might be called multiple times in a row with the same `errno`.
```c
#include <shnet/error.h>

int handle(int err) {
  switch(err) {
    case ENOMEM: {
      /* Try freeing excess application resources or such, or
      just return something other than 0 if can't do anything */
      break;
    }
    default: return -1;
  }
  return 0;
}

int main() {
  handle_error = handle; /* <--- The trick. "handle_error" is exported
  by shnet/error.h, but never initialised to anything meaningful. */
  
  /* Let's do something that might fail */
  void* ptr;
  /* safe_execute(expression, error_condition, error)
  where *error* might be errno or anything else */
  safe_execute(ptr = malloc(4096), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    /* Failed allocating the memory AND handle_error() failed to free some */
    return -1;
  }
  
  return 0;
}
```

**All** library functions are using this module to handle errors. That's also why calling the same function after it failed misses the point.

## debug
A module mainly used in testing at the moment, but the header can be added to any code wishing to log some stuff to a text file.
Run `sudo make dynamic/static debug=true` to enable logging capabilities of the `debug()` function. If you wish to always be able to log something and not depend on the flag, use `_debug()`.

Upon first usage, these functions will open a new file called `logs.txt` in the current directory and append new logs to the end of it.
If you need to free all memory upon exit, including the pointer to the file, run `debug_free()`.

The syntax is as follows:
```c
void debug(string, should_it_appear_on_stdout_boolean, any_string_args...);
```
If the second argument is `1`, the logged string will also appear in the `stdout`. Note that there's no formatting in files, so if you want your `stdout` to look pretty, consider logging formatted text in the console using `printf()` and not formatted to the text file using `debug()`.
```c
_debug("critical error, errno %d", 0, errno);

debug("a new connection on the server", 1);
```
File logging also has timestamps next to each new entry. They are not included in `stdout` logs if the bool is `1`. The formatting at the moment supports up to 1000 days of non stop logging, after which the formatting will break. It is designed to count time since the first log (first call to `debug()` or `_debug()`) and not since the epoch or show current date. Minimum timestamp accuracy is 1 microsecond, allowing for benchmarking or other time-precise processes.

## aflags
This module takes care of all kinds of atomic operations on a variety of data widths: 8, 16, 32 and 64 bits. Each of provided functions handling various atomic operations can be invoked manually, but to not need to remember all of the names, C11's `_Generic` statement is used to handle all data widths at once.

The motivation for this module was a need of having an atomic bit field in a struct, which is sadly not possible. Thus, an easy way of handling bytes instead of bits was necessary, which is what this module achieves.

Note that this isn't a module about atomic arithmetics, but about flags. There won't be addition in a sense of a plus sign, but rather in a sense of ORing 2 numbers together. That's what flags do.

There are 2 types of functions: ones having sequentially consistent memory ordering, and ones having acquire-release memory ordering. The latter family of functions have the same names as the former, but with a `2` at the end, so `aflag_xxx2` will be acquire-release memory ordered, while `aflag_xxx` will be sequentially consistent.
- `aflag_get(a)` loads the atomic flag,
- `aflag_add(a, b)` ORs `b` with `a` and stores the result in `a`,
- `aflag_del(a, b)` removes `b` from `a`,
- `aflag_test(a, b)` checks if `b` is in `a`,
- `aflag_clear(a)` zeroes the atomic flag.

## storage
The storage module makes it easy to store many pointers to various memory segments of varying size and of various needs.

The initialisation can't be any easier:
```c
struct data_storage storage = {0};
```

Note that the `struct data_storage` structure is **packed** - it takes 12 bytes on 64bit systems (instead of 16) and 8 on 32bit.

Let's suppose I have some memory to keep for later, but for obvious reasons I won't just hardcode the pointer names:
```c
char string[] = "Hello, world!";
void* ptr = malloc(64);
void* shared_ptr = malloc(16);
```

Every one of these pointers have different characteristics that the module can handle:
- `string[]` is hardcoded in the memory and can't be freed nor overwritten, thus doesn't need to be copied to a different location since it will never change - not temporary, read-only
- `ptr` is a temporary memory segment I want to use and get rid of afterwards - temporary, read-only
- `shared_ptr` is a static memory segment being used in multiple places (can't change location) and can't be freed afterwards, but its contents can change - not temporary, not read-only

Because of the "temporary or not" and "read-only or not" characteristics, there also exist `data_dont_free` and `data_read_only` enum members controlling the behavior of other `data_storage` functions. Of course, the safest way to do this would be to always copy the given memory region into a newly allocated space and not worry about anything, but that's a very bad way of dealing with things. For instance, given I have a CDN that needs to host multiple huge files, I would under no circumstance want to trash the server's memory with multiple copies of the files just to send them over the wire. Instead, the better approach is to declare the files read-only and then only a pointer to them will be used in the underlying code, no copies made. This approach only requires the server to load the file once.

Syntax is as follows:
```c
int err = data_storage_add(&storage, string, sizeof(string), data_dont_free | data_read_only);
```

Note that if the function returns `-1`, meaning "no memory" failure, and `data_dont_free` was not specified, `string` **will be freed**. Also note that this will be an extreme case in which all calls to `handle_error()` failed, so retrying again would be the last thing anyone would consider doing. That's why freeing the data is the correct approach, even if unwanted by the application in some cases. If you don't want that to happen, try calling the function with only the `data_dont_free` flag. The underlying code will then create a copy of the given memory segment, and free that copy if anything goes wrong instead of the input memory.

Accessing the first data piece in the storage:
```c
void* data = storage.frames->data;
uint64_t len = storage.frames->len;
uint64_t offset = storage.frames->offset;

// or

struct data_storage_frame latest = storage.frames[0];
```

The `offset` member is used to denote how many bytes of the memory segment have already been read by the application. If that number reaches `0`, the frame will be disposed of, and its data segment will be freed if `data_dont_free` was not specified in the call adding it.

Increasing the "read bytes" count:
```c
int end = data_storage_drain(&storage, 12);
```

If `end` is 1, there are no more data frames in the queue - we have drained everything there is. That can also be double checked using:
```c
int end = data_storage_is_empty(&storage);
```

If the application wishes to be memory-friendly, it might want to consider cleaning up bytes which have already been read using `data_storage_drain()`. Note that this will only work if the current frame was not declared as read-only, but that's checked by the function.
```c
data_storage_finish(&storage);
```

The function doesn't return any failure code, because if it fails to reallocate the memory, it will simply stick to the old one. If you wish to detect failures, check `errno`, although it might not be very useful, since the function will only fail after `handle_error` returns failure.

## heap
This module features a flexible heap implementation that can be used for any item sizes. It's about 2 times slower than if it was made for a certain data type.

Because the underlying code doesn't care how big the data is or what it represents, the application must supply a function which compares 2 pieces of data and returns `>0` if `a > b`, `<0` if `a < b`, and `0` if `a == b` (so that it can simply return `a - b` for simple data types).

Initialisation:
```c
struct heap heap = {0};

heap.sign = heap_min; /* Or heap_max */

int cmp(const void* a, const void* b) {
  return *((int*) a) - *((int*) b);
}

heap.compare = cmp;

heap.item_size = sizeof(int);

/* Didn't want to make a whole heap_init()
function just for these two lines */
heap.size = heap.item_size;
heap.used = heap.item_size;
```

As you can see, this heap will be a min heap (having the minimum value at the root) and will handle `int`s.

The heap items are indexed from 1 instead of 0 to slightly speed up computation, but also because not knowing the data type requires that, so that some functions don't need to dynamically allocate memory. In that case, the 0 index is used by these functions to move items around and more. `heap_pop()` is also using that to place the root at the 0 index and make it be the return value, no allocations required.

When popping items from the heap, it will not be shrunk. The application can control this behavior and/or allocate more memory for the heap before inserting a lot of elements using the `heap_resize()` function:
```c
int no_mem = heap_resize(&heap, heap.item_size * 5);
```

Note that the second argument, `new_size`, must be an absolute byte count, not just the number of elements or *new* elements. In the example above, `heap_insert()` can now be called 4 times without a risk of throwing an out-of-memory error. The fifth is the 0 index, not used for new elements, but rather only for computational benefits mentioned above.

Insertion:
```c
int no_mem = heap_insert(&heap, &(int){ 1 });
// ...
no_mem = heap_insert(&heap, &(int){ 0 });
```

Note that the second argument must be a pointer to the item, not the item itself.

Some helper functions:
```c
heap_free(&heap); /* Frees the heap, can be used again immediatelly after this.
The item size will remain the same, all of the previous items will be deallocated.
If you wish to not free all of the memory the heap has built up, you can do: */
heap.used = heap.item_size;

int is = heap_is_empty(&heap); /* Checks if there are any items in it */

void* root = heap_peak(&heap, heap.item_size); /* Makes it able to view any item
in the heap without deleting them. The index must be a multiple of heap.item_size.
The pointer is only valid until any other heap function is called. */

/* Given you have a pointer to a node, like root above, you can also retrieve
its absolute index: */
uint64_t idx = heap_abs_idx(&heap, root);
/* This absolute index can then be used with heap_down() and heap_up() functions,
explained a few code blocks below. */

root = heap_peak_rel(&heap, 1); /* The equivalent of the above function, but the
index is divided by heap.item_size to make it more intuitive if needed. */
```

Currently, the heap has `0` as the root, since it's smaller than `1`. We can retrieve that value by using:
```c
root = heap_pop(&heap);
```

Note that this doesn't have the same effect as `heap_peak()`, because this function updates the whole heap. The pointer, yet again, is only valid until any other heap function is called. It is not allocated, and you can modify the contents of the pointer.

There is also a version of the function which doesn't do additional memory shifting and returns nothing:
```c
heap_pop_(&heap);
```

The module also exposes a few internal functions to allow for advanced heap manipulation if required:
```c
heap_down(&heap, index);

heap_up(&heap, index);
```
Where `index` is the absolute index of an item, explained above.

These functions try relocating the item at the given index up or down the heap. The index is an absolute index measured in bytes from the beginning of the array, so that you can do:
```c
void* item = heap_peak_rel(&heap, 3);

/* ... modify the item */

heap_down(&heap, heap_abs_idx(item));
```

Choosing `heap_down()` or `heap_up()` is a matter of sign of the heap and the change made to the item. For instance, if the heap was a max heap, and the value of an item was increased (as in, the comparing function will now return a greater weight for it), `heap_up()` would then need to be used. Using `heap_down()` would not be invalid though, it will simply waste a few CPU cycles figuring out there's nothing to do.

If you have no idea if the modified item is greater or less in weight than its previous state, you can use both `heap_down()` and `heap_up()` to make sure it's updated accordingly no matter what.

Additionally, `heap_min` and `heap_max` are `-1` and `1` respectively, allowing for calculation via multiplication to not write excessive `if` statements. This fact is used internally as well.

## refheap
Refheaps are heaps, but with one additional very important ability - to remove any items from the heap, not only the root.

The goal was to create something much lighter than a binary tree, while still having its characteristic of fast deletion. Binary trees are too much for this job, because they also allow for quick searches, which is not needed. They also take significantly more memory to keep track of children.

Thus, this module has been born. A refheap is a normal heap, but with an additional pointer bound to every inserted item that points to an application-defined variable that will be updated with the position of the item in the tree. That application-defined variable might not exist, meaning the item is not to be tracked, and then the internal pointer is simply NULL.

Initialisation is very similar to heaps, but with one important difference:
```c
struct heap refheap = {0};

// ... the same

refheap.item_size = sizeof(int) + sizeof(uint64_t**); /* <---- */
```

The only requirement for using refheaps is adding `sizeof(uint64_t**)` to the `item_size` member. After that, the application can start using the refheap functions.

Every heap function is available as a refheap function. The only difference is the `ref` prefix, so that instead of `heap_xxx()` the application can use `refheap_xxx()`. Additionally, `refheap_min` and `refheap_max` are `heap_min` and `heap_max` respectively. There is no `struct refheap` structure, use `struct heap` instead.

Any item-retrieving refheap functions return a pointer to the item, not to the ref (it is stored in front of the item), so that the behavior is identical to normal heaps. If you for some reason would like to inspect the reference, subtract `sizeof(uint64_t**)` from the pointer.

Deleting items and managing references looks like the following:
```c
refheap.sign = refheap_min;

uint64_t reference;

(void) refheap_insert(&refheap, &(int[]){ 2 }, NULL);

(void) refheap_insert(&refheap, &(int[]){ 1 }, &reference);

/* 1 is the root, 2 is the left child. */

/* The reference will keep being updated even if new items are added: */
(void) refheap_insert(&refheap, &(int[]){ 0 }, NULL);

/* Deleting an item by reference is as simple as: */
refheap_delete(&refheap, reference);

/* The reference must not be deleted again. The pointer to it should not change.
A reference can be reused: */
(void) refheap_insert(&refheap, &(int[]){ 3 }, &reference);

/* Or you can "inject" references based on an abs_idx of an item, if you didn't
do that when inserting the item or if you want to update it: */
uint64_t ref;
refheap_inject(&refheap, refheap.item_size, &ref);

/* Or using shortened indexing: */
refheap_inject_rel(&refheap, 1, &ref);

/* Using the above injection functions, you can probably neglect the advise that
reference pointers should not change, and update them accordingly if needed. */
```

## threads
This module provides an easy way of managing threads and implements a thread pool.

The module creates 3 abstractions - `struct thread`, `struct threads`, and `struct thread_pool`.

### One thread

Starting off with `struct thread`, it's a very simple structure allowing control of one thread only:
```c
struct thread thread = {0};

void job(void* data) {
  // ...
}

void* data = /*...*/;

int err = thread_start(&thread, job, data);
```

There are 2 methods to stop a thread:
```c
thread_stop(&thread);

thread_stop_async(&thread);
```

And a few more to control its behavior:
```c
thread_cancellation_disable();
thread_cancellation_enable();

thread_cancellation_async();
thread_cancellation_deferred();
```

The first one is synchronous and guarantees that the thread is stopped when the function exits. There's no such promise with the second function. The `thread` structure can be reused immediatelly afterwards in both cases.

Some of functions in this module exist in both sync and async versions to suit everyone's needs. Note though that asynchronousness isn't really a key to optimisation - the time saved to quit such a function a bit earlier is mostly insignificant unless the number of threads is really big (a few hundred or thousand) or they perform heavy tasks which can't be interrupted until they are finished (because only then can such a thread be stopped). If you are in doubt, benchmark and make sure this is the bottleneck that asynchronousness will fix.

All threads have default settings when launched. Moreover, `canceltype` is `deferred` and cancellability is enabled. That means the threads can only be stopped if they contain any cancellation points. The application is free to change cancellability state and type as it wishes.

All thread-adding and removing functions for single as well as multi thread structures can be called from within the spawned threads themselves and are guaranteed to work as intended.

### Multiple threads

Moving from one thread to multiple, we need to use the `struct threads` structure:
```c
struct threads threads = {0};
```

New threads can be added and existing ones removed at any time (not thread-safe):
```c
int err = threads_add(&threads, job, data, 4); /* add 4 threads */

threads_remove(&threads, 2); /* or threads_remove_async() */

/* 2 threads are present now */
```

Note that you can't specify which threads specifically to remove. They are removed in order from the last to the first. If you need which threads to remove, use multiple `struct threads` structures.

The list of threads can be resized using:
```c
int err = threads_resize(&threads, new_size); /* new_size = any uint32_t number */
```

The function is helpful when you want to add threads and not worry about the return value of `threads_add()`. Additionally, since `threads_remove()` and `threads_remove_async()` **DO NOT** shrink the list of threads, it might happen that you will find yourself doing `threads_resize(&threads, threads.used)` in order to account for that and clean up the unused space, for instance when you add a lot of threads, remove most of them, and never add any again.

If you want to get rid of all threads instantly, you can use `threads_shutdown(&threads)` or `threads_shutdown_async(&threads)`. These functions are equivalent to `threads_remove(&threads, threads.used)` and `threads_remove_async(&threads, threads.used)` respectively.

If you want to stop using the structure, first remove all threads and then call `threads_free(&threads)`. The structure can be reused immediatelly afterwards.

Do you wish to stop AND free the structure at the same time from one of the spawned threads? Well, you can!
```c
void thread_function(void* data) {
  // ...
  thread_cancellation_disable();
  threads_shutdown_async(&threads);
  threads_free(&threads);
  thread_cancellation_enable();
  // unreachable
}
```

### Thread pool

Thread pool is an extension to the prior mentioned threading structures, both `struct thread` and `struct threads`. One thread pool structure may be used within multiple threading structures and vice versa.

Initialisation requires a function usage:
```c
struct thread_pool pool = {0};

int err = thread_pool(&pool);
```

Deinitialization requires making sure no threads access the thread pool again or that they are stopped, and then calling `thread_pool_free(&pool)`. The structure can be reused immediatelly afterwards.

Using the thread pool involves passing the prepared thread pool function and the thread pool itself as function and data to any prior mentioned thread-creating function:
```c
/* Single thread */
thread_start(&thread,  thread_pool_thread, &pool  );

/* Multi thread */
threads_add(&threads,  thread_pool_thread, &pool,  amount);

//                     ^^^^^^^^^^^^^^^^^^  ^^^^^
```

It doesn't matter if the step above is done before or after adding any jobs to the thread pool.

All other functions from now on, except locking functions, come in 2 variants - `thread_pool_xxx()` and `thread_pool_xxx_raw()`. The difference is that the former is thread-safe and the latter is not.

To account for advanced usage cases like calling multiple thread pool functions in one go, the application can lock the thread pool first using `thread_pool_lock(&pool)`, call any `thread_pool_xxx_raw()` functions, and then unlock it using `thread_pool_unlock(&pool)`:
```c
thread_pool_lock(&pool);
// thread_pool_xxx_raw(&pool, ...);
// thread_pool_yyy_raw(&pool, ...);
// thread_pool_zzz_raw(&pool, ...);
thread_pool_unlock(&pool);
```

The queue of jobs to do can be resized the same way list of threads in `struct threads` could be and it follows the same rules:
```c
int err = thread_pool_resize(&pool);
```

Jobs can be added using:
```c
int err = thread_pool_add(&pool, job, data);
```

The queue of jobs can be completely cleared using `thread_pool_clear(&pool)`.

This module also allows the application to do work on its own and not only in the threads, with the difference that threads execute jobs forever while the application doesn't have to:
```c
/* If there are any jobs, grab one and run it, otherwise wait for one. */
thread_pool_work(&pool);

/* If there are any jobs, grab one and run it, otherwise return. */
thread_pool_try_work(&pool);
```

The `thread_pool_thread` function is just basically:
```c
while(1) {
  thread_pool_work(&pool);
}
```

## time

This module provides clock access and timers. To do that, one thread must be spawned to keep track of all timers:
```c
struct time_manager manager = {0};
int err = time_manager(&manager);
/* Timers will start being processed after
the function below is finished */
err = time_manager_start(&manager);

/* Deinitialisation */
time_manager_stop/*_async*/(&manager);
time_manager_free(&manager);
/* Can be reused immediatelly after */
```

Similarly to thread pool in the `threads` module, time manager structure is thread-safe by default, but there are locking/unlocking functions and the raw equivalents of other functions to make it easy to perform multiple tasks under one mutex lock:
```c
time_manager_lock(&manager);
// time_manager_xxx_raw(&manager, ...);
// time_manager_yyy_raw(&manager, ...);
// time_manager_zzz_raw(&manager, ...);
time_manager_unlock(&manager);
```

This module can be divided into 3 sections - time manipulating routines, timeouts, and intervals.

### Time manipulating routines

There are a plenty of functions which fetch time with different precisions or which convert between precisions.

The most basic function is the one fetching the current time:
```c
uint64_t time = time_get_time();
```

The received resolution is nanoseconds. We can also fetch various time quantities ahead of the current time:
```c
/* Fetch current time and
add 100 nanoseconds to it */
time = time_get_ns(100);

/* Fetch current time and
add 100 microseconds to it */
time = time_get_us(100);

/* Fetch current time and
add 100 milliseconds to it */
time = time_get_ms(100);

/* Fetch current time and
add 100 seconds to it */
time = time_get_sec(100);
```

There are also functions to convert between precisions. For instance, if we wanted to convert seconds to nanoseconds, we could do:
```c
const uint64_t seconds = 123;
const uint64_t nanoseconds = time_sec_to_ns(seconds);
```

The entire list of converting routines:
```c
time_sec_to_ms();

time_sec_to_us();

time_sec_to_ns();


time_ms_to_sec();

time_ms_to_us();

time_ms_to_ns();


time_us_to_ns();

time_us_to_sec();

time_us_to_ms();


time_ns_to_sec();

time_ns_to_ms();

time_ns_to_us();
```

### Timeouts

Timers in this module can be divided into timeouts and intervals. Just like in JavaScript, timeouts only run once and are then disarmed, while intervals run infinitely unless they are cancelled. All timeout functions also apply to intervals, but intervals have different function arguments, which will be covered in the Intervals section below.

The application can choose to hold a reference to a timer to be able to edit or cancel it later on. It can as well choose not to do so, but then it will lose the ability to modify the timer in any way.

A very basic timeout might look like the following:
```c
void timer(void* data) {
  /* do something */
}
void* data = /* something */;

int err = time_manager_add_timeout/*_raw*/(&manager, timer, data, NULL);
//                                                timer reference ^^^^
```

Following the trend of other modules, both timeouts and intervals can be adjusted to allocate more space or clean up existing memory:
```c
/* Allowing for 5 timeouts, doesn't include any intervals */
int err = time_manager_timeouts_resize/*_raw*/(&manager, 5);

/* Allowing for 5 intervals, doesn't include any timeouts */
err = time_manager_intervals_resize/*_raw*/(&manager, 5);

/* The respective timer-adding functions
won't throw errors 5 times in a row */
```

Cancelling a timeout requires a reference to be held:
```c
struct time_reference ref = {0};

int err = time_manager_add_timeout(&manager, timer, data, &ref);

/* Change of mind */
int success = time_manager_cancel_timeout/*_raw*/(&manager, &ref);
```

The function can fail if the timeout has already been executed or cancelled.

A timer can also be opened and edited, then closed and updated appropriately by the underlying code:
```c
struct time_timeout* timeout = time_manager_open_timeout(&manager, &ref);
if(timeout == NULL) {
  /* Cancelled or executed already */
}
/* Now we hold the manager's lock - let's not block it excessively */

/* Reconfigure the timer to run in 30 seconds from now */
timeout->time = time_get_sec(30);
/* Change its callback */
timeout->func = other_timer;
/* Change the data */
timeout->data = other_data;

/* Roll things back to normal */
time_manager_close_timeout(&manager, &ref);
```

Note that if `time_manager_open_timeout()` returns `NULL`, the thread **DOES NOT** own the manager's lock. If you want the lock, rework the above code to use locks and raw functions like so:
```c
time_manager_lock(&manager);
struct time_timeout* timeout = time_manager_open_timeout_raw(&manager, &ref);
if(timeout != NULL) {
  timeout->time = time_get_sec(30);
  time_manager_close_timeout_raw(&manager, &ref);
}

/* Continue executing manager functions under the lock */

time_manager_unlock(&manager);
```

Timeouts and intervals are __not__ executed under the manager's lock, but they are executed on the only thread that the manager has, thus potentially delaying any other timers scheduled to run at the same time. With that in mind, if the application wishes to run a lot of heavy tasks, it should consider a thread pool or multiple instances of time manager. Either way is fine, since more threads are needed in such a case. Benchmark if in doubt of which one to pick and if you rely on performance.

### Intervals

Intervals work exactly the same as timeouts do and their function names are exactly the same, but with `timeout` substituted for `interval`.

The only noticable difference is how `struct time_interval` works. It looks like the following:
```c
struct time_interval {
  uint64_t base_time;
  uint64_t interval;
  uint64_t count;
  // ... other meta
};
```

It isn't any ordinary looped timeout that is never removed from the manager. `base_time` denotes the starting time of the timeout - we can assume it's 0, or `time_get_time()`. `interval` is the time that has to pass between 2 executions of an interval. `count` starts with 0, but can be adjusted by the application by opening the interval with functions mentioned before. As `count` goes up, `interval` is multiplied by it and the result is added to `base_time` to get the next execution time of the interval. This way, the interval will never drive away from its original timing scheme, even if it fails by small amounts on each execution.

Shortly speaking, the equation is:
```c
const uint64_t next_time = base_time + interval * count;
```

Every new interval starts with `count` set to `0` so that if `base_time` is the current time, the interval will be executed almost instantly after it's added. If the application wishes to avoid that, it should increase `base_time` by `interval` so that this does not happen.

Adding an interval is the only function with different arguments:
```c
int err = time_manager_add_interval/*_raw*/(&manager, base_time, interval, func, data, &ref);
```

It is exactly the same as the `time_manager_add_timeout()` functions, with the additional `interval` argument.

## net

Still experimented with.

## tcp

Still experimented with.