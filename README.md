**Shnet** is an asynchronous networking library created in "modules", each of which can be used by the application independently.

The `avl` module implements a high-performance AVL tree that can be used to store any data of any size in a node.

The `heap` module implements a high-performance heap that is universal as the AVL tree as well, but it costs it about twice the speed. In other words, if the application wants a specialized heap, for instance to store 8 byte values in a node, and it really cares about performance, it will likely be better off copying and pasting the heap's code into new files and rewriting it slightly.

The `refheap` module implements a universal heap as well, but it allows the application to keep track of where the nodes are using a reference (`ref` in the name). It is mainly used in the `time` module.

The `threads` module implements a synchronous thread poll, meaning starting and stopping threads in the poll will be done synchronously - the respective functions will only return after the requested operation on the thread poll has been done. That is to reduce the callback madness. The thread poll has no job queue.

The `time` module implements fast user space timers without an event loop and file descriptors. It supports one-shot timers with nanosecond time resolution and intervals, which can be executed right away or after their interval time has passed. Additionally, timers can be cancelled by the application if it wishes to do so - it just needs to allocate one pointer per timer and keep it until it wishes to cancel the timer. When a timer expires, a callback function with data the application provided is executed. The module also features numerous time-converting and time-fetching functions.

The `contmem` module implements continuous memory for the `avl` module (because it is not recommended to change location of nodes). It is basically a linked-list of allocated memory regions.

The `net` module implements very basic operations on sockets - without managing them in any way. Such operations include for instance setting an address, port, family, etc., or getting the aforementioned things. There are mostly 3 functions for each of these operations that operate on 3 different structures. The module also implements an event loop. This event loop and the other networking modules have limitations that the application must be aware of in order to not fall victim to undefined behavior.

The `udp` and `udplite` modules implement very basic functions to operate on `udp` and `udplite`. It is not comparable in any way to the `tcp` module, since I don't need to use the UDP protocol. Maybe the module will be expanded in the future.

The `tcp` module implements non-blocking `tcp` clients and servers using the event loop and other various functions from the `net` module. The clients only have a lock for managing the buffered data to be sent - other than that, the limitations that the library has allow for high-performance sockets with minimal lock contention. Performance of the sockets does not drop by much when their amount is increased by thousands - it only drops slightly, because of the event loop needing to handle many more sockets.

The `tls` module is built using OpenSSL and runs on top of the `tcp` module using callbacks that the `tcp` module provides. TLS clients have higher lock contention than TCP ones due to 2 more locks that are required to function. TLS functions are almost an exact copy of TCP ones to make it as intuitive to use as possible.

The `compress` module implements DEFLATE and Brotli compression & decompression.

The `http_p` module implements high customizable HTTP 1.1 request and response parsing & creation, with support for chunked transfer and DEFLATE & Brotli content encoding. The parsers is not be RFC compliant. The application is able to specify what to parse, when to stop parsing and return the result to the application, and more. The parsers after returning to the application can be resumed to parse the rest, possibly with modified parsing settings. Upon parsing continuation, the parsers is not start from the beginning again, but rather resume right where they stopped.

The `http` module implements http and https 1.1 client and server. The application can simply use `http(url, NULL)` and `http_server(url, NULL)` calls to make a client and a server respectively, without setting up epoll, time manager, or other necessary ingredients. The application is able to specify a lot of options though, using the second argument to these functions. The server is using a hash table to store and lookup requested resources in a matter of nanoseconds. Support for keep-alive connections, compression, chunked transfer, custom application headers, custom reason phrases and status codes, custom request methods, and more.

In the nearest future, the `http` module will also feature the availability of switching to the websocket protocol. Making websockets a standalone module would make it very difficult to code.

# Requirements

