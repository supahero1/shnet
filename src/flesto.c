#include "flesto.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

void vf(const struct flesto* const flesto) {
  for(unsigned long i = 0; i <= flesto->last_part; ++i) {
    for(unsigned long j = 0; j < flesto->parts[i].capacity; ++j) {
      if(j == 0) {
        printf("%hhu", flesto->parts[i].contents[0]);
      } else {
        printf(" %hhu", flesto->parts[i].contents[j]);
      }
    }
    printf(" | ");
  }
  printf("\n");
}

struct flesto flesto(const unsigned long default_max_capacity, const unsigned long delete_after) {
  return (struct flesto) {
    .parts = NULL,
    .last_part = 0,
    .max_amount_of_parts = 0,
    .default_max_capacity = default_max_capacity,
    .delete_after = delete_after
  };
}

int flesto_resize(struct flesto* const flesto, const unsigned long new_size) {
  if(new_size < flesto->max_amount_of_parts) {
    for(unsigned long i = flesto->max_amount_of_parts; i >= new_size; --i) {
      free(flesto->parts[i].contents);
    }
    flesto->last_part = new_size - 1;
  }
  struct flesto_part* const parts = realloc(flesto->parts, sizeof(struct flesto_part) * new_size);
  if(parts == NULL) {
    return flesto_out_of_memory;
  }
  flesto->parts = parts;
  const unsigned long old_size = flesto->max_amount_of_parts;
  flesto->max_amount_of_parts = new_size;
  for(unsigned long i = old_size; i < new_size; ++i) {
    flesto->parts[i].contents = malloc(flesto->default_max_capacity);
    if(flesto->parts[i].contents == NULL) {
      for(--i; i >= old_size; --i) {
        free(flesto->parts[i].contents);
      }
      return flesto_out_of_memory;
    }
    flesto->parts[i].max_capacity = flesto->default_max_capacity;
    flesto->parts[i].capacity = 0;
  }
  return flesto_success;
}

int flesto_add_part(struct flesto* const flesto, char* const contents, const unsigned long max_capacity) {
  struct flesto_part* const parts = realloc(flesto->parts, sizeof(struct flesto_part) * (flesto->max_amount_of_parts + 1));
  if(parts == NULL) {
    return flesto_out_of_memory;
  }
  flesto->parts = parts;
  flesto->parts[flesto->max_amount_of_parts++] = (struct flesto_part) {
    .contents = contents,
    .max_capacity = max_capacity,
    .capacity = 0
  };
  return flesto_success;
}

int flesto_add_parts(struct flesto* const flesto, char** const contents, unsigned long* const max_capacity, const unsigned long amount) {
  struct flesto_part* const parts = realloc(flesto->parts, sizeof(struct flesto_part) * (flesto->max_amount_of_parts + amount));
  if(parts == NULL) {
    return flesto_out_of_memory;
  }
  flesto->parts = parts;
  for(unsigned long i = 0; i < amount; ++i) {
    flesto->parts[flesto->max_amount_of_parts++] = (struct flesto_part) {
      .contents = contents[i],
      .max_capacity = max_capacity[i],
      .capacity = 0
    };
  }
  return flesto_success;
}

int flesto_remove_empty_parts(struct flesto* const flesto) {
  if(flesto->max_amount_of_parts > flesto->last_part + 1) {
    for(unsigned long i = flesto->max_amount_of_parts - 1; i != flesto->last_part; --i) {
      free(flesto->parts[i].contents);
    }
    flesto->max_amount_of_parts = flesto->last_part + 1;
    struct flesto_part* const ptr = realloc(flesto->parts, sizeof(struct flesto_part) * flesto->max_amount_of_parts);
    if(ptr == NULL) {
      return flesto_out_of_memory;
    }
    flesto->parts = ptr;
  }
  return flesto_success;
}

#define flesto_last_part flesto->parts[flesto->last_part]

