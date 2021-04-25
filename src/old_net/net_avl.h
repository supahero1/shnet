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

#ifndef lu_eoVE_9ZX_B__QSYe_uZ2d_kgw_b_f
#define lu_eoVE_9ZX_B__QSYe_uZ2d_kgw_b_f 1

#ifdef __cplusplus
extern "C" {
#endif

#include "net_base.h"

#include <stdint.h>
#include <pthread.h>

struct net_avl_node {
  struct net_avl_node* parent;
  struct net_avl_node* left;
  struct net_avl_node* right;
#if __WORDSIZE == 64
  struct NETSocket socket;
  int balance;
#else
  int balance;
  struct NETSocket socket;
#endif
};

struct net_avl_tree {
  struct net_avl_node** parts;
  struct net_avl_node* head;
  struct net_avl_node* last;
  uint32_t count;
  uint32_t amount;
  uint32_t max_items_per_part;
};

extern struct net_avl_tree net_avl_tree(const uint32_t);

extern int net_avl_init(struct net_avl_tree* const);

extern void net_avl_free(struct net_avl_tree* const);

extern int net_avl_insert(struct net_avl_tree* const, const struct NETSocket);

extern struct NETSocket* net_avl_search(struct net_avl_tree* const, const int);

extern void net_avl_delete(struct net_avl_tree* const, const int);

struct net_avl_multithread_tree {
  pthread_mutex_t mutex;
#if __WORDSIZE == 64
  struct net_avl_tree tree;
  uint32_t counter;
  pthread_mutex_t protect;
#else
  struct net_avl_tree tree;
  pthread_mutex_t protect;
  uint32_t counter;
#endif
};

extern struct net_avl_multithread_tree net_avl_multithread_tree(const uint32_t);

extern int net_avl_multithread_init(struct net_avl_multithread_tree* const);

extern void net_avl_multithread_free(struct net_avl_multithread_tree* const);

extern int net_avl_multithread_insert(struct net_avl_multithread_tree* const, const struct NETSocket);

extern struct NETSocket* net_avl_multithread_search(struct net_avl_multithread_tree* const, const int);

extern void net_avl_multithread_delete(struct net_avl_multithread_tree* const, const int);

#ifdef __cplusplus
}
#endif

#endif // lu_eoVE_9ZX_B__QSYe_uZ2d_kgw_b_f