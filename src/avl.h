#ifndef YSfCugXXYkw_b__jFFk___qGEnC_chg0
#define YSfCugXXYkw_b__jFFk___qGEnC_chg0 1

#ifdef __cplusplus
extern "C" {
#endif

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
  struct avl_node* (*new_node)(struct avl_tree*);
  long (*compare)(const void*, const void*);
  unsigned long is_empty;
};

extern struct avl_tree avl_tree(const unsigned long, struct avl_node* (*)(struct avl_tree*), long (*)(const void*, const void*));

extern int avl_insert(struct avl_tree* const, const void* const, const int);

extern struct avl_node* avl_search(struct avl_tree* const, const void* const);

extern struct avl_node* avl_delete_node(struct avl_tree* const, struct avl_node*);

extern struct avl_node* avl_delete(struct avl_tree* const, const void* const);

extern struct avl_node* avl_try_delete(struct avl_tree* const, const void* const);

extern struct avl_node* avl_min(struct avl_tree* const);

extern struct avl_node* avl_max(struct avl_tree* const);

#ifdef __cplusplus
}
#endif

#endif // YSfCugXXYkw_b__jFFk___qGEnC_chg0