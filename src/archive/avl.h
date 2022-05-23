#ifndef YSfCugXXYkw_b__jFFk___qGEnC_chg0
#define YSfCugXXYkw_b__jFFk___qGEnC_chg0 1

#include <stdint.h>

/* No error lib. This is a fast AVL tree implementation that makes the
application manage its memory, which is partially why it's so fast. It also
means there is no room for any memory allocation or such, that otherwise would
be handled by the error lib.
Memory efficient version: swap all pointers with uint32_t's.
Flexible version: each avl_node should have a uint32_t which may be used by the
application in any way to reference the node with the corresponding object.
This way, nodes may carry data of various size. Probably increased performance
because of good node locality (given data is bigger than 4 bytes).

Note: It's probably better to not use contmem and swap all pointers with
uint64_t's. This module is unused in other parts of the library anyway. If to be
ever used, it should be adjusted to suit the needs.
*/

enum avl_consts {
  avl_allow_copies = 0,
  avl_disallow_copies = 1
};

struct avl_node {
  struct avl_node* parent;
  struct avl_node* left;
  struct avl_node* right;
  int balance;
};

struct avl_tree {
  struct avl_node* head;
  void* (*new_node)(struct avl_tree*);
  int (*compare)(const void*, const void*);
  uint32_t item_size;
  uint32_t not_empty;
};

extern int   avl_insert(struct avl_tree* const, const void* const, const int);

extern void* avl_search(struct avl_tree* const, const void* const);

extern void* avl_delete_node(struct avl_tree* const, struct avl_node*);

extern void* avl_delete(struct avl_tree* const, const void* const);

extern void* avl_try_delete(struct avl_tree* const, const void* const);

extern void* avl_min(struct avl_tree* const);

extern void* avl_max(struct avl_tree* const);

#endif // YSfCugXXYkw_b__jFFk___qGEnC_chg0
