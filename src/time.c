#include "time.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

uint64_t time_get_ns(const uint64_t ns) {
  struct timespec tp;
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  return ns + tp.tv_sec * 1000000000 + tp.tv_nsec;
}

uint64_t time_get_us(const uint64_t us) {
  return time_get_ns(us * 1000);
}

uint64_t time_get_ms(const uint64_t ms) {
  return time_get_ns(ms * 1000000);
}

uint64_t time_get_sec(const uint64_t sec) {
  return time_get_ns(sec * 1000000000);
}

static void* time_manager_new_node(struct avl_tree* tree) {
  return contmem_get(&((struct time_manager*) tree)->contmem);
}

static long time_manager_compare(const void* a, const void* b) {
  struct time_manager_node* const x = (struct time_manager_node*) a;
  struct time_manager_node* const y = (struct time_manager_node*) b;
  if(x->time > y->time) {
    return 1;
  } else if(y->time > x->time) {
    return -1;
  } else if(x->id > y->id) {
    return 1;
  } else if(y->id > x->id) {
    return -1;
  } else {
    return 0;
  }
}

static void time_manager_remove_node(struct time_manager* const manager, struct avl_node* const node) {
  void* const last = contmem_last(&manager->contmem);
  int output = contmem_pop(&manager->contmem, node);
  if(output == 1) {
    if(node->parent != NULL) {
      if(node->parent->right == last) {
        node->parent->right = node;
      } else {
        node->parent->left = node;
      }
    } else {
      manager->tree.head = node;
    }
    if(node->left != NULL) {
      node->left->parent = node;
    }
    if(node->right != NULL) {
      node->right->parent = node;
    }
  }
}

static void time_manager_thread(void*);

int time_manager(struct time_manager* const manager, void (*on_timer_expire)(struct time_manager*, void*), const unsigned long max_timeouts, const unsigned long tolerance) {
  struct contmem mem;
  int err = contmem(&mem, max_timeouts, sizeof(struct time_manager_tree_node), tolerance);
  if(err == contmem_out_of_memory) {
    return time_out_of_memory;
  }
  *manager = (struct time_manager) {
    .tree = avl_tree(sizeof(struct time_manager_node), time_manager_new_node, time_manager_compare),
    .contmem = mem,
    .on_timer_expire = on_timer_expire
  };
  err = sem_init(&manager->work, 0, 0);
  if(err != 0) {
    errno = err;
    return time_failure;
  }
  err = sem_init(&manager->amount, 0, 0);
  if(err != 0) {
    (void) sem_destroy(&manager->work);
    errno = err;
    return time_failure;
  }
  err = pthread_mutex_init(&manager->mutex, NULL);
  if(err != 0) {
    (void) sem_destroy(&manager->work);
    (void) sem_destroy(&manager->amount);
    errno = err;
    return time_failure;
  }
  err = threads(&manager->thread);
  if(err != threads_success) {
    (void) sem_destroy(&manager->work);
    (void) sem_destroy(&manager->amount);
    (void) pthread_mutex_destroy(&manager->mutex);
    return time_failure;
  }
  manager->thread.func = time_manager_thread;
  manager->thread.data = manager;
  return time_success;
}

static void time_manager_internal_cancel_timer(struct time_manager* const manager, const uint64_t time, const uint32_t id) {
  time_manager_remove_node(manager, avl_delete(&manager->tree, &((struct time_manager_node){ .time = time, .id = id })));
  if(manager->tree.is_empty == 1) {
    manager->latest_ptr = NULL;
    atomic_store(&manager->latest, 0);
  } else {
    uint64_t* const latest = (uint64_t*) avl_min(&manager->tree);
    manager->latest_ptr = latest;
    atomic_store(&manager->latest, *latest);
  }
  (void) sem_post(&manager->work);
}

#define manager ((struct time_manager*) time_manager_thread_data)

