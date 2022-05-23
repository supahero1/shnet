#include "tests.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <shnet/avl.h>

#define TIMES 100

unsigned long avl_get_max_height(struct avl_tree* const tree, struct avl_node* const node, unsigned long height) {
  if(node->left == NULL) {
    if(node->right == NULL) {
      return height;
    } else {
      return avl_get_max_height(tree, node->right, height + 1);
    }
  } else {
    if(node->right == NULL) {
      return avl_get_max_height(tree, node->left, height + 1);
    } else {
      unsigned long a = avl_get_max_height(tree, node->right, height + 1);
      unsigned long b = avl_get_max_height(tree, node->left, height + 1);
      if(a > b) {
        return a;
      }
      return b;
    }
  }
}

long avl_get_balance(struct avl_tree* const tree, struct avl_node* const node) {
  if(node->left != NULL) {
    if(node->right != NULL) {
      return avl_get_max_height(tree, node->right, 0) - avl_get_max_height(tree, node->left, 0);
    } else {
      return -avl_get_max_height(tree, node->left, 0) - 1;
    }
  } else {
    if(node->right != NULL) {
      return avl_get_max_height(tree, node->right, 0) + 1;
    } else {
      return 0;
    }
  }
}

void avl_integrity_check(struct avl_tree* const tree, struct avl_node* const node, const int child) {
  if(node->parent != NULL) {
    if(node == node->parent) {
      _debug("\r(0) broken link", 1);
      TEST_FAIL;
    }
    if(child == 0 && node->parent->left != node) {
      _debug("\r(1) broken link", 1);
      TEST_FAIL;
    }
    if(child == 1 && node->parent->right != node) {
      _debug("\r(2) broken link", 1);
      TEST_FAIL;
    }
  }
  if(node->left != NULL) {
    if(node->left->parent != node) {
      _debug("\r(3) broken link", 1);
      TEST_FAIL;
    }
    avl_integrity_check(tree, node->left, 0);
  }
  if(node->right != NULL) {
    if(node->right->parent != node) {
      _debug("\r(4) broken link", 1);
      TEST_FAIL;
    }
    avl_integrity_check(tree, node->right, 1);
  }
}

void avl_balance_check(struct avl_tree* const tree, struct avl_node* node) {
  long balance = avl_get_balance(tree, node);
  if(balance != node->balance) {
    _debug("\rinvalid balance, has %ld, should have %ld", 1, node->balance, balance);
    TEST_FAIL;
  }
  if(node->left != NULL) {
    avl_balance_check(tree, node->left);
  }
  if(node->right != NULL) {
    avl_balance_check(tree, node->right);
  }
}

struct tree_node {
  struct avl_node node;
  unsigned long val;
};

unsigned long GetTime(const unsigned long nanoseconds) {
  struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  return nanoseconds + tp.tv_sec * 1000000000 + tp.tv_nsec;
}

int cmp(const void* a, const void* b) {
  if(*((unsigned long*) a) > *((unsigned long*) b)) {
    return 1;
  } else if(*((unsigned long*) b) > *((unsigned long*) a)) {
    return -1;
  } else {
    return 0;
  }
}

struct tree_node mem[5000];
unsigned long used = 0;
struct avl_tree tree;

void* newnode(struct avl_tree* tree) {
  return &mem[++used - 1];
}

void delnode(struct avl_node* node) {
  --used;
  if((uintptr_t)(mem + used) != (uintptr_t) node) {
    (void) memcpy(node, mem + used, sizeof(struct tree_node));
    if(node->parent != NULL) {
      if((uintptr_t) node->parent->right == (uintptr_t)(mem + used)) {
        node->parent->right = node;
      } else {
        node->parent->left = node;
      }
    } else {
      tree.head = node;
    }
    if(node->left != NULL) {
      node->left->parent = node;
    }
    if(node->right != NULL) {
      node->right->parent = node;
    }
  }
}

int main() {
  _debug("Testing avl:", 1);
  {
    struct timespec tp;
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec + tp.tv_sec * 1000000000);
  }
  _debug("1. Stress test", 1);
  memset(&tree, 0, sizeof(tree));
  tree.item_size = sizeof(unsigned long);
  tree.new_node = newnode;
  tree.compare = cmp;
  for(unsigned long i = 0; i < TIMES; ++i) {
    if(used != 5000) {
      int how_much;
      if(used < 2000) {
        how_much = 2000;
      } else {
        how_much = (rand() % (5000 - used)) + 1;
      }
      for(int j = 0; j < how_much; ++j) {
        const unsigned long e = rand();
        (void) avl_insert(&tree, &e, avl_disallow_copies);
        avl_integrity_check(&tree, tree.head, 0);
        avl_balance_check(&tree, tree.head);
      }
    } else {
      const int how_much = (rand() % 3000) + 2001;
      for(int j = 0; j < how_much; ++j) {
        struct avl_node* const ptr = avl_try_delete(&tree, &mem[0].val);
        if(ptr == NULL) {
          TEST_FAIL;
        }
        if(tree.not_empty == 1) {
          avl_integrity_check(&tree, tree.head, 0);
          avl_balance_check(&tree, tree.head);
          delnode(ptr);
          avl_integrity_check(&tree, tree.head, 0);
          avl_balance_check(&tree, tree.head);
        }
      }
    }
    printf("\r%.*f%%", 1, (float)(i + 1) * 100 / TIMES);
    fflush(stdout);
  }
  printf("\r");
  TEST_PASS;
  _debug("2. Single deletion", 1);
  used = 0;
  for(unsigned long i = 0; i < 33000; ++i) {
    (void) avl_insert(&tree, &(unsigned long){ 1 }, avl_allow_copies);
    (void) avl_try_delete(&tree, &mem[0].val);
    used = 0;
  }
  TEST_PASS;
}
