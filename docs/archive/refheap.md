Refheaps are heaps, but with one additional very important
ability - to remove any items from the heap, not only the root.

The goal was to create something much lighter than a binary tree, while still
having its characteristic of fast deletion. Binary trees are too much for this
job, because they also allow for quick searches, which is not needed. They also
take significantly more memory to keep track of children.

Thus, this module has been born. A refheap is a normal heap, but with
an additional pointer bound to every inserted item that points to an
application-defined variable that will be updated with the position of
the item in the tree. That application-defined variable might not exist,
meaning the item is not to be tracked, and then the internal pointer is
simply NULL.

Initialisation is very similar to heaps, but with one important difference:

```c
struct heap refheap = {0};

// ... the same

refheap.item_size = sizeof(int) + sizeof(uint64_t**); /* <---- */
```

The only requirement for using refheaps is adding `sizeof(uint64_t**)` to the
`item_size` member. After that, the application can start using the refheap
functions.

Every heap function is available as a refheap function. The only difference
is the `ref` prefix, so that instead of `heap_xxx()` the application can use
`refheap_xxx()`. Additionally, `refheap_min` and `refheap_max` are `heap_min`
and `heap_max` respectively. There is no `struct refheap` structure, use
`struct heap` instead.

Any item-retrieving refheap functions return a pointer to the item, not to the
ref (it is stored in front of the item), so that the behavior is identical to
normal heaps. If you for some reason would like to inspect the reference,
subtract `sizeof(uint64_t**)` from the pointer.

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
