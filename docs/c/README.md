# Code of conduct

This directory contains documentation of all actively used modules in the
library. Documentation of WIP modules might not be here yet. Symbols that
aren't supposed to be used by the application might not be documented.

Each module has a list of dependencies somewhere at the top of its
documentation (or it doesn't, meaning it's a leaf). You should get
to know them first or otherwise you might not underestand what the
documentation explains.

**Unless specified otherwise...**

- ... before using any structure, zero it first.

  ```c
  struct some thing = {0};
  ```

- ... functions return 0 on success and other values
      on failure, setting errno to the error's code.

  ```c
  assert(! func());
  /* assert(! error );
  assert(no error); */
  
  if(func()) {  /* if(error occured){} */
    /* ... */
  }
  ```

- ... structures can be reused immediatelly after a call to an appropriate
      function freeing them (if any). They can only be reused if they will
      be used the same way they were before. Otherwise, full zeroing is needed.

  ```c
  struct data_type data = {0};
  data_type_init(&data);
  /* ... use it ... */

  data_type_free(&data);
  /* ready to be used again, without zeroing */
  data_type_init(&data);
  
  data_type_free(&data);
  /* can't reuse for a different purpose without zeroing */
  data = {0};
  other_data_type_init(&data);
  ```

- ... structures and functions are thread-unsafe by default.

  ```c
  // thread 1:
  do_something(&structure); // UNDEFINED BEHAVIOR
  
  // thread 2:
  do_something_else(&structure); // UNDEFINED BEHAVIOR
  ```

- ... functions that end with the word "raw" are always thread-unsafe. The
      documentation will mention if there exists a pair of functions where
      one is raw and the other is not. In that case, the non-raw function
      is always thread-safe and there must exist a locking function for
      the data type to allow the usage of raw functions.

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
