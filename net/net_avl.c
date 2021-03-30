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

#include "net_avl.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct net_avl_tree net_avl_tree(const uint32_t max_items_per_part) {
  return (struct net_avl_tree) {
    .parts = NULL,
    .head = NULL,
    .last = NULL,
    .count = 0,
    .amount = 0,
    .max_items_per_part = max_items_per_part
  };
}

int net_avl_init(struct net_avl_tree* const tree) {
  void* ptr;
  uint_fast32_t i = 0;
  ptr = malloc(sizeof(struct net_avl_node*));
  if(ptr == NULL) {
    return ENOMEM;
  }
  tree->parts = ptr;
  ptr = malloc(sizeof(struct net_avl_node) * tree->max_items_per_part);
  if(ptr == NULL) {
    free(tree->parts);
    return ENOMEM;
  }
  for(; i < sizeof(struct net_avl_tree) * tree->max_items_per_part; i += 4096) {
    (void) memset(ptr + i, 0, sizeof(long));
  }
  tree->parts[0] = ptr;
  return 0;
}

void net_avl_free(struct net_avl_tree* const tree) {
  const uint32_t amount = tree->amount;
  uint32_t i = 0;
  if(amount != 0) {
    for(; i < amount; ++i) {
      free(tree->parts[i]);
    }
    free(tree->parts);
    tree->parts = NULL;
    tree->amount = 0;
    tree->count = 0;
  }
}

__nonnull((1, 2))
static void net_avl_rotate(struct net_avl_tree* const tree, struct net_avl_node* const node, struct net_avl_node* const temp, const int sign) {
  if(sign == -1) {
    node->left = temp->right;
    if(temp->right != NULL) {
      temp->right->parent = node;
    }
    temp->right = node;
  } else {
    node->right = temp->left;
    if(temp->left != NULL) {
      temp->left->parent = node;
    }
    temp->left = node;
  }
  temp->parent = node->parent;
  if(node->parent != NULL) {
    if(node == node->parent->left) {
      node->parent->left = temp;
    } else {
      node->parent->right = temp;
    }
  } else {
    tree->head = temp;
  }
  node->parent = temp;
}

__nonnull((1, 2))
static void net_avl_rotate2(struct net_avl_tree* const tree, struct net_avl_node* const node, struct net_avl_node* const temp, const int sign) {
  if(sign == -1) {
    struct net_avl_node* const temp2 = temp->right;
    node->left = temp2->right;
    if(temp2->right != NULL) {
      temp2->right->parent = node;
    }
    temp2->parent = node->parent;
    if(node->parent != NULL) {
      if(node == node->parent->left) {
        node->parent->left = temp2;
      } else {
        node->parent->right = temp2;
      }
    } else {
      tree->head = temp2;
    }
    node->parent = temp2;
    if(temp2->balance >= 0) {
      node->balance = 0;
    } else {
      node->balance = -temp2->balance;
    }
    temp->right = temp2->left;
    if(temp2->left != NULL) {
      temp2->left->parent = temp;
    }
    temp->parent = temp2;
    if(temp2->balance <= 0) {
      temp->balance = 0;
    } else {
      temp->balance = -temp2->balance;
    }
    temp2->right = node;
    temp2->left = temp;
    temp2->balance = 0;
  } else {
    struct net_avl_node* const temp2 = temp->left;
    node->right = temp2->left;
    if(temp2->left != NULL) {
      temp2->left->parent = node;
    }
    temp2->parent = node->parent;
    if(node->parent != NULL) {
      if(node == node->parent->left) {
        node->parent->left = temp2;
      } else {
        node->parent->right = temp2;
      }
    } else {
      tree->head = temp2;
    }
    node->parent = temp2;
    if(temp2->balance <= 0) {
      node->balance = 0;
    } else {
      node->balance = -temp2->balance;
    }
    temp->left = temp2->right;
    if(temp2->right != NULL) {
      temp2->right->parent = temp;
    }
    temp->parent = temp2;
    if(temp2->balance >= 0) {
      temp->balance = 0;
    } else {
      temp->balance = -temp2->balance;
    }
    temp2->left = node;
    temp2->right = temp;
    temp2->balance = 0;
  }
}