int flesto_add_item(struct flesto* const flesto, const void* const item, const unsigned long item_size) {
  char* tail;
  if(flesto->max_amount_of_parts == 0) {
    if(flesto_resize(flesto, 1) == flesto_failure) {
      return flesto_out_of_memory;
    }
    tail = flesto->parts[0].contents;
  } else if(flesto_last_part.max_capacity - flesto_last_part.capacity < item_size) {
    if(flesto->last_part + 1 == flesto->max_amount_of_parts && flesto_resize(flesto, flesto->max_amount_of_parts + 1) == flesto_failure) {
      return flesto_out_of_memory;
    }
    tail = flesto->parts[++flesto->last_part].contents;
  } else {
    tail = flesto_last_part.contents + flesto_last_part.capacity;
  }
  (void) memcpy(tail, item, item_size);
  flesto_last_part.capacity += item_size;
  return flesto_success;
}

int flesto_add_items(struct flesto* const flesto, const void* const items, unsigned long amount_of_items, const unsigned long item_size) {
  {
    unsigned long memory = 0;
    const unsigned long total = amount_of_items * item_size;
    for(unsigned long i = flesto->last_part; i < flesto->max_amount_of_parts; ++i) {
      const unsigned long div = (flesto->parts[i].max_capacity - flesto->parts[i].capacity) / item_size;
      if(div >= 1) {
        memory += div * item_size;
        if(memory >= total) {
          break;
        }
      }
    }
    if(memory < total) {
      unsigned long parts_needed = (total - memory) / flesto->default_max_capacity;
      if((total - memory) % flesto->default_max_capacity != 0) {
        ++parts_needed;
      }
      if(flesto_resize(flesto, flesto->max_amount_of_parts + parts_needed) == flesto_failure) {
        return flesto_out_of_memory;
      }
    }
  }
  unsigned long used = 0;
  while(1) {
    const unsigned long how_much_max = (flesto_last_part.max_capacity - flesto_last_part.capacity) / item_size;
    const unsigned long how_much = how_much_max > amount_of_items ? amount_of_items * item_size : how_much_max * item_size;
    (void) memcpy(flesto_last_part.contents + flesto_last_part.capacity, (char*) items + used, how_much);
    used += how_much;
    amount_of_items -= how_much / item_size;
    flesto_last_part.capacity += how_much;
    if(amount_of_items != 0) {
      ++flesto->last_part;
    } else {
      return flesto_success;
    }
  }
}

void flesto_remove_item(struct flesto* const flesto, void* const item, const unsigned long item_size) {
  char* const last_item = flesto_last_part.contents + flesto_last_part.capacity - item_size;
  if((uintptr_t) last_item != (uintptr_t) item) {
    (void) memcpy(item, last_item, item_size);
  }
  flesto_last_part.capacity -= item_size;
  if(flesto_last_part.capacity == 0 && flesto->last_part != 0) {
    --flesto->last_part;
  }
}

int flesto_delete_after(struct flesto* const flesto) {
  if(flesto->max_amount_of_parts > flesto->last_part + 1 && flesto_last_part.max_capacity - flesto_last_part.capacity >= flesto->delete_after) {
    free(flesto->parts[flesto->last_part + 1].contents);
    struct flesto_part* const ptr = realloc(flesto->parts, sizeof(struct flesto_part) * --flesto->max_amount_of_parts);
    if(ptr == NULL) {
      return flesto_out_of_memory;
    }
    flesto->parts = ptr;
  }
  return flesto_success;
}

#undef flesto_last_part

void flesto_pop(struct flesto* const flesto, unsigned long amount) {
  for(unsigned long i = flesto->last_part; i >= 0; --i) {
    if(amount < flesto->parts[i].capacity) {
      flesto->parts[i].capacity -= amount;
      if(flesto->parts[i].capacity == 0 && flesto->last_part != 0) {
        --flesto->last_part;
      }
      return;
    } else {
      amount -= flesto->parts[i].capacity;
      flesto->parts[i].capacity = 0;
      if(flesto->last_part != 0) {
        --flesto->last_part;
      } else {
        return;
      }
    }
  }
}

