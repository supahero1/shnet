#include <shnet/test.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <shnet/storage.h>

#define TEST_MAGIC 's'

test_register(void*, shnet_malloc, (const size_t a), (a));
test_register(void*, shnet_realloc, (void* const a, const size_t b), (a, b));
test_register(void*, mmap, (void* a, size_t b, int c, int d, int e, off_t f), (a, b, c, d, e, f))

int main() {
  test_begin("storage check");
  test_error_check(void*, shnet_malloc, (0xbad));
  test_error_check(void*, shnet_realloc, ((void*) 0xbad, 0xbad));
  test_error_check(void*, mmap, ((void*) 0xbad, 0xbad, 0xbad, 0xbad, 0xbad, 0xbad));
  
  test_error_set_retval(shnet_malloc, NULL);
  test_error_set_errno(shnet_malloc, TEST_MAGIC);
  test_error_set_retval(shnet_realloc, NULL);
  test_error_set_errno(shnet_realloc, TEST_MAGIC);
  test_error_set_retval(mmap, MAP_FAILED);
  test_end();
  
  test_begin("storage init");
  struct data_storage storage = {0};
  assert(!data_storage_resize(&storage, 1));
  data_storage_free(&storage);
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = NULL,
    .len = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  data_storage_free(&storage);
  assert(!data_storage_resize(&storage, 1));
  test_end();
  
  test_begin("storage resize err");
  test_error(shnet_realloc);
  struct data_storage tem = storage;
  assert(data_storage_resize(&storage, 2) == -1);
  assert(memcmp(&storage, &tem, sizeof(storage)) == 0);
  assert(errno == TEST_MAGIC);
  errno = 0;
  test_end();
  
  test_begin("storage resize 0");
  assert(storage.used == 0);
  assert(storage.size == 1);
  assert(!data_storage_resize(&storage, 0));
  assert(storage.used == 0);
  assert(storage.size == 0);
  assert(storage.frames == NULL);
  assert(!data_storage_resize(&storage, 1));
  test_end();
  
  test_begin("storage add ptr err");
  tem = storage;
  char* ptr = malloc(1);
  assert(ptr);
  test_error(shnet_malloc);
  assert(data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .free_onerr = 1,
    .dont_free = 1
  })) == -1);
  assert(memcmp(&storage, &tem, sizeof(storage)) == 0);
  errno = 0;
  test_end();
  
  test_begin("storage add mmap err");
  ptr = mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(ptr != MAP_FAILED);
  tem = storage;
  test_error(shnet_malloc);
  assert(data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .mmaped = 1,
    .len = 1,
    .free_onerr = 1,
    .dont_free = 1
  })) == -1);
  assert(memcmp(&storage, &tem, sizeof(storage)) == 0);
  assert(errno == TEST_MAGIC);
  test_expect_segfault(ptr);
  errno = 0;
  test_end();
  
  test_begin("storage add file err 1");
  test_error(mmap);
  test_error_set_errno(mmap, TEST_MAGIC);
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  assert(sfd != -1);
  tem = storage;
  errno = 0;
  assert(data_storage_add(&storage, &((struct data_frame) {
    .fd = sfd,
    .file = 1,
    .len = 1,
    .free_onerr = 1,
    .dont_free = 1
  })) == -1);
  assert(memcmp(&storage, &tem, sizeof(storage)) == 0);
  assert(errno == TEST_MAGIC);
  assert(close(sfd) == -1);
  assert(errno == EBADF);
  errno = 0;
  test_end();
  
  test_begin("storage add file err 2");
  sfd = socket(AF_INET, SOCK_STREAM, 0);
  assert(sfd != -1);
  tem = storage;
  test_error(shnet_malloc);
  assert(data_storage_add(&storage, &((struct data_frame) {
    .fd = sfd,
    .file = 1,
    .len = 1,
    .free_onerr = 1,
    .dont_free = 1
  })) == -1);
  assert(memcmp(&storage, &tem, sizeof(storage)) == 0);
  errno = 0;
  assert(close(sfd) == -1);
  assert(errno == EBADF);
  errno = 0;
  test_end();
  
  test_begin("storage finish()");
  ptr = malloc(2);
  assert(ptr);
  ptr[1] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 2
  })));
  data_storage_drain(&storage, 1);
	assert(!data_storage_is_empty(&storage));
  data_storage_finish(&storage);
  assert(storage.frames->offset == 0);
  assert(storage.frames->len == 1);
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  /* No frames, but it should be aware of that. */
  data_storage_finish(&storage);
  assert(data_storage_is_empty(&storage));
  data_storage_drain(&storage, 0);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage freeable not-readonly");
  ptr = malloc(1);
  assert(ptr);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage freeable readonly");
  ptr = malloc(1);
  assert(ptr);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .read_only = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage not-freeable not-readonly");
  ptr = malloc(1);
  assert(ptr);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .dont_free = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage not-freeable readonly");
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  free(ptr);
  test_end();
  
  test_begin("storage mmapped freeable not-readonly");
  ptr = mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(ptr != MAP_FAILED);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .mmaped = 1
  })));
  test_expect_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage mmapped freeable readonly");
  ptr = mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(ptr != MAP_FAILED);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .mmaped = 1,
    .read_only = 1
  })));
  test_expect_no_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_expect_segfault(ptr);
  test_end();
  
  test_begin("storage mmapped not-freeable not-readonly");
  ptr = mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(ptr != MAP_FAILED);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .mmaped = 1,
    .dont_free = 1
  })));
  test_expect_no_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_expect_no_segfault(ptr);
  test_end();
  
  test_begin("storage mmapped not-freeable readonly");
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .mmaped = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  test_expect_no_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  data_storage_drain(&storage, 1);
  assert(data_storage_is_empty(&storage));
  test_expect_no_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(!munmap(ptr, 1));
  test_expect_segfault(ptr);
  test_end();
  
  test_begin("storage file freeable not-readonly");
  char* _dir = get_current_dir_name();
  assert(_dir);
  const size_t len = strlen(_dir);
  const size_t len2 = strlen("/../test.txt");
  char* dir = malloc(len + len2 + 1);
  assert(dir);
  memcpy(dir, _dir, len);
  memcpy(dir + len, "/../test.txt", len2 + 1);
  
  int file = open(dir, 0);
  /*
   * Make sure to execute "make test"
   * from the root directory (../../).
   */
  assert(file != -1);
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .fd = file,
    .len = 3,
    .offset = 1,
    .file = 1
  })));
  assert(storage.frames->data[1] == TEST_MAGIC);
  data_storage_drain(&storage, 2);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage file freeable readonly");
  file = open(dir, 0);
  assert(file != -1);
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .fd = file,
    .len = 3,
    .offset = 1,
    .file = 1,
    .read_only = 1
  })));
  assert(storage.frames->file);
  data_storage_drain(&storage, 2);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage file not-freeable not-readonly");
  file = open(dir, 0);
  assert(file != -1);
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .fd = file,
    .len = 3,
    .offset = 1,
    .file = 1,
    .dont_free = 1
  })));
  assert(storage.frames->data[1] == TEST_MAGIC);
  data_storage_drain(&storage, 2);
  assert(data_storage_is_empty(&storage));
  test_end();
  
  test_begin("storage file not-freeable readonly");
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .fd = file,
    .len = 3,
    .offset = 1,
    .file = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  assert(storage.frames->file);
  data_storage_drain(&storage, 2);
  assert(data_storage_is_empty(&storage));
  assert(!close(file));
  test_end();
  
  test_begin("storage free");
  data_storage_free(&storage);
  assert(data_storage_is_empty(&storage));
  assert(storage.frames == NULL);
  assert(storage.used == 0);
  assert(storage.size == 0);
  free(_dir);
  free(dir);
  test_end();
  return 0;
}
