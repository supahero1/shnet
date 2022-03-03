#include <shnet/tests.h>

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

#include <shnet/storage.h>

#define TEST_MAGIC 's'

int main() {
  begin_test("storage init");
  struct data_storage storage = {0};
  assert(!data_storage_resize(&storage, 1));
  end_test();
  
  begin_test("storage finish()");
  char* ptr = malloc(2);
  assert(ptr);
  ptr[1] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 2
  })));
  assert(!data_storage_drain(&storage, 1));
  data_storage_finish(&storage);
  assert(storage.frames->offset == 0);
  assert(storage.frames->len == 1);
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  /* No frames, but it should be aware of that. */
  data_storage_finish(&storage);
  assert(data_storage_drain(&storage, 0));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage freeable not-readonly");
  ptr = malloc(1);
  assert(ptr);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage freeable readonly");
  ptr = malloc(1);
  assert(ptr);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .read_only = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage not-freeable not-readonly");
  ptr = malloc(1);
  assert(ptr);
  ptr[0] = TEST_MAGIC;
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .dont_free = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage not-freeable readonly");
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  free(ptr);
  end_test();
  
  begin_test("storage mmapped freeable not-readonly");
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
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage mmapped freeable readonly");
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
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  test_expect_segfault(ptr);
  end_test();
  
  begin_test("storage mmapped not-freeable not-readonly");
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
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  test_expect_no_segfault(ptr);
  end_test();
  
  begin_test("storage mmapped not-freeable readonly");
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .data = ptr,
    .len = 1,
    .mmaped = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  test_expect_no_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 1));
  assert(data_storage_is_empty(&storage));
  test_expect_no_segfault(ptr);
  assert(storage.frames->data[0] == TEST_MAGIC);
  assert(!munmap(ptr, 1));
  test_expect_segfault(ptr);
  end_test();
  
  begin_test("storage file freeable not-readonly");
  char* _dir = get_current_dir_name();
  assert(_dir);
  const size_t len = strlen(_dir);
  const size_t len2 = strlen("/tests/test.txt");
  char* dir = malloc(len + len2 + 1);
  assert(dir);
  memcpy(dir, _dir, len);
  memcpy(dir + len, "/tests/test.txt", len2 + 1);
  
  int file = open(dir, 0);
  /*
   * Make sure to execute "make test"
   * from the root directory (above this).
   */
  assert(file != -1);
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .fd = file,
    .len = 3,
    .offset = 1,
    .file = 1
  })));
  assert(storage.frames->data[1] == TEST_MAGIC);
  assert(data_storage_drain(&storage, 2));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage file freeable readonly");
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
  assert(data_storage_drain(&storage, 2));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage file not-freeable not-readonly");
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
  assert(data_storage_drain(&storage, 2));
  assert(data_storage_is_empty(&storage));
  end_test();
  
  begin_test("storage file not-freeable readonly");
  assert(!data_storage_add(&storage, &((struct data_frame) {
    .fd = file,
    .len = 3,
    .offset = 1,
    .file = 1,
    .dont_free = 1,
    .read_only = 1
  })));
  assert(storage.frames->file);
  assert(data_storage_drain(&storage, 2));
  assert(data_storage_is_empty(&storage));
  assert(!close(file));
  end_test();
  
  begin_test("storage free");
  data_storage_free(&storage);
  assert(data_storage_is_empty(&storage));
  free(_dir);
  free(dir);
  end_test();
  return 0;
}