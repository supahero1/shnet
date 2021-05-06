#include "avl.h"

#include <stdlib.h>
#include <string.h>

struct avl_tree avl_tree(const unsigned long item_size, void* (*new_node)(struct avl_tree*), long (*compare)(const void*, const void*)) {
  return (struct avl_tree) {
    .head = NULL,
    .item_size = item_size,
    .new_node = new_node,
    .compare = compare,
    .is_empty = 1
  };
}

static void avl_rotate(struct avl_tree* const tree, struct avl_node* const node, struct avl_node* const temp, const int sign) {
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

static void avl_rotate2(struct avl_tree* const tree, struct avl_node* const node, struct avl_node* const temp, const int sign) {
  if(sign == -1) {
    struct avl_node* const temp2 = temp->right;
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
    struct avl_node* const temp2 = temp->left;
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

static struct avl_node* avl_insert_seek_nc(struct avl_tree* const tree, const void* const item) {
  struct avl_node* node = tree->head;
  while(1) {
    const long diff = tree->compare(item, node + 1);
    if(diff > 0) {
      if(node->right != NULL) {
        node = node->right;
      } else {
        struct avl_node* const temp = tree->new_node(tree);
        if(temp == NULL) {
          return NULL;
        }
        *temp = (struct avl_node) {
          .parent = node,
          .left = NULL,
          .right = NULL,
          .balance = 0
        };
        (void) memcpy(temp + 1, item, tree->item_size);
        node->right = temp;
        ++node->balance;
        break;
      }
    } else if(diff < 0) {
      if(node->left != NULL) {
        node = node->left;
      } else {
        struct avl_node* const temp = tree->new_node(tree);
        if(temp == NULL) {
          return NULL;
        }
        *temp = (struct avl_node) {
          .parent = node,
          .left = NULL,
          .right = NULL,
          .balance = 0
        };
        (void) memcpy(temp + 1, item, tree->item_size);
        node->left = temp;
        --node->balance;
        break;
      }
    } else {
      return NULL;
    }
  }
  return node;
}

static struct avl_node* avl_insert_seek(struct avl_tree* const tree, const void* const item) {
  struct avl_node* node = tree->head;
  while(1) {
    const long diff = tree->compare(item, node + 1);
    if(diff > 0) {
      if(node->right != NULL) {
        node = node->right;
      } else {
        struct avl_node* const temp = tree->new_node(tree);
        if(temp == NULL) {
          return NULL;
        }
        *temp = (struct avl_node) {
          .parent = node,
          .left = NULL,
          .right = NULL,
          .balance = 0
        };
        (void) memcpy(temp + 1, item, tree->item_size);
        node->right = temp;
        ++node->balance;
        break;
      }
    } else {
      if(node->left != NULL) {
        node = node->left;
      } else {
        struct avl_node* const temp = tree->new_node(tree);
        if(temp == NULL) {
          return NULL;
        }
        *temp = (struct avl_node) {
          .parent = node,
          .left = NULL,
          .right = NULL,
          .balance = 0
        };
        (void) memcpy(temp + 1, item, tree->item_size);
        node->left = temp;
        --node->balance;
        break;
      }
    }
  }
  return node;
}

int avl_insert(struct avl_tree* const tree, const void* const item, const int flags) {
  struct avl_node* node;
  if(tree->is_empty == 1) {
    node = tree->new_node(tree);
    if(node == NULL) {
      return avl_failure;
    }
    tree->is_empty = 0;
    *node = (struct avl_node) {
      .parent = NULL,
      .left = NULL,
      .right = NULL,
      .balance = 0
    };
    (void) memcpy(node + 1, item, tree->item_size);
    tree->head = node;
    return avl_success;
  }
  if(flags == avl_disallow_copies) {
    node = avl_insert_seek_nc(tree, item);
    if(node == NULL) {
      return avl_failure;
    }
  } else {
    node = avl_insert_seek(tree, item);
  }
  if(node->balance == 0 || node->parent == NULL) {
    return avl_success;
  }
  long sign;
  if(node->parent->left == node) {
    sign = -1;
  } else {
    sign = 1;
  }
  struct avl_node* temp = node;
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
        return avl_success;
      }
    } else {
      node->balance += sign;
      if(node->balance == 0) {
        return avl_success;
      }
      if(sign * temp->balance == 1) {
        avl_rotate(tree, node, temp, sign);
        node->balance = 0;
        temp->balance = 0;
      } else {
        avl_rotate2(tree, node, temp, sign);
      }
      return avl_success;
    }
  }
}

