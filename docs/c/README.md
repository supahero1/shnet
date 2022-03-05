This directory contains documentation of all actively used modules in the library. Documentation of WIP modules might not be here yet. Symbols that aren't supposed to be used by the application might not be documented.

# Code of conduct

**Unless specified otherwise...**

- ... before using any structure, zero it first.

  ```c
  struct some thing = {0};
  ```

- ... functions return 0 on success and other values on failure, setting errno to the error's code.

  ```c
  assert(! func());
  /* assert(! error );
  assert(no error); */
  ```

- ... structures can be reused immediatelly after a call to an appropriate function freeing them (if any). They can only be reused if they will be used the same way they were before. Otherwise, full zeroing is needed.

  ```c
  struct data_type data = {0};
  data_type_init(&data);
  /* ... use it ... */

  data_type_free(&data);
  /* ready to be used again, without zeroing */
  data_type_init(&data);
  ```

- ... structures and functions are thread-unsafe.

  ```c
  // thread 1:
  do_something(&structure); // UNDEFINED BEHAVIOR
  
  // thread 2:
  do_something_else(&structure); // UNDEFINED BEHAVIOR
  ```

- ... functions that end with the word "raw" are always thread-unsafe. If a raw function exists, there also exists its counterpart without the word "raw" and which is thread-safe. This holds true only if both of the functions exist. Functions without the counterpart might or might not be thread-safe.

  ```c
  /* Thread-safe */
  some_function(some_structure);
  
  /* ... */
  
  some_structure_lock(some_structure);
  
  /* Thread-unsafe */
  some_function_raw(some_structure);
  some_other_function_raw(some_structure);
  
  some_structure_unlock(some_structure);
  ```