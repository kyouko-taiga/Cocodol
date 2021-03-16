#ifndef COCODOL_CONTEXT_H
#define COCODOL_CONTEXT_H

#include "ast.h"
#include "common.h"

/// A structure that holds AST nodes along with other long-lived metadata.
typedef struct Context {

  /// The input string representing the program source.
  const char* source;

  /// The buffer containing the AST nodes managed by this context.
  Node* nodes;

  /// The number of nodes in the context.
  size_t node_count;

  /// The capacity of the node buffer.
  size_t node_capacity;

} Context;

/// Initializes a context.
void context_init(Context*, const char* source);

/// Deinitializes a context.
void context_deinit(Context*);

/// Allocates a new node and returns its index in the context.
///
/// Calling this function may invalidate all existing node pointers.
NodeID context_new_node(Context*);

/// Deallocate the node at the given index.
void context_delete_node(Context*, NodeID index);

/// Returns a pointer to the node with the specified ID.
///
/// The returned pointer is not guaranteed after the node list is mutated. In particular, a call to
/// either `context_new_node` or `context_delete_node` may invalidate it.
static inline Node* context_get_nodeptr(Context* self, NodeID index) {
  return self->nodes + index;
}

#endif