int net_avl_insert(struct net_avl_tree* const tree, const struct NETSocket socket) {
  void* ptr;
  struct net_avl_node* node = tree->head;
  struct net_avl_node* temp;
  uint_fast32_t i = 0;
  int sign;
  if(tree->count == 0) {
    tree->parts[0][0] = (struct net_avl_node) {
      .parent = NULL,
      .left = NULL,
      .right = NULL,
      .socket = socket,
      .balance = 0
    };
    tree->count = 1;
    tree->amount = 1;
    tree->head = tree->parts[0];
    tree->last = tree->parts[0];
    return 0;
  } else if((tree->count + tree->amount) % tree->max_items_per_part == 0) {
    ptr = realloc(tree->parts, sizeof(struct net_avl_node*) * (tree->amount + 1));
    if(ptr == NULL) {
      return ENOMEM;
    }
    tree->parts = ptr;
    ptr = malloc(sizeof(struct net_avl_node) * tree->max_items_per_part);
    if(ptr == NULL) {
      return ENOMEM;
    }
    for(; i < sizeof(struct net_avl_tree) * tree->max_items_per_part; i += 4096) {
      (void) memset(ptr + i, 0, sizeof(long));
    }
    tree->parts[tree->amount++] = ptr;
    tree->last = ptr - 1;
  }
  temp = ++tree->last;
  ++tree->count;
  while(1) {
    if(socket.sfd > node->socket.sfd) {
      if(node->right != NULL) {
        node = node->right;
      } else {
        *temp = (struct net_avl_node) {
          .parent = node,
          .left = NULL,
          .right = NULL,
          .socket = socket,
          .balance = 0
        };
        node->right = temp;
        ++node->balance;
        sign = 1;
        break;
      }
    } else {
      if(node->left != NULL) {
        node = node->left;
      } else {
        *temp = (struct net_avl_node) {
          .parent = node,
          .left = NULL,
          .right = NULL,
          .socket = socket,
          .balance = 0
        };
        node->left = temp;
        --node->balance;
        sign = -1;
        break;
      }
    }
  }
  if(node->balance == 0 || node->parent == NULL) {
    return 0;
  }
  if(node->parent->left == node) {
    sign = -1;
  } else {
    sign = 1;
  }
  temp = node;
  node = node->parent;
  while(1) {
    if(node->balance == 0) {
      node->balance += sign;
      if(node->parent != NULL) {
        if(node->parent->left == node) {
          sign = -1;
        } else {
          sign = 1;
        }
        temp = node;
        node = node->parent;
      } else {
        return 0;
      }
    } else {
      node->balance += sign;
      if(node->balance == 0) {
        return 0;
      }
      if(sign * temp->balance == 1) {
        net_avl_rotate(tree, node, temp, sign);
        node->balance = 0;
        temp->balance = 0;
      } else {
        net_avl_rotate2(tree, node, temp, sign);
      }
      return 0;
    }
  }
}

struct NETSocket* net_avl_search(struct net_avl_tree* const tree, const int sfd) {
  struct net_avl_node* node = tree->head;
  while(1) {
    if(sfd > node->socket.sfd) {
      node = node->right;
    } else if(sfd < node->socket.sfd) {
      node = node->left;
    } else {
      return &node->socket;
    }
  }
}

__nonnull((1, 2))
static void net_avl_swap(struct net_avl_tree* const tree, struct net_avl_node* node) {
  *node = *tree->last;
  if(node->parent != NULL) {
    if(node->parent->right == tree->last) {
      node->parent->right = node;
    } else {
      node->parent->left = node;
    }
  } else {
    tree->head = node;
  }
  if(node->left != NULL) {
    node->left->parent = node;
  }
  if(node->right != NULL) {
    node->right->parent = node;
  }
  if((tree->count + tree->amount) % tree->max_items_per_part == 0) {
    free(tree->parts[--tree->amount]);
    tree->parts = realloc(tree->parts, sizeof(struct net_avl_tree*) * tree->amount);
  }
}