* [Linux](https://www.kernel.org/)
* [GCC](https://gcc.gnu.org/) or [Clang](https://clang.llvm.org/)
* [Zlib](https://www.zlib.net/)
* [Brotli](https://brotli.org/)

My personal preference is on GCC, added clang because it tries to be compatible with GCC, and so the code can still be compiled with it if anyone wants.

To install the dependencies, one can do (debian):
```bash
sudo apt update -y
sudo apt install libgcc-11-dev zlib1g-dev libbrotli-dev -y
```

..., or (fedora):
```bash
yum groupinstall "Development Tools" -y
yum install openssl-devel zlib-devel brotli-devel -y
```

# Building

```bash
git clone -b main --single-branch https://github.com/supahero1/shnet
cd shnet
```

The library is available in static and dynamic releases. Additionally, if the user lacks sudo perms, a stripped install that doesn't copy the header files into `include` can be chosen, but then the files must be included manually in one's project.

```bash
sudo make dynamic
sudo make strip-dynamic
sudo make static
sudo make strip-static
```

If the dynamic installation yields linking errors when compiling tests or application code, try the static one.

To build without installing:
```bash
sudo make build
```

If full build was chosen, the user can then test the library:
```bash
sudo make test
```

Note though that some tests require big amount of file descriptors available. To make sure TCP and TLS tests work, do:
```bash
ulimit -n 20000
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

Multiple make commands can be chained very simply. The following command performs a full dynamic reinstall and tests at the end:
```bash
sudo make clean uninstall dynamic test
```

# Code of conduct

1. Before using any library structure, such as callbacks or settings, be sure to zero it, unless stated otherwise.
2. Some structures can have their location changed, some can't. Pay close attention to that. If it's not specified, assume you can change the location.
3. If a return value is -1, check errno for details.

# Documentation

Table of contents

* [AVL tree](#avl)
* [Heap](#heap)
* [Refheap](#refheap)
* [Threads](#threads)
* [Time](#time)
* [Contmem](#contmem)

# AVL

Initialising an AVL tree:

```c
struct avl_tree avl_tree = {0};

/* Size of user data a node will carry. For
the examples, assume item_size is 4. */
avl_tree.item_size = (uint32_t);

/* Compares first node with the second one.
If the result is > 0, the second node will
be chosen (the right one).
If the result is < 0, the first node will
be chosen (the left one).
Return value of 0 means equality. Behavior
depends on which function is being executed.
When inserting without checking for copies,
the left node will be chosen. */
avl_tree.compare = (int (*)(const void*, const void*));

/* Only used during insertion. Asks the application
to provide memory of size:
sizeof(struct avl_node) + avl_tree.item_size
If the application returns NULL, insertion will fail. */
avl_tree.new_node = (void* (*new_node)(struct avl_tree*));
```

AVL nodes work based on pointers, so naturally, changing their location by for instance using `realloc()` on an array of AVL nodes is invalid. It is possible to swap location of 1 node though. That requires the application to then swap pointers of nodes that point to the old location of the node. Sample code doing that is attached in the **deleting an item** section below.

Insertion:

```c
int item = rand();

/* avl_insert can operate in 2 modes:
1. Allow copies. In other words, don't return
  an error if an item with the same value is
  found. A return value of 0 from
  avl_tree.compare will choose the left node.
2. Disallow copies. It comes at a small
  performance penalty for checking if return
  value of avl_tree.compare is 0.
*/
int err = avl_insert(&avl_tree, &item, avl_allow_copies);
/* The return value -1 means an error has occured
and the value was no inserted. It might be for 2
reasons:
1. avl_disallow_copies mode was specified and
  an item with the same value was found, or
2. avl_tree.new_node returned NULL.
*/
if(err == -1) {
  // ...
}
```

Searching for an item:

```c
/* avl_search searches for the given item_value: */
int item_value = item;
void* found_item = avl_search(&avl_tree, &item_value);
if(found_item == NULL) {
  /* Not found */
}
int found_value = *((int*) found_item);
/* found_value == item */

/* If the application wants to gain access
to the node that holds the item's value, it
can do the following: */
struct avl_node* node = (struct avl_node*) found_item - 1;
```

Deleting an item:

```c
/* Given a pointer to a node in the tree,
one can do: */
void* deleted_node = avl_delete_node(&avl_tree, node);
/* The returned pointer will be an unused node
that the application once returned from
avl_tree.new_node, and which now can be safely
cleaned up, for instance by freeing it. */

/* If the application only has an item value: */
void* deleted_node_2 = avl_delete(&avl_tree, &item_value);

/* Or, if the application isn't sure that the
value exists at all: */
void* deleted_node_3 = avl_try_delete(&avl_tree, &item_value);
/* In this case, NULL will be returned if the
given item_value could not have been found. */

/* If the application is using contmem or any
other form of continuous memory that doesn't
change it's location, a quick way of getting
rid of unused nodes so that there are no gaps
is to shift the last used node to the unused
location. That requires changing pointers of
that node to other nodes and vice versa though.
It can be done with the following code: */
struct tree_node mem[5000];
unsigned used = 0;

void* newnode(struct avl_tree* tree) {
  return &mem[++used - 1];
}
void delnode(struct avl_node* node) {
  /* Less used ones now */
  --used;
  /* If the deleted node was the last node,
  we don't need to shift anything */
  if((uintptr_t)(mem + used) != (uintptr_t) node) {
    /* Copy the whole contents of the last
    node to the unused node */
    (void) memcpy(node, mem + used, sizeof(struct tree_node));
    /* Swap parent pointers */
    if(node->parent != NULL) {
      if((uintptr_t) node->parent->right == (uintptr_t)(mem + used)) {
        node->parent->right = node;
      } else {
        node->parent->left = node;
      }
    } else {
      /* If no parent, we are the root */
      tree.head = node;
    }
    /* Swap children pointers */
    if(node->left != NULL) {
      node->left->parent = node;
    }
    if(node->right != NULL) {
      node->right->parent = node;
    }
  }
}
```

Misc functions:

```c
/* Getting minimum and maximum values: */
void* minimum_item = avl_min(&avl_tree);
void* maximum_item = avl_max(&avl_tree);
```

# Heap

Initialising a heap:

```c
struct heap heap = {0};

/* Sign of the heap. If min is chosen, at the
top of the heap will be the smallest value in
it. Max is the reverse. */
heap.sign = (heap_min || heap_max);

/* A function to compare 2 nodes. If the first
item is greater than the second one, return > 0.
If the second is greater than the first one,
return < 0. Otherwise, return 0. */
heap.compare = (int (*)(const void*, const void*));

/* Size of a node */
heap.item_size = (uint32_t);

/* Needed for it to work propertly */
heap.size = heap.item_size; // uint64_t
heap.used = heap.item_size; // uint64_t
```

The created heap will manage an array (`heap.array`) internally that it will keep items in. By default, when inserting a new item, and if `heap.used == heap.size`, it will resize the array by 1 item. If the application wishes to manage the array on its own, it may use the later mentioned `heap_resize()` function.
The heap's array will not be shrunk when popping an item. Instead, the `heap.used` value will be decremented by `heap.item_size`.

Insertion:
```c
int err = heap_insert(&heap);
/* An error (return value -1) can only occur
if there was no space for a new node and
heap_resize() returned ENOMEM */
if(err == -1) {
  /* maybe free memory and retry */
}

/* This makes space for 10 more items. With
that in mind, the application can ignore the
return value of heap_insert for 10 insertions. */
err = heap_resize(&heap, heap.used + heap.item_size * 10);
if(err == -1) {
  /* out of memory */
}
```

Heap manipulation:
```c
/* Remove and return the first item in the
heap */
heap_pop(&heap);

/* The removed item can be accessed like so: */
void* removed_item = heap.array;
/* It is copied to the first place in the
array of nodes. This place is not always
unused though - it is crucial when doing
operations on a heap. With that in mind,
if the application wants to keep the item
for a longer while and plans to insert
or pop more items, it should allocate
space for the item and memcpy it.
The first unused item is not useful when
creating a specialised heap, as any temp
values can be stored in the code, since
their size is known. */

/* One can explicitly manipulate a node
in a heap, knowing it's index: */
heap_down(&heap, (uint64_t));
heap_up(&heap, (uint64_t));
/* heap_down will check if the node can
be brought down in the heap, and will do
so if it's possible. heap_up does the
opposite. */

/* heap_peak returns the node under the
given index */
void* peaked_item = heap_peak(&heap, (uint64_t));

/* heap_is_empty checks if there are no
used nodes in the heap. heap.array may
still be initialised to some memory,
because of the first unused item. */
int is_empty = heap_is_empty(&heap);

/* heap_free deinitialises a heap; it
frees it's array of nodes and sets
heap.used and heap.size to heap.item_size
so that the heap can be used again at
any time without initialising it again. */
heap_free(&heap);
```

# Refheap

Refheap is basically a heap with one pointer of reserved data. That pointer points to an application pointer that will be updated to point at the node the application wants to keep track of. That means the application can call `heap_down`, `heap_up`, and `heap_peak` with an index that can be fetched using `refheap_ref_to_idx()`.

Initialising a refheap only differs by adding `sizeof(void**)` to `item_size`:

```c
heap.item_size = (uint32_t) + sizeof(void**);
/* Update heap.used and heap.size accordingly */
```

All heap functions to be used for refheap have the `ref` prefix, so that `heap_xxx` becomes `refheap_xxx`.

Refheap insertion:

```c
void* reference;
int err = refheap_insert(&heap, &item, &reference);
```

Additional refheap functions:

```c
/* Convert a reference to an index */
uint64_t idx = refheap_ref_to_idx(&heap, reference);

/* Delete a node by it's reference */
refheap_delete(&heap, reference);
```

# Threads

Initialisation:

```c
struct threads tp = {0};

int err = threads(&tp);
if(err == -1) {
  /* Any of sem_init() errors */
}

/* Function and data supplied to
that function, called when a new
thread is spawned. */
tp.func = (void (*)(void*));
tp.data = (void*);
```

The application can add and remove any amount of threads it likes, but it can't do it simultaneously. The operations are guaranteed to be synchronous and can be done even from the spawned threads. Removing of threads can't be manipulated to delete any given threads - they will be deleted in the order they were created. if a thread calls to remove threads and it is amongst of them, it will first stop all the other threads and at the end it will `pthread_cancel` itself.

Adding and removing threads:

```c
/* Resize an array of thread IDs. This is
rather very marginal comparing to size of
a thread's stack and resources a thread
requires, but is still there. */
int err = threads_resize(&tp, (uint32_t));
if(err == -1) {
  /* out of memory */
}

err = threads_add(&tp, (uint32_t));
if(err == -1) {
  /* out of memory, or any pthread_create()
  error, or any pthread_barrier_init() error */
}

/* Removes threads, no errors */
threads_remove(&tp, (uint32_t));

/* Removes all threads */
threads_shutdown(&tp);

/* Frees the thread poll. To use it
again, the application must zero it
and call thread_init() again. Make
sure to first call threads_shutdown(). */
threads_free(&tp);
```

# Time

Initialisation:

```c
struct time_manager manager = {0};

int err = time_manager(&manager);
if(err == -1) {
  /* Any error of sem_init(),
  pthread_mutex_init(), or threads(). */
}

/* When initialised, added timers will
still not be executed. To start the
thread that will take care of timers,
the application must also do: */
err = time_manager_start(&manager);
if(err == -1) {
  /* threads_add() error */
}

/* To stop the timers' thread: */
time_manager_stop(&manager);

/* To free the structure: */
time_manager_free(&manager);
/* To use it again, it needs to be
zeroed and initialisation must be
repeated. */
```

Adding and canceling timers:

```c
struct time_timeout timeout;
timeout.time = time_get_sec(1); /* 1 second from now */
timeout.func = (void (*func)(void*));
timeout.data = (void*);

/* To add a timer, one does not need
the time_timeout structure, as seen
below. It was only to make this example
prettier. */

/* To be able to cancel the timer later
on, a pointer must be passed that the
underlying code will update to reflect
current position of the timer. Canceling
a timer is thread-safe. It can be done
after the timer has been already executed
or cancelled. */
struct time_timeout_ref timeout_ref = {0};

/* The application can resize the available
space for timeouts and intervals once it
knows how to do so for refheaps. Look above
for refheap's documentation.
The application can then do:
refheap_resize(&manager->timeouts, ...);
refheap_resize(&manager->intervals, ...); */
err = time_manager_add_timeout(&manager, time, func, data, &timeout_ref);
if(err == -1) {
  /* out of memory */
}

/* The timeout can be canceled at any time
with the use of the reference. 1 is returned
if the timer was successfully cancelled before
execution, 0 otherwise. Note that 0 will be
returned if the timer was already cancelled.
Currently, there is no method of distinguishing. */
int cancelled = time_manager_cancel_timeout(&manager, &timeout_ref);

/* Intervals are the same, except one more
value to supply to the insertion call: */
err = time_manager_add_interval(..., (time_instant || time_not_instant));
/* It controls how the interval will behave.
If the application wants the interval to start
it's execution instantly, it should pick time_instant.
Otherwise, to wait the interval's time interval,
time_not_instant should be supplied.
Intervals will never drive too far off their
scheduled execution time, since their time is
counted with: base_time + count * interval_time */
```

Fetching current time and converting it:

```c
/* time_x_to_y: convert x to y
For example: time_sec_to_ms(1) will
yield 1000, because 1000 milliseconds
are 1 second. time_ms_to_sec(1) will
yield 0. */
uint64_t time = time_sec_to_ns(uint64_t);

time_sec_to_us(uint64_t);

time_sec_to_ms(uint64_t);

time_ms_to_ns(uint64_t);

time_ms_to_us(uint64_t);

time_us_to_ns(uint64_t);

time_ns_to_sec(uint64_t);

time_us_to_sec(uint64_t);

time_ms_to_sec(uint64_t);

time_ns_to_ms(uint64_t);

time_us_to_ms(uint64_t);

time_ns_to_us(uint64_t);

/* time_get_x(y): fetch current time and
then add time_x_to_ns(y) to it. Resulting
time will always be in nanoseconds.
For example, time_get_sec(1) will fetch
current time in nanoseconds and then add
1 second in nanoseconds to it. */

time_get_ns(uint64_t);

time_get_us(uint64_t);

time_get_ms(uint64_t);

time_get_sec(uint64_t);

/* time_get_time() only fetches current
time, it doesn't add anything to it. */

time_get_time();
```

# Contmem

Initialisation:

```c
struct contmem mem = {0};

const uint64_t items_in_a_block = 64;
const uint64_t item_size = 8;
/* Tolerance controls how blocks are shrunk.
If a block is completely free and there are
above tolerance free bytes in the last used
block, the last block will be freed. A value
of 0 means to always free an unused block. */
const uint64_t tolerance = 0;

/* A call to contmem() initialises one block.
It does not wait for items to be inserted. */
int err = contmem(&mem, items_in_a_block, item_size, tolerance);
if(err == -1) {
  /* out of memory */
}

/* Frees all blocks. To be reused, contmem()
must be called. Zeroing the structure is not
necessary. */
contmem_free(&mem);
```

Requesting and returning memory:

```c
/* Requests mem.item_size of memory */
void* memory = contmem_get(&mem);
if(memory == NULL) {
  /* out of memory */
}

/* Returns the memory. If the memory was the
last available, nothing is done and 0 is returned.
Otherwise, the last available item is copied to
the place of the removed item to remove the gap.
That also means the only way of returning memory
is by somehow keeping track of items.
This is a perfect case scenario for the AVL tree,
because items know each other' pointers and when
a node is removed, pointers to the last item are
fixed so that they are still valid. Access time
is then O(1). */
int was_last = contmem_pop(memory);

/* If the application requires accessing the
last item that was moved, for instance to let
it know about the switch of it's location, it
must do it the following way: */
if(contmem_last(contmem) != mem) {
  (void) memcpy(mem, contmem_last(contmem), contmem->item_size);
  /* Access the last item here */
}
/* And call this to finish the work. */
contmem_pop_cleanup(&mem);
/* Otherwise, if the above is not done, the last
block might have been free()'d by the underlying
code during the call to contmem_pop(), causing
undefined behavior by accessing the last item. */

/* Returns pointer to the last used item. */
void* last = contmem_last(&mem);
```

# The rest

Docs for the rest of modules will probably be created in the future.