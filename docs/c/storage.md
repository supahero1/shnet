# The storage facility

This module aims to provide a system for
managing data with various properties:

- Read-only non-freeable data
- Read-only freeable data
- Writable data

Each of these have different characteristics that allow them to
be handled in different ways, sometimes optimising their usage:

- Writable data is not reliable - it needs to be
  copied to other read-only region to guarantee integrity,
- Read-only data can be operated on using just a pointer - no
  second storage needs to be created, no increased memory usage,
- Freeable data will be disposed of automatically
  after all of its contents are read.

There are also various data types one can operate on:

- `malloc()` pointer,
- `mmap()` pointer,
- A file descriptor supporting `mmap()`-like operations.

All of these are taken care of in this module using one structure.

## Dependencies

None.

## Dev dependencies

- [`error.md`](./error.md)

## Usage

```c
/* Initialisation */
struct data_storage storage = {0};

/* Freeing */
data_storage_free(&storage);
```

A storage consists of *frames* which carry
information about given memory region:

```c
struct data_frame
{
	union
	{
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
	uint64_t _unused:1;
};
```

Maximum data length and offset are `2^61 - 1`, as seen above. `mmaped` must be
`1` if `data` is a pointer returned by `mmap()` or other equivalent function.
`file` must be `1` if `fd` is used instead of `data`.

By default, if a new frame can't be added to a storage (an error occured), it
will not be modified in any way by the underlying code. If you wish to free the
frame (i.e. the data that comes with it), set `free_onerr` to `1`. Allocated
memory will be `free()`'d, `mmap()`'ed will be `munmap()`'ed, file descriptors
will be `close()`'d. This will also be mentioned below.

`read_only` should be set to `1` if you are sure the data won't be modified
after adding it to a storage. This makes the underlying code make optimised
decisions about how to handle the frame.

Read-only frames are not touched by the code internally. None of the memory they
point to is modified in any way, nor is it copied, and no additional memory is
allocated, besides a new spot for the frame in the array of frames in the
`struct data_storage` structure.

However, if a frame is not marked as `read_only`, the underlying code will do
whatever it can to copy contents of the frame somewhere else, where they can be
read-only. File descriptors will be `mmap()`'ed, allocated memory regions will
have new memory region allocated for them to be copied to. The original frame
will be freed, unless marked with `dont_free` (see below), and the
newly-allocated frame will also be freed upon complete usage.

Since `mmap()` does not guarantee data integrity when viewing a not-read-only
file, frames of such files are copied to yet another allocated memory region
after being `mmap()`'ed, and then the `mmap()`'ed region is `munmap()`'ed.
Since this is a pretty expensive process, you might want to consider creating
a read-only cache of files (loading them completely to memory) before using.
Read-only frames achieve the highest efficiency possible.

`dont_free` must be set to `1` if you don't want the data to be destroyed
after it is fully read. This also means `data_storage_free()` will not
touch it (but the array of frames will be `free()`'d overall).

You can insert a frame like this:

```c
int err = data_storage_add(&storage, &(
(struct data_frame)
{
	.data = some_pointer,
	.len = how_many_bytes_in_the_pointer,
	.free_onerr = 0
	/* ... other properties ... */
}
));

if(err)
{
	/* Likely no memory. 'some_pointer' would
	be freed if 'free_onerr' was 1. */
}
```

If you want to use the `offset` property, you **MUST NOT** decrease the
`len`. For instance, assuming you have 4096 bytes of memory, and you
read 256 so that your offset is 256, you **MUST** do the following:

```c
err = data_storage_add(&storage, &(
(struct data_frame)
{
	.data = some_pointer,
	.len = 4096,
	.offset = 256
}
));
```

Playing clever by setting `offset` to `0`, `len` to `4096 - 256`
and `data` to `some_pointer + 256` **WILL NOT** work.

`offset` may never be greater than `len`.

Data can be used like so:

```c
do
{
	uint64_t used;

	if(!storage.frames->file)
	{
		used = send_some_memory(to,
			storage.frames->data + storage.frames->offset,
			storage.frames->len - storage.frames->offset
		);
	}
	else
	{
		used = sendfile_some_memory(to,
			storage.frames->fd,
			storage.frames->offset,
			storage.frames->len
		);
	}

	data_storage_drain(&storage, used);
}
while(!data_storage_is_empty(&storage)/* && used */);

data_storage_finish(&storage);
```

In the above example, `data_storage_drain()` marks `used` bytes as "read"
(increases the frame's `offset`), eventually freeing and deleting it from the
storage if all bytes were read. The bytes to consume **MUST NOT** be greater
than `storage.frames->len - storage.frames->offset` (that is, available bytes
to read from the first frame in the storage). The function may be called if
there are currently no frames in the storage, but only if `usage` is `0`.

The `data_storage_is_empty()` function returns `1` if the storage has no more
frames (and so the code breaks out of the loop in the example), or `0`
otherwise.

Another function used in the above example is `data_storage_finish()`.
Its job is to decrease memory footprint of the first frame in the queue.

If a frame was not marked as read-only at the time of passing it to
`data_storage_add()`, a new memory region will be allocated for it to make it
read-only, as described above. When `data_storage_drain()` uses up some bytes of
the frame, and since it's a property of the underlying code and nothing passed
down by the user, the point of `data_storage_finish()` is to use `realloc()` to
shrink down that memory region of the frame, effectively lowering memory usage.

`data_storage_finish()` should not be used after every drain, because it
is possible in the next loop the whole frame will be consumed, effectively
negating any work done by the function. Use it only at the end of a routine
to try to optimise the final state of the storage.

It is also legal to call the function when there are no
frames in the storage. It will simply do nothing then.

The function should probably only be used when the frames are really big in
size, so that they don't completely clobber up the system. An example would
be static file hosting, where the files could potentially be hundreds of MB
in size. In that scenario, getting rid of unused memory would be mandatory.

Assume in the above example the `&& used` code was not commented. In this
case, if no bytes are consumed, but there are still pending frames, the
usage of `data_storage_finish()` can decrease memory usage of the first
frame. (Otherwise, after breaking out of the loop, the storage would
always be empty, so the function wouldn't do anything.)

You can access `storage.bytes` to get the total number of bytes
available in the storage. On every drain, this number will be
lowered, and on every insertion it will be increased. If it's
`0`, that's an equivalent of saying there are no frames.

You can free a frame by yourself using `data_frame_free(&frame)` and
`data_frame_free_err(&frame)`. They will only free the given frame if
`dont_free` or `free_onerr` were set to `0` and `1` respectively.
