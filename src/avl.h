#ifndef YSfCugXXYkw_b__jFFk___qGEnC_chg0
#define YSfCugXXYkw_b__jFFk___qGEnC_chg0 1

#include <stdint.h>

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

extern int avl_insert(struct avl_tree* const, const void* const, const int);

extern void* avl_search(struct avl_tree* const, const void* const);

extern void* avl_delete_node(struct avl_tree* const, struct avl_node*);

extern void* avl_delete(struct avl_tree* const, const void* const);

extern void* avl_try_delete(struct avl_tree* const, const void* const);

extern void* avl_min(struct avl_tree* const);

extern void* avl_max(struct avl_tree* const);

#endif // YSfCugXXYkw_b__jFFk___qGEnC_chg0