static void time_manager_thread(void* time_manager_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  {
    sigset_t mask;
    (void) sigfillset(&mask);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }
  while(1) {
    check_for_timers:
    (void) sem_wait(&manager->amount);
    while(1) {
      uint64_t time = atomic_load(&manager->latest);
      (void) sem_timedwait(&manager->work, &(struct timespec){ .tv_sec = time / 1000000000, .tv_nsec = time % 1000000000 });
      time = atomic_load(&manager->latest);
      if(time_get_ns(0) >= time) {
        if(time == 0) {
          goto check_for_timers;
        }
        break;
      }
    }
    (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    (void) pthread_mutex_lock(&manager->mutex);
    if(atomic_load(&manager->latest) == 0) {
      (void) pthread_mutex_unlock(&manager->mutex);
    } else {
      struct time_manager_node timeout = *((struct time_manager_node*) manager->latest_ptr);
      const unsigned long tolerance = manager->contmem.tolerance;
      if(timeout.interval != 0) {
        manager->contmem.tolerance = UINTPTR_MAX;
      }
      time_manager_internal_cancel_timer(manager, timeout.time, timeout.id);
      if(timeout.interval != 0) {
        manager->contmem.tolerance = tolerance;
      }
      (void) pthread_mutex_unlock(&manager->mutex);
      manager->on_timer_expire(manager, &timeout);
    }
    (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
  }
}

#undef manager

int time_manager_start(struct time_manager* const manager) {
  switch(threads_add(&manager->thread, 1)) {
    case threads_success: return time_success;
    case threads_out_of_memory: return time_out_of_memory;
    case threads_failure: return time_failure;
  }
  return 0;
}

void time_manager_cancel_timer(struct time_manager* const manager, const uint64_t time, const uint32_t id) {
  (void) pthread_mutex_lock(&manager->mutex);
  time_manager_remove_node(manager, avl_delete(&manager->tree, &((struct time_manager_node){ .time = time, .id = id })));
  if(manager->tree.is_empty == 1) {
    manager->latest_ptr = NULL;
    atomic_store(&manager->latest, 0);
  } else {
    uint64_t* const latest = (uint64_t*) avl_min(&manager->tree);
    manager->latest_ptr = latest;
    atomic_store(&manager->latest, *latest);
  }
  (void) sem_post(&manager->work);
  (void) pthread_mutex_unlock(&manager->mutex);
}

uint32_t time_manager_add_timer(struct time_manager* const manager, const uint64_t time, void (*func)(void*), void* const data, const int is_interval) {
  (void) pthread_mutex_lock(&manager->mutex);
  if(manager->counter == UINT32_MAX) {
    manager->counter = 0;
  }
  const uint32_t id = manager->counter++;
  const int err = avl_insert(&manager->tree, &((struct time_manager_node) { .time = time, .id = id, .interval = is_interval, .func = func, .data = data }), avl_allow_copies);
  if(err == avl_out_of_memory) {
    --manager->counter;
    errno = time_out_of_memory;
    (void) pthread_mutex_unlock(&manager->mutex);
    return 0;
  }
  errno = time_success;
  uint64_t* const latest = (uint64_t*) avl_min(&manager->tree);
  if(manager->latest_ptr == NULL || *latest != *manager->latest_ptr) {
    manager->latest_ptr = latest;
    atomic_store(&manager->latest, *latest);
    (void) sem_post(&manager->work);
  }
  (void) sem_post(&manager->amount);
  (void) pthread_mutex_unlock(&manager->mutex);
  return id;
}

void time_manager_stop(struct time_manager* const manager) {
  threads_shutdown(&manager->thread);
}

void time_manager_free(struct time_manager* const manager) {
  (void) sem_destroy(&manager->work);
  (void) sem_destroy(&manager->amount);
  (void) pthread_mutex_destroy(&manager->mutex);
  contmem_free(&manager->contmem);
  threads_free(&manager->thread);
}