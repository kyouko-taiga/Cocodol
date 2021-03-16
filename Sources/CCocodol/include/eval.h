#ifndef COCODOL_EVAL_H
#define COCODOL_EVAL_H

#include "common.h"
#include "symtable.h"
#include "value.h"

#define VALUE_STACK_SIZE 1024

struct EvalFrame;

/// The state of an interpreter.
typedef struct EvalState {

  /// The context of the program to interpreter.
  struct Context* context;

  /// The exit status of the interpreter.
  int status;

  /// The table of global symbols.
  SymTable globals;

  /// A pointer to the current local frame.
  struct EvalFrame* frame;

  /// The current index in the value stack.
  size_t value_index;

  /// The value stack.
  RuntimeValue value_stack[VALUE_STACK_SIZE];

} EvalState;

/// A runtime error.
typedef struct EvalError {

  /// The start location of the error in the program source.
  size_t start;

  /// The end location of the error in the program source.
  size_t end;

  /// The error message.
  const char* message;

} EvalError;

/// Initializes an interpreter's state.
void eval_init(EvalState*, struct Context*);

/// Deinitializes an interpreter's state.
void eval_deinit(EvalState*);

/// Evaluates the given program.
int eval_program(EvalState*, const NodeID* decls, size_t decl_count, EvalErrorCallback);

#endif
