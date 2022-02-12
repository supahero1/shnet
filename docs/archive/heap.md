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