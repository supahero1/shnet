/*
   Copyright 2021 shädam

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
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
  struct net_avl_tree tree;
  uint32_t counter;
  pthread_mutex_t protect;
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