/*
  Copyright (c) 2021 shädam

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef N_OhP5seHByu7beYY15_JeHmwXut_9sV
#define N_OhP5seHByu7beYY15_JeHmwXut_9sV 1

#ifdef __cplusplus
extern "C" {
#endif

enum flesto_consts {
  flesto_success,
  flesto_out_of_memory,
  flesto_failure,
  
  flesto_right = 0,
  flesto_left
};

struct flesto_part {
  char* contents;
  unsigned long max_capacity;
  unsigned long capacity;
};

struct flesto {
  struct flesto_part* parts;
  unsigned long last_part;
  unsigned long max_amount_of_parts;
  unsigned long default_max_capacity;
  unsigned long delete_after;
};

extern void vf(const struct flesto* const);/////////////////////////////

extern struct flesto flesto(const unsigned long, const unsigned long);

extern int flesto_resize(struct flesto* const, const unsigned long);

extern int flesto_add_part(struct flesto* const, char* const, const unsigned long);

extern int flesto_add_parts(struct flesto* const, char** const, unsigned long* const, const unsigned long);

extern int flesto_remove_empty_parts(struct flesto* const);

extern int flesto_add_item(struct flesto* const, const void* const, const unsigned long);

extern int flesto_add_items(struct flesto* const, const void* const, unsigned long, const unsigned long);

extern void flesto_remove_item(struct flesto* const, void* const, const unsigned long);

extern int flesto_remove_items(struct flesto* const, void* const, const unsigned long, const unsigned long, const unsigned long, const unsigned long);

extern void flesto_pop(struct flesto* const, unsigned long);

extern int flesto_delete_after(struct flesto* const);

extern unsigned long flesto_trailing_items(const struct flesto* const, unsigned long, const unsigned long);

extern void* flesto_idx_to_ptr(const struct flesto* const, unsigned long, unsigned long, unsigned long, const int, unsigned long* const, unsigned long* const);

extern int flesto_ptr_to_idx(const struct flesto* const, const void* const, unsigned long* const, unsigned long* const, const unsigned long, const int);

extern void flesto_copy_items(const struct flesto* const, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, const unsigned long);

extern unsigned long flesto_amount_of_items(const struct flesto* const);

extern int flesto_check_address(const struct flesto* const, const void* const);

extern int flesto_check_address1(const struct flesto* const, const void* const);

extern int flesto_check_address2(const struct flesto* const, const void* const);

extern int flesto_check_address3(const struct flesto* const, const void* const);

extern void flesto_free(struct flesto* const);

#ifdef __cplusplus
}
#endif

#endif // N_OhP5seHByu7beYY15_JeHmwXut_9sV