int flesto_remove_items(struct flesto* const flesto, void* const items, const unsigned long part, const unsigned long offset, const unsigned long amount_of_items, const unsigned long item_size) {
  unsigned long dest_part;
  unsigned long dest_offset;
  if(items == NULL) {
    dest_part = part;
    dest_offset = offset;
  } else {
    if(flesto_ptr_to_idx(flesto, items, &dest_part, &dest_offset, 0, flesto_right) == flesto_failure) {
      return flesto_failure;
    }
  }
  unsigned long src_part;
  unsigned long src_offset;
  const unsigned long total = amount_of_items * item_size;
  if(flesto_idx_to_ptr(flesto, dest_part, dest_offset, total, flesto_right, &src_part, &src_offset) != NULL) {
    const unsigned long trail = flesto_trailing_items(flesto, src_part, src_offset);
    if(trail >= total) {
      (void) flesto_idx_to_ptr(flesto, flesto->last_part, flesto->parts[flesto->last_part].capacity, total, flesto_left, &src_part, &src_offset);
      flesto_copy_items(flesto, dest_part, dest_offset, src_part, src_offset, total, item_size);
    } else {
      flesto_copy_items(flesto, dest_part, dest_offset, src_part, src_offset, trail, item_size);
    }
  }
  flesto_pop(flesto, total);
  return flesto_success;
}

unsigned long flesto_trailing_items(const struct flesto* const flesto, unsigned long part, const unsigned long offset) {
  unsigned long res = flesto->parts[part].capacity - offset;
  for(++part; part <= flesto->last_part; ++part) {
    res += flesto->parts[part].capacity;
  }
  return res;
}

void* flesto_idx_to_ptr(const struct flesto* const flesto, unsigned long ref_part, unsigned long ref_offset, unsigned long idx, const int dir, unsigned long* const part, unsigned long* const offset) {
  if(dir == flesto_right) {
    while(1) {
      if(ref_offset + idx < flesto->parts[ref_part].capacity) {
        if(part != NULL) {
          *part = ref_part;
        }
        ref_offset += idx;
        if(offset != NULL) {
          *offset = ref_offset;
        }
        break;
      } else {
        idx -= flesto->parts[ref_part].capacity - ref_offset;
        if(ref_part++ == flesto->last_part) {
          return NULL;
        }
        ref_offset = 0;
      }
    }
  } else {
    while(1) {
      if(ref_offset >= idx) {
        if(part != NULL) {
          *part = ref_part;
        }
        ref_offset -= idx;
        if(offset != NULL) {
          *offset = ref_offset;
        }
        break;
      } else {
        idx -= ref_offset;
        if(ref_part-- == 0) {
          return NULL;
        }
        ref_offset = flesto->parts[ref_part].capacity;
      }
    }
  }
  return flesto->parts[ref_part].contents + ref_offset;
}

int flesto_ptr_to_idx(const struct flesto* const flesto, const void* const ptr, unsigned long* const part, unsigned long* const offset, const unsigned long tip, const int dir) {
  if(dir == flesto_right) {
    for(unsigned long i = tip; i <= flesto->last_part; ++i) {
      if((uintptr_t) flesto->parts[i].contents <= (uintptr_t) ptr && (uintptr_t) flesto->parts[i].contents + flesto->parts[i].capacity > (uintptr_t) ptr) {
        *part = i;
        *offset = (uintptr_t) ptr - (uintptr_t) flesto->parts[i].contents;
        return flesto_success;
      }
    }
    return flesto_failure;
  } else {
    for(unsigned long i = tip; i >= 0; --i) {
      if((uintptr_t) flesto->parts[i].contents <= (uintptr_t) ptr && (uintptr_t) flesto->parts[i].contents + flesto->parts[i].capacity > (uintptr_t) ptr) {
        *part = i;
        *offset = (uintptr_t) ptr - (uintptr_t) flesto->parts[i].contents;
        return flesto_success;
      } else if(i == 0) {
        break;
      }
    }
    return flesto_failure;
  }
}

