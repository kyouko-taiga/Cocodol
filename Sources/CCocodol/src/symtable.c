#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "symtable.h"
#include "utils.h"

#define INITIAL_CAPACITY  16
#define LOAD_FACTOR       0.75

#define TOMB_BIT          1
#define USED_BIT          2
#define is_tomb(key)      ((key & TOMB_BIT) == TOMB_BIT)
#define is_used(key)      ((key & USED_BIT) == USED_BIT)
#define is_free(key)      ((key & (TOMB_BIT | USED_BIT)) == 0)
#define extract_ptr(key)  ((const char*)(key & ~(TOMB_BIT | USED_BIT)))

typedef struct SymTableEntry {
  uintptr_t key;
  void*     val;
  uint64_t  h;
} SymTableEntry;

void symtable_init(SymTable* self) {
  // Initialize the buckets.
  size_t length = INITIAL_CAPACITY * sizeof(SymTableEntry);
  self->buckets = malloc(length);
  memset(self->buckets, 0, length);

  self->count = 0;
  self->capacity = INITIAL_CAPACITY;
}

void symtable_deinit(SymTable* self, bool delete_keys) {
  // Free the keys if requested.
  if (delete_keys) {
    for (size_t i = 0; i < self->capacity; ++i) {
      if (!is_free(self->buckets[i].key)) {
        free((char*)extract_ptr(self->buckets[i].key));
      }
    }
  }

  free(self->buckets);
  self->buckets = NULL;
  self->count = 0;
  self->capacity = 0;
}

/// Resizes the table, doubling its capacity.
void symtable_resize(SymTable* self) {
  // Create a new bucket array.
  size_t new_capacity = self->capacity * 2;
  SymTableEntry* new_buckets = malloc(new_capacity * sizeof(SymTableEntry));
  memset(new_buckets, 0, new_capacity * sizeof(SymTableEntry));

  // Re-insert the entries.
  size_t new_count = 0;
  for (size_t i = 0; i < self->capacity; ++i) {
    if (is_used(self->buckets[i].key)) {
      size_t position = self->buckets[i].h % new_capacity;
      while (!is_free(new_buckets[position].key)) {
        position = (position + 1) % new_capacity;
      }
      new_buckets[position] = self->buckets[i];
      new_count++;
    }
  }

  // Substitute the old bucket array for the new one.
  free(self->buckets);
  self->buckets = new_buckets;
  self->count = new_count;
  self->capacity = new_capacity;
}

/// Finds the entry for the given key.
SymTableEntry* symtable_search(SymTable* self, const char* key) {
  // Search for `key`.
  size_t h = fnv1_hash_string(key);
  size_t pos = h % self->capacity;

  while (!is_free(self->buckets[pos].key)) {
    // Skip tombstones.
    if (is_tomb(self->buckets[pos].key)) { continue; }

    // Check for equality.
    if ((self->buckets[pos].h == h) &&
        (strcmp(extract_ptr(self->buckets[pos].key), key) == 0)) {
      return &self->buckets[pos];
    }

    // Move to the next position.
    pos = (pos + 1) % self->capacity;
  }

  return NULL;
}

void* symtable_insert(SymTable* self, const char* key, void* val) {
  // Resize the table if we reached its load factor.
  if (((float)self->count / (float)self->capacity) > LOAD_FACTOR) {
    symtable_resize(self);
  }

  // Search for a free bucket.
  size_t h = fnv1_hash_string(key);
  size_t pos = h % self->capacity;
  size_t insert_position = self->capacity;

  while (!is_free(self->buckets[pos].key)) {
    if (is_tomb(self->buckets[pos].key)) {
      // Remember the position of the first tombstone.
      if (insert_position == self->capacity) {
        insert_position = pos;
      }
    } else if ((self->buckets[pos].h == h) &&
               (strcmp(extract_ptr(self->buckets[pos].key), key) == 0)) {
      // `key` is already in the map.
      return self->buckets[pos].val;
    }

    // Move to the next position.
    pos = (pos + 1) % self->capacity;
  }

  // Insert the new entry.
  if (insert_position == self->capacity) {
    insert_position = pos;
    self->count++;
  }
  self->buckets[pos].key = (uintptr_t)key | USED_BIT;
  self->buckets[pos].val = val;
  self->buckets[pos].h   = h;

  return NULL;
}

void* symtable_update(SymTable* self, const char* key, void* value) {
  SymTableEntry* entry = symtable_search(self, key);
  if (entry != NULL) {
    assert(!is_tomb(entry->key));
    void* val = entry->val;
    entry->val = value;
    return val;
  }

  return symtable_insert(self, key, value);
}

void* symtable_remove(SymTable* self, const char* key) {
  SymTableEntry* entry = symtable_search(self, key);
  if (entry != NULL) {
    // Create a tombstone.
    assert(!is_tomb(entry->key));
    entry->key = entry->key & ~USED_BIT;
    return entry->val;
  } else {
    return NULL;
  }
}

void* symtable_get(SymTable* self, const char* key) {
  SymTableEntry* entry = symtable_search(self, key);
  if (entry != NULL) {
    assert(!is_tomb(entry->key));
    return entry->val;
  } else {
    return NULL;
  }
}

size_t symtable_entry_count(SymTable* self) {
  size_t count = 0;
  for (size_t i = 0; i < self->capacity; ++i) {
    if (is_used(self->buckets[i].key)) {
      count++;
    }
  }
  return count;
}

size_t symtable_map(SymTable* self, void** results, void*(*transform)(const char*, void*)) {
  size_t count = 0;
  for (size_t i = 0; i < self->capacity; ++i) {
    if (is_used(self->buckets[i].key)) {
      void* rv = transform(extract_ptr(self->buckets[i].key), self->buckets[i].val);
      if (results) {
        results[count] = rv;
        count++;
      }
    }
  }
  return count;
}

void symtable_foreach(SymTable* self, void* user, void(*action)(const char*, void*, void*)) {
  for (size_t i = 0; i < self->capacity; ++i) {
    if (is_used(self->buckets[i].key)) {
      action(extract_ptr(self->buckets[i].key), self->buckets[i].val, user);
    }
  }
}