void* avl_search(struct avl_tree* const tree, const void* const item) {
  struct avl_node* node = tree->head;
  while(1) {
    const long diff = tree->compare(item, node + 1);
    if(diff > 0) {
      node = node->right;
    } else if(diff < 0) {
      node = node->left;
    } else {
      return node + 1;
    }
  }
}

void* avl_delete_node(struct avl_tree* const tree, struct avl_node* node) {
  struct avl_node* temp;
  void* deleted;
  long sign;
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
        deleted = node;
        node = node->parent;
      } else {
        tree->is_empty = 1;
        tree->head = NULL;
        return node;
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
        deleted = node;
        node = node->parent;
      } else {
        tree->head = node->left;
        return node;
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
        deleted = node;
        node = node->parent;
      } else {
        tree->head = node->right;
        return node;
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
          (void) memcpy(node + 1, temp + 1, tree->item_size);
          deleted = temp;
          sign = -1;
        } else {
          do {
            temp = temp->left;
          } while(temp->left != NULL);
          temp->parent->left = temp->right;
          if(temp->right != NULL) {
            temp->right->parent = temp->parent;
          }
          (void) memcpy(node + 1, temp + 1, tree->item_size);
          deleted = temp;
          node = temp->parent;
          sign = 1;
        }
      } else {
        if(temp->left != NULL) {
          temp->left->parent = node;
        }
        node->left = temp->left;
        (void) memcpy(node + 1, temp + 1, tree->item_size);
        deleted = temp;
        sign = 1;
      }
    }
  }
  while(1) {
    if(node->balance == 0) {
      node->balance += sign;
      return deleted;
    }
    if(node->balance + sign != 0) {
      if(sign == 1) {
        temp = node->right;
        if(temp->balance >= 0) {
          avl_rotate(tree, node, temp, 1);
          if(temp->balance == 0) {
            temp->balance = -1;
            return deleted;
          } else {
            temp->balance = 0;
            node->balance = 0;
            node = temp;
          }
        } else {
          avl_rotate2(tree, node, temp, 1);
          node = node->parent;
        }
      } else {
        temp = node->left;
        if(temp->balance <= 0) {
          avl_rotate(tree, node, temp, -1);
          if(temp->balance == 0) {
            temp->balance = 1;
            return deleted;
          } else {
            temp->balance = 0;
            node->balance = 0;
            node = temp;
          }
        } else {
          avl_rotate2(tree, node, temp, -1);
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
      return deleted;
    }
  }
}

void* avl_delete(struct avl_tree* const tree, const void* const item) {
  struct avl_node* node = tree->head;
  while(1) {
    const long diff = tree->compare(item, node + 1);
    if(diff > 0) {
      node = node->right;
    } else if(diff < 0) {
      node = node->left;
    } else {
      return avl_delete_node(tree, node);
    }
  }
}

void* avl_try_delete(struct avl_tree* const tree, const void* const item) {
  struct avl_node* node = tree->head;
  while(1) {
    const long diff = tree->compare(item, node + 1);
    if(diff > 0) {
      if(node->right == NULL) {
        return NULL;
      }
      node = node->right;
    } else if(diff < 0) {
      if(node->left == NULL) {
        return NULL;
      }
      node = node->left;
    } else {
      return avl_delete_node(tree, node);
    }
  }
}

void* avl_min(struct avl_tree* const tree) {
  struct avl_node* node = tree->head;
  while(1) {
    if(node->left != NULL) {
      node = node->left;
    } else {
      return node + 1;
    }
  }
}

void* avl_max(struct avl_tree* const tree) {
  struct avl_node* node = tree->head;
  while(1) {
    if(node->right != NULL) {
      node = node->right;
    } else {
      return node + 1;
    }
  }
}