void flesto_copy_items(const struct flesto* const flesto, unsigned long dest_part, unsigned long dest_offset, unsigned long src_part, unsigned long src_offset, unsigned long amount_of_items, const unsigned long item_size) {
  while(1) {
    const unsigned long max_paste = (flesto->parts[dest_part].max_capacity - dest_offset) / item_size;
    const unsigned long max_copy  = (flesto->parts[ src_part].max_capacity -  src_offset) / item_size;
    const unsigned long copy_len = max_paste > max_copy ? max_copy : max_paste;
    const unsigned long real_len = amount_of_items > copy_len ? copy_len : amount_of_items;
    (void) memcpy(flesto->parts[dest_part].contents + dest_offset, flesto->parts[src_part].contents + src_offset, real_len * item_size);
    amount_of_items -= real_len;
    if(amount_of_items == 0) {
      return;
    }
    if(real_len == max_paste) {
      ++dest_part;
      dest_offset = 0;
      src_offset += real_len * item_size;
    } else if(real_len == max_copy) {
      ++src_part;
      src_offset = 0;
      dest_offset += real_len * item_size;
    } else {
      ++dest_part;
      dest_offset = 0;
      ++src_part;
      src_offset = 0;
    }
  }
}

unsigned long flesto_amount_of_items(const struct flesto* const flesto) {
  unsigned long res = 0;
  for(unsigned long i = 0; i <= flesto->last_part; ++i) {
    res += flesto->parts[i].capacity;
  }
  return res;
}

int flesto_check_address(const struct flesto* const flesto, const void* const ptr) {
  for(unsigned long i = 0; i <= flesto->last_part; ++i) {
    if((uintptr_t) flesto->parts[i].contents <= (uintptr_t) ptr && (uintptr_t) flesto->parts[i].contents + flesto->parts[i].capacity > (uintptr_t) ptr) {
      return flesto_success;
    }
  }
  return flesto_failure;
}

int flesto_check_address1(const struct flesto* const flesto, const void* const ptr) {
  for(unsigned long i = 0; i <= flesto->last_part; ++i) {
    if((uintptr_t) flesto->parts[i].contents <= (uintptr_t) ptr && (uintptr_t) flesto->parts[i].contents + flesto->parts[i].max_capacity > (uintptr_t) ptr) {
      return flesto_success;
    }
  }
  return flesto_failure;
}

int flesto_check_address2(const struct flesto* const flesto, const void* const ptr) {
  for(unsigned long i = 0; i < flesto->max_amount_of_parts; ++i) {
    if((uintptr_t) flesto->parts[i].contents <= (uintptr_t) ptr && (uintptr_t) flesto->parts[i].contents + flesto->parts[i].capacity > (uintptr_t) ptr) {
      return flesto_success;
    }
  }
  return flesto_failure;
}

int flesto_check_address3(const struct flesto* const flesto, const void* const ptr) {
  for(unsigned long i = 0; i < flesto->max_amount_of_parts; ++i) {
    if((uintptr_t) flesto->parts[i].contents <= (uintptr_t) ptr && (uintptr_t) flesto->parts[i].contents + flesto->parts[i].max_capacity > (uintptr_t) ptr) {
      return flesto_success;
    }
  }
  return flesto_failure;
}

void flesto_free(struct flesto* const flesto) {
  for(unsigned long i = 0; i < flesto->max_amount_of_parts; ++i) {
    free(flesto->parts[i].contents);
  }
  free(flesto->parts);
  flesto->parts = NULL;
  flesto->max_amount_of_parts = 0;
  flesto->last_part = 0;
}