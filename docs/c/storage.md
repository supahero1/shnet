# The storage facility

This module aims to provide a system for managing data with various properties:
- Read-only non-freeable data
- Read-only freeable data
- Writable data

Each of these have different characteristics that allow them to be handled in different ways, sometimes optimising their usage:

- Writable data is not reliable - it needs to be copied to other read-only region to guarantee integrity
- Read-only data can be operated on using just a pointer - no second storage needs to be created, no increased memory usage
- Freeable data will be disposed of automatically after all of its contents are read

All of these are taken care of in this module using one structure, instead of creating 3 distinct data handling modules.

The module supports `malloc`, `mmap`, and all file descriptor types supporting `mmap`-like operations (like files or pipes).

## Usage

```c
/* Initialisation */
struct data_storage storage = {0};

/* Freeing */
data_storage_free(&storage);
```

A storage consists of *frames* which carry information about given memory region:

```c
struct data_frame {
  union {
    char* data;
    int fd;
  };
  uint64_t len:61;
  uint64_t read_only:1;
  uint64_t dont_free:1;
  uint64_t free_onerr:1;
  uint64_t offset:61;
  uint64_t mmaped:1;
  uint64_t file:1;
};
```

Maximum data length and offset are `2^61 - 1`, as seen above. `mmaped` must be `1` if `data` is a pointer returned by `mmap()` or other equivalent function. `file` must be `1` if `fd` is used instead of `data`.

By default, if a new frame can't be added to a storage (an error occured), it will not be modified in any way by the underlying code. If you wish to free the frame (i.e. the data that comes with it), set `free_onerr` to `1`. Allocated memory will be `free()`'d, `mmap()`'ed will be `munmap()`'ed, file descriptors will be `close()`'d.

`read_only` should be set to `1` if you are sure the data won't be modified after adding it to a storage. This makes the underlying code make optimised decisions about how to handle the frame.

`dont_free` must be set to `1` if you don't want the data to be freed after it is fully read.

You can insert a frame like this:

```c
int err = data_storage_add(&storage, &((struct data_frame) {
  .data = some_pointer,
  .len = how_many_bytes_in_the_pointer
  /* ... other properties ... */
}));

if(err) {
  /* No memory. some_pointer was
  freed if free_onerr was 1 */
}
```

If you want to use the `offset` property, you **SHOULD NOT** decrease the `len`. For instance, assuming you have 4096 bytes of memory, and you read 256 so that your offset is 256, you **SHOULD** do the following:

```c
err = data_storage_add(&storage, &((struct data_frame) {
  .data = some_pointer,
  .len = 4096,
  .offset = 256
}));
```

Playing clever by setting `offset` to `0` and `len` to `4096 - 256` **MIGHT NOT** work and might end up with undefined behavior in case of `mmap()`ed data. The bytes below `offset` will never really be touched by the underlying code, the real length is what matters. Only set `offset` to `0` if you know what you are doing.

Data can be used like so:

```c
while(1) {
  if(!storage.frames->file) {
    uint64_t used = use_some_memory(
      storage.frames->data + storage.frames->offset,
      storage.frames->len - storage.frames->offset
    );
    if(!used || data_storage_drain(&storage, used)) {
      break;
    }
  } else {
    /* ... consider using sendfile() ... */
  }
}
data_storage_finish(&storage);
```

In the above example, `data_storage_drain()` marks `used` bytes as "read" (increases the frame's `offset`, eventually freeing and deleting it from the storage if all bytes were read). The bytes to consume **MUST NOT** be greater than `storage.frames->len - storage.frames->offset` (that is, available bytes to read from the first frame in the storage).

The function returns `1` if the storage has no more frames (and so the code breaks out of the loop in the example), or `0` otherwise.

It is legal to try to drain `0` bytes if there are no frames. The return value will then be set to `1`.

Another function used in the above example is `data_storage_finish()`. Its job is to try to optimise the first frame in the queue. Under certain conditions, it will decrease size of the frame's allocated memory segment, at the cost of using `realloc()`. The data segment will never be any pointer passed down by you via `data_storage_add()` - it will be a completely new block of memory allocated by the underlying code to store not-readonly frames. No memory that you pass to the storage is ever modified by the underlying code - only memory that it itself allocated to work properly.

`data_storage_finish()` should not be used after every drain, because it is possible in the next loop the whole frame will be consumed, effectively negating any work done by the function. Use it only at the end of a routine to try to optimise the final state of a storage.

It is also legal to call the function when there are no frames in the storage. It will simply do nothing then.

---

If not using `data_storage_drain()`, you can check if a storage is empty with:

```c
int bool = data_storage_is_empty(&storage);
```

The above function will yield `1` if the provided storage is empty, or `0` otherwise.

Additionally, the list of frames can be resized to fit the application's needs:

```c
uint32_t new_size = storage.used;
int no_mem = data_storage_resize(&storage, new_size);
```

In the above example, the storage's list of frames is resized to its real size. This might prove important to you, because when frames are removed (using `data_storage_drain()`), the list of frames isn't `realloc()`'ed to the new smaller size. Thus, the above function can be used to optimise your code - it either makes space for a lot of new frames, or cleans up after a lot of frames. You can access the fake size of the list of frames (fake, because not necessarily are all of the frames used) using `storage.size`.