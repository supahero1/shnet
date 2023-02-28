#include <shnet/error.h>
#include <shnet/storage.h>

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>


static void
data_frame_free_common(const struct data_frame* const frame)
{
	if(frame->mmaped)
	{
		(void) munmap(frame->data, frame->len);
	}
	else if(frame->file)
	{
		(void) close(frame->fd);
	}
	else
	{
		free(frame->data);
	}
}


void
data_frame_free(const struct data_frame* const frame)
{
	if(!frame->dont_free)
	{
		data_frame_free_common(frame);
	}
}


void
data_frame_free_err(const struct data_frame* const frame)
{
	if(frame->free_onerr)
	{
		data_frame_free_common(frame);
	}
}


void
data_storage_free(struct data_storage* const storage)
{
	if(storage->frames != NULL)
	{
		for(uint32_t i = 0; i < storage->used; ++i)
		{
			data_frame_free(&storage->frames[i]);
		}

		free(storage->frames);

		storage->frames = NULL;
	}

	storage->bytes = 0;
	storage->used = 0;
	storage->size = 0;
}


static int
data_storage_resize(struct data_storage* const storage, const uint32_t new_len)
{
	if(new_len == storage->size)
	{
		return 0;
	}

	if(new_len == 0)
	{
		data_storage_free(storage);

		return 0;
	}

	void* const ptr =
		shnet_realloc(storage->frames, sizeof(*storage->frames) * new_len);

	if(ptr == NULL)
	{
		return -1;
	}

	storage->frames = ptr;
	storage->size = new_len;

	return 0;
}


int
data_storage_add(struct data_storage* const storage,
	const struct data_frame* const frame)
{
	if(frame->offset == frame->len)
	{
		return 0;
	}

	const uint32_t new_size = storage->used + 1;

	if(
		new_size > storage->size &&
		data_storage_resize(storage, (new_size << 1) | 1) &&
		data_storage_resize(storage, new_size)
	)
	{
		return -1;
	}

	if(!frame->read_only)
	{
		if(!frame->file)
		{
			const uint64_t len = frame->len - frame->offset;

			void* const data_ptr = shnet_malloc(len);

			if(data_ptr == NULL)
			{
				goto err;
			}

			(void) memcpy(data_ptr, frame->data + frame->offset, len);

			storage->frames[storage->used++] =
			(struct data_frame)
			{
				.data = data_ptr,
				.len = len
			};
			storage->bytes += len;
		}
		else
		{
			void* data_ptr;

			safe_execute(
				data_ptr = mmap(NULL, frame->len, PROT_READ,
					MAP_PRIVATE | MAP_POPULATE, frame->fd, 0),
				data_ptr == MAP_FAILED,
				errno
			);

			if(data_ptr == MAP_FAILED)
			{
				goto err;
			}

			(void) madvise(data_ptr, frame->len, MADV_SEQUENTIAL);

			if(data_storage_add(storage, &(
			(struct data_frame)
			{
				.data = data_ptr,
				.len = frame->len,
				.offset = frame->offset,
				.free_onerr = 1,
				.mmaped = 1
			}
			))) {
				goto err;
			}
		}

		data_frame_free(frame);
	}
	else
	{
		storage->frames[storage->used++] = *frame;
		storage->bytes += frame->len - frame->offset;
	}

	return 0;

	err:

	data_frame_free_err(frame);

	return -1;
}


void
data_storage_drain(struct data_storage* const storage, const uint64_t amount)
{
	if(storage->used == 0)
	{
		assert(amount == 0);

		return;
	}

	struct data_frame* const frame = storage->frames;

	frame->offset += amount;
	storage->bytes -= amount;

	if(frame->offset == frame->len)
	{
		data_frame_free(frame);

		--storage->used;

		(void) memmove(frame, frame + 1,
			sizeof(*frame) * storage->used);

		if(storage->used < (storage->size >> 2))
		{
			(void) data_storage_resize(storage, storage->used << 1);
		}
	}
}


void
data_storage_finish(const struct data_storage* const storage)
{
	struct data_frame* const frame = storage->frames;

	if(storage->used == 0 || frame->read_only || frame->offset == 0)
	{
		return;
	}

	frame->len -= frame->offset;

	(void) memmove(frame->data, frame->data + frame->offset, frame->len);

	frame->offset = 0;

	char* const ptr = shnet_realloc(frame->data, frame->len);

	if(ptr != NULL)
	{
		frame->data = ptr;
	}
}


int
data_storage_is_empty(const struct data_storage* const storage)
{
  	return storage->used == 0;
}