void net_avl_delete(struct net_avl_tree* const tree, const int sfd) {
  struct net_avl_node* node = tree->head;
  struct net_avl_node* temp;
  struct net_avl_node* temp2;
  uint32_t sign;
  --tree->count;
  while(1) {
    if(sfd > node->socket.sfd) {
      node = node->right;
    } else if(sfd < node->socket.sfd) {
      node = node->left;
    } else {
      if(node->right == NULL) {
        if(node->left == NULL) {
          if(node->parent != NULL) {
            if(node->parent->right == node) {
              node->parent->right = NULL;
              sign = -1;
            } else {
              node->parent->left = NULL;
              sign = 1;
            }
            if(node->parent != tree->last) {
              temp2 = node->parent;
              if(node != tree->last) {
                net_avl_swap(tree, node);
              }
            } else {
              temp2 = node;
              net_avl_swap(tree, node);
            }
          } else {
            tree->count = 0;
            return;
          }
        } else {
          node->left->parent = node->parent;
          if(node->parent != NULL) {
            if(node->parent->right == node) {
              node->parent->right = node->left;
              sign = -1;
            } else {
              node->parent->left = node->left;
              sign = 1;
            }
            if(node->parent != tree->last) {
              temp2 = node->parent;
              if(node != tree->last) {
                net_avl_swap(tree, node);
              }
            } else {
              temp2 = node;
              net_avl_swap(tree, node);
            }
          } else {
            tree->head = node->left;
            if(node != tree->last) {
              net_avl_swap(tree, node);
            }
            return;
          }
        }
      } else {
        if(node->left == NULL) {
          node->right->parent = node->parent;
          if(node->parent != NULL) {
            if(node->parent->right == node) {
              node->parent->right = node->right;
              sign = -1;
            } else {
              node->parent->left = node->right;
              sign = 1;
            }
            if(node->parent != tree->last) {
              temp2 = node->parent;
              if(node != tree->last) {
                net_avl_swap(tree, node);
              }
            } else {
              temp2 = node;
              net_avl_swap(tree, node);
            }
          } else {
            tree->head = node->right;
            if(node != tree->last) {
              net_avl_swap(tree, node);
            }
            return;
          }
        } else {
          temp = node->left;
          if(temp->right != NULL) {
            temp = node->right;
            if(temp->left == NULL) {
              if(temp->right != NULL) {
                temp->right->parent = node;
              }
              node->right = temp->right;
              node->socket = temp->socket;
              if(node != tree->last) {
                temp2 = node;
                if(temp != tree->last) {
                  net_avl_swap(tree, temp);
                }
              } else {
                temp2 = temp;
                net_avl_swap(tree, temp);
              }
              sign = -1;
            } else {
              do {
                temp = temp->left;
              } while(temp->left != NULL);
              temp->parent->left = temp->right;
              if(temp->right != NULL) {
                temp->left->parent = temp->parent;
              }
              node->socket = temp->socket;
              if(temp->parent != tree->last) {
                temp2 = temp->parent;
                if(temp != tree->last) {
                  net_avl_swap(tree, temp);
                }
              } else {
                temp2 = temp;
                net_avl_swap(tree, temp);
              }
              sign = 1;
            }
          } else {
            if(temp->left != NULL) {
              temp->left->parent = node;
            }
            node->left = temp->left;
            node->socket = temp->socket;
            if(node != tree->last) {
              temp2 = node;
              if(temp != tree->last) {
                net_avl_swap(tree, temp);
              }
            } else {
              temp2 = temp;
              net_avl_swap(tree, temp);
            }
            sign = 1;
          }
        }
      }
      break;
    }
  }
  --tree->last;
  node = temp2;
  while(1) {
    if(node->balance == 0) {
      node->balance += sign;
      return;
    }
    if(node->balance + sign != 0) {
      if(sign == 1) {
        temp = node->right;
        if(temp->balance >= 0) {
          net_avl_rotate(tree, node, temp, 1);
          if(temp->balance == 0) {
            temp->balance = -1;
            return;
          } else {
            temp->balance = 0;
            node->balance = 0;
            node = temp;
          }
        } else {
          net_avl_rotate2(tree, node, temp, 1);
          node = node->parent;
        }
      } else {
        temp = node->left;
        if(temp->balance <= 0) {
          net_avl_rotate(tree, node, temp, -1);
          if(temp->balance == 0) {
            temp->balance = 1;
            return;
          } else {
            temp->balance = 0;
            node->balance = 0;
            node = temp;
          }
        } else {
          net_avl_rotate2(tree, node, temp, -1);
          node = node->parent;
        }
      }
    } else {
      node->balance += sign;
    }
    if(node->parent != NULL) {
      if(node->parent->left == node) {
        sign = 1;
      } else {
        sign = -1;
      }
      node = node->parent;
    } else {
      return;
    }
  }
}

struct net_avl_multithread_tree net_avl_multithread_tree(const uint32_t max_items_per_part) {
  return (struct net_avl_multithread_tree) {
    .tree = net_avl_tree(max_items_per_part)
  };
}

int net_avl_multithread_init(struct net_avl_multithread_tree* const tree) {
  int err = pthread_mutex_init(&tree->mutex, NULL);
  if(err != 0) {
    return err;
  }
  err = pthread_mutex_init(&tree->protect, NULL);
  if(err != 0) {
    (void) pthread_mutex_destroy(&tree->mutex);
    return err;
  }
  err = net_avl_init(&tree->tree);
  if(err != 0) {
    (void) pthread_mutex_destroy(&tree->mutex);
    (void) pthread_mutex_destroy(&tree->protect);
  } else {
    tree->counter = 0;
  }
  return err;
}

void net_avl_multithread_free(struct net_avl_multithread_tree* const tree) {
  net_avl_free(&tree->tree);
  (void) pthread_mutex_destroy(&tree->mutex);
  (void) pthread_mutex_destroy(&tree->protect);
}

int net_avl_multithread_insert(struct net_avl_multithread_tree* const tree, const struct NETSocket socket) {
  (void) pthread_mutex_lock(&tree->mutex);
  int err = net_avl_insert(&tree->tree, socket);
  (void) pthread_mutex_unlock(&tree->mutex);
  return err;
}

struct NETSocket* net_avl_multithread_search(struct net_avl_multithread_tree* const tree, const int sfd) {
  (void) pthread_mutex_lock(&tree->protect);
  if(++tree->counter == 1) {
    (void) pthread_mutex_lock(&tree->mutex);
  }
  (void) pthread_mutex_unlock(&tree->protect);
  struct NETSocket* socket = net_avl_search(&tree->tree, sfd);
  (void) pthread_mutex_lock(&tree->protect);
  if(--tree->counter == 0) {
    (void) pthread_mutex_unlock(&tree->mutex);
  }
  (void) pthread_mutex_unlock(&tree->protect);
  return socket;
}

void net_avl_multithread_delete(struct net_avl_multithread_tree* const tree, const int sfd) {
  (void) pthread_mutex_lock(&tree->mutex);
  net_avl_delete(&tree->tree, sfd);
  (void) pthread_mutex_unlock(&tree->mutex);
}