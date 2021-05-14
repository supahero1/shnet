#ifndef YSfCugXXYkw_b__jFFk___qGEnC_chg0
#define YSfCugXXYkw_b__jFFk___qGEnC_chg0 1

enum avl_consts {
  avl_success,
  avl_out_of_memory,
  avl_failure,
  
  avl_allow_copies = 0,
  avl_disallow_copies = 1
};

struct avl_node {
  struct avl_node* parent;
  struct avl_node* left;
  struct avl_node* right;
  long balance;
};

struct avl_tree {
  struct avl_node* head;
  unsigned long item_size;
  void* (*new_node)(struct avl_tree*);
  long (*compare)(const void*, const void*);
  unsigned long is_empty;
};

extern struct avl_tree avl_tree(const unsigned long, void* (*)(struct avl_tree*), long (*)(const void*, const void*));

extern int avl_insert(struct avl_tree* const, const void* const, const int);

extern void* avl_search(struct avl_tree* const, const void* const);

extern void* avl_delete_node(struct avl_tree* const, struct avl_node*);

extern void* avl_delete(struct avl_tree* const, const void* const);

extern void* avl_try_delete(struct avl_tree* const, const void* const);

extern void* avl_min(struct avl_tree* const);

extern void* avl_max(struct avl_tree* const);

#endif // YSfCugXXYkw_b__jFFk___qGEnC_chg0