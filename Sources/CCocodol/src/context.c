#include <stdlib.h>
#include <string.h>

#include "context.h"

#define INITIAL_CAPACITY 16

void context_init(Context* self, const char* source) {
  self->source = source;

  // Initialize the node vector.
  self->nodes = malloc(INITIAL_CAPACITY * sizeof(Node));
  self->node_count = 0;
  self->node_capacity = INITIAL_CAPACITY;
}

void context_deinit(Context* self) {
  self->source = NULL;

  // Deinitialize the node vector.
  for (size_t i = 0; i < self->node_count; ++i) {
    node_deinit(self->nodes + i);
  }
  free(self->nodes);
  self->nodes = NULL;
  self->node_count = 0;
  self->node_capacity = 0;
}

void context_resize_node_buffer(Context* self) {
  size_t new_capacity = self->node_capacity * 2;
  Node* new_buffer = malloc(new_capacity * sizeof(Node));
  memcpy(new_buffer, self->nodes, self->node_count * sizeof(Node));
  free(self->nodes);
  self->nodes = new_buffer;
  self->node_capacity = new_capacity;
}

size_t context_new_node(Context* self) {
  if (self->node_count == self->node_capacity) {
    context_resize_node_buffer(self);
  }

  size_t index = self->node_count;
  self->node_count++;
  return index;
}

void context_delete_node(Context* self, size_t index) {
}
