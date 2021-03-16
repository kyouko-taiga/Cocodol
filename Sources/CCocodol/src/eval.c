#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"
#include "builtins.h"
#include "context.h"
#include "eval.h"

#define MAX_CAPTURE_COUNT 64

#define EVAL_STATUS_OK  0
#define EVAL_STATUS_BRK 1
#define EVAL_STATUS_ERR -1

#define eval_stack_top(self)     (self->value_stack[(self)->value_index - 1])
#define eval_stack(self, offset) (self->value_stack[(self)->value_index - 1 + (offset)])

size_t capture_list(EvalState* self, NodeID fun_index, Token** symv);
void   eval_pop_frame(EvalState* self);
void*  free_symbol_entry(const char* key, void* value);
void   value_copy(RuntimeValue* dst, RuntimeValue* src);

/// An value identifier.
typedef struct {

  /// The name of the identifier, as a null-terminated character string.
  char*  name;

  /// The index at which the identifier starts in the source input.
  size_t start;

  /// The index at which the identifier ends in the source input.
  size_t end;

} Ident;

/// Initializes the value of an identifier with the given token.
void ident_init(Ident* self, Context* context, Token* token) {
  assert(token->kind == tk_name);

  size_t len = token->end - token->start;
  self->name = malloc(len + 1);
  memcpy(self->name, context->source + token->start, len);
  self->name[len] = 0;

  self->start = token->start;
  self->end = token->end;
}

/// The kind of a frame.
typedef enum {
  ef_function ,
  ef_anonymous,
} FrameKind;

/// A local frame.
typedef struct EvalFrame {

  /// The frame's kind.
  FrameKind kind;

  /// The index of the interpreter's value stack at the beginning of the frame.
  size_t value_index;

  /// The table of local symbols.
  SymTable locals;

  /// A pointer to the previous frame.
  struct EvalFrame* prev;

} EvalFrame;

/// The evaluation environment of the AST walker.
typedef struct {

  EvalState* state;

  EvalErrorCallback report_diag;

} EvalEnv;

/// Drops a runtime value, disposing of its associated memory if necessary.
void drop(RuntimeValue* value) {
  switch (value->kind) {
    case rv_function: {
      SymTable* fun_env = value->bits.function_v.env;
      if (fun_env != NULL) {
        symtable_map(fun_env, NULL, free_symbol_entry);
        symtable_deinit(fun_env, true);
        free(fun_env);
      }
      break;
    }

    default:
      break;
  }

  value->kind = rv_junk;
}

/// Frees the values of a symbol table.
///
/// Value pointers are allocated in the evaluation loop, when new symbols are created.
void* free_symbol_entry(const char* key, void* value) {
  drop((RuntimeValue*)value);
  free(value);
  return NULL;
}

/// Copies the contents of a symbol table.
void copy_symbol_entry(const char* key, void* value, void* user) {
  size_t len = strlen(key);
  char* new_key = malloc(len + 1);
  strcpy(new_key, key);

  RuntimeValue* new_value = malloc(sizeof(RuntimeValue));
  new_value->kind = rv_junk;
  value_copy(new_value, (RuntimeValue*)value);

  SymTable* other = (SymTable*)user;
  symtable_insert(other, new_key, new_value);
}

void eval_init(EvalState* self, Context* context) {
  self->context = context;
  self->status = EVAL_STATUS_OK;
  symtable_init(&self->globals);
  self->frame = NULL;
  self->value_index = 0;
}

void eval_deinit(EvalState* self) {
  self->context = NULL;
  self->status = EVAL_STATUS_OK;

  // Deinitialize the globals.
  symtable_map   (&self->globals, NULL, free_symbol_entry);
  symtable_deinit(&self->globals, true);
  while (self->frame != NULL) {
    eval_pop_frame(self);
  }

  // Clear the value stack.
  self->value_index = 0;
}

// ------------------------------------------------------------------------------------------------
// MARK: Runtime
// ------------------------------------------------------------------------------------------------

/// Pushes a new stack from.
EvalFrame* eval_push_frame(EvalState* self, FrameKind frame_kind) {
  EvalFrame* new_frame = malloc(sizeof(EvalFrame));

  symtable_init(&new_frame->locals);
  new_frame->kind = frame_kind;
  new_frame->value_index = self->value_index;
  new_frame->prev = self->frame;
  self->frame = new_frame;

  return new_frame;
}

/// Pops the current stack frame.
void eval_pop_frame(EvalState* self) {
  if (self->frame == NULL) { return; }
  EvalFrame* frame = self->frame;
  self->frame = frame->prev;

  symtable_map   (&frame->locals, NULL, free_symbol_entry);
  symtable_deinit(&frame->locals, true);
  return;
}

/// Inserts a new symbol in the given table.
///
/// This function returns `true` if the new symbol was successfully inserted. Otherwise, it returns
/// `false` if the symbol is invalid (e.g., it's a reserved identifier) or if it already exists.
bool insert_symbol(EvalState* self,
                   SymTable* table,
                   Ident* ident,
                   RuntimeValue* value,
                   EvalErrorCallback report_diag)
{
  if (strcmp(ident->name, "print") == 0) {
    EvalError error = {
      ident->start, ident->end,
      "invalid declaration, 'print' is a reserved identifier"
    };
    report_diag(error, self);
    return false;
  }

  if (symtable_insert(table, ident->name, value)) {
    char msg[255] = { 0 };
    strcpy(msg, "duplicate declaration '");
    strcat(msg, ident->name);
    strcat(msg, "'");
    EvalError error = { ident->start, ident->end, msg };
    report_diag(error, self);
    return false;
  } else {
    return true;
  }
}

/// Copies a runtime value.
void value_copy(RuntimeValue* dst, RuntimeValue* src) {
  drop(dst);
  switch (src->kind) {
    case rv_junk:
    case rv_bool:
    case rv_integer:
    case rv_float:
    case rv_lazy:
    case rv_print:
      *dst = *src;
      break;

    case rv_function:
      dst->kind = rv_function;
      dst->bits.function_v.decl = src->bits.function_v.decl;
      if (src->bits.function_v.env != NULL) {
        dst->bits.function_v.env = malloc(sizeof(SymTable));
        symtable_init(dst->bits.function_v.env);
        symtable_foreach(src->bits.function_v.env, dst->bits.function_v.env, copy_symbol_entry);
      } else {
        dst->bits.function_v.env = NULL;
      }
      break;
  }
}

// ------------------------------------------------------------------------------------------------
// MARK: Sema
// ------------------------------------------------------------------------------------------------

/// Looks up an identifier.
RuntimeValue* ident_lookup(EvalState* self, Ident* ident, EvalErrorCallback report_diag) {
  // Search the locals.
  EvalFrame* frame = self->frame;
  while (frame != NULL) {
    RuntimeValue* value = (RuntimeValue*)symtable_get(&frame->locals, ident->name);
    if (value != NULL) {
      return value;
    } else if (frame->kind != ef_function) {
      frame = frame->prev;
    } else {
      frame = NULL;
    }
  }

  // Search a global symbol.
  RuntimeValue* value = (RuntimeValue*)symtable_get(&self->globals, ident->name);
  if (value != NULL) {
    return value;
  }

  char msg[255] = { 0 };
  strcpy(msg, "undefined identifier '");
  strcat(msg, ident->name);
  strcat(msg, "'");
  EvalError error = { ident->start, ident->end, msg };
  report_diag(error, self);
  return NULL;
}

typedef struct {
  EvalState* state;
  NodeID fun_index;
  NodeID body_index;
  NodeID scope;
  size_t symc;
  Token** symv;
} CaptureVisitorEnv;

bool ident_is_local(CaptureVisitorEnv* env, Token* lhs) {
  EvalState* self = env->state;
  Token* rhs = NULL;

  // Search within the function's local scopes.
  NodeID scope_index = env->scope;
  while (scope_index != ~0) {
    // Get the declaration list of the current scope.
    Node* scope = context_get_nodeptr(self->context, scope_index);
    assert(scope->kind == nk_brace_stmt);

    // Search within the current scope.
    DeclList* list = scope->bits.brace_stmt.last_decl;
    while (list) {
      Node* decl = context_get_nodeptr(self->context, list->decl);
      switch (decl->kind) {
        case nk_var_decl: {
          rhs = &decl->bits.var_decl.name;
          if (token_text_equal(self->context, lhs, rhs)) {
            // It's a reference to a local declaration; no further action required.
            return true;
          }
          break;
        }

        default:
          break;
      }

      list = list->prev;
    }

    // Move to the parent scope, unless we reached the function.
    if (scope_index != env->body_index) {
      scope_index = scope->bits.brace_stmt.parent;
    } else {
      scope_index = ~0;
    }
  }

  // Check the function's parameters.
  Node* fun_decl = context_get_nodeptr(self->context, env->fun_index);
  for (size_t i = 0; i < fun_decl->bits.fun_decl.paramc; ++i) {
    rhs = &fun_decl->bits.fun_decl.paramv[i];
    if (token_text_equal(self->context, lhs, rhs)) {
      // It's a reference to a parameter; no further action required.
      return true;
    }
  }

  // Check the function's name.
  rhs = &fun_decl->bits.fun_decl.name;
  if (token_text_equal(self->context, lhs, rhs)) {
    // It's a recursive reference; no further action required.
    return true;
  }

  // The symbol is defined externally.
  return false;
}

bool capture_visitor(NodeID index, NodeKind kind, bool pre, void* user) {
  CaptureVisitorEnv* env = (CaptureVisitorEnv*)user;
  EvalState* self = env->state;

  switch (kind) {
    case nk_fun_decl: {
      // Build the nested function's capture list.
      Token* symv[MAX_CAPTURE_COUNT];
      size_t symc = capture_list(self, index, symv);

      // Check whether the nested function's captured symbols also escape the current function.
      for (size_t i = 0; i < symc; ++i) {
        if (!ident_is_local(env, symv[i])) {
          assert(env->symc < MAX_CAPTURE_COUNT);
          env->symv[env->symc] = symv[i];
          env->symc++;
        }
      }

      assert(pre);
      return false;
    }

    case nk_obj_decl:
      assert(false);

    case nk_declref_expr: {
      // Lookup the symbol statically.
      Node* expr = context_get_nodeptr(self->context, index);
      Token* lhs = &expr->bits.declref_expr;
      if (!ident_is_local(env, lhs)) {
        assert(env->symc < MAX_CAPTURE_COUNT);
        env->symv[env->symc] = lhs;
        env->symc++;
      }

      assert(pre);
      return false;
    }

    case nk_brace_stmt:
      if (pre) {
        env->scope = index;
      } else {
        env->scope = context_get_nodeptr(self->context, index)->bits.brace_stmt.parent;
      }
      return true;

    default:
      // assert(pre);
      return true;
  }
}

/// Returns the list of variables occurring free in the given function declaration.
size_t capture_list(EvalState* self, NodeID fun_index, Token** symv) {
  NodeID body_index = context_get_nodeptr(self->context, fun_index)->bits.fun_decl.body;
  CaptureVisitorEnv env = {
    self      , // state of the interpreter
    fun_index , // index of the declaration being visited
    body_index, // index of the function's body
    body_index, // initial search scope
    0         , // identifier count
    symv };

  // Visit the function declaration.
  node_walk(body_index, self->context, &env, capture_visitor);
  return env.symc;
}

// ------------------------------------------------------------------------------------------------
// MARK: Debug helpers
// ------------------------------------------------------------------------------------------------

/// Returns a character string describing the type of the given value.
const char* value_type_name(RuntimeValue* value) {
  switch (value->kind) {
    case rv_junk    : return "Junk";
    case rv_bool    : return "Bool";
    case rv_integer : return "Int";
    case rv_float   : return "Float";
    case rv_lazy    :
    case rv_print   :
    case rv_function: return "Function";
  }
}

/// Evaluates the built-in `print` function.
void eval_print(RuntimeValue* value) {
  switch (value->kind) {
    case rv_junk:
      printf("$junk\n");
      break;

    case rv_bool:
      fputs(value->bits.bool_v ? "true\n" : "false\n", stdout);
      break;

    case rv_integer:
      printf("%li\n", value->bits.integer_v);
      break;

    case rv_float:
      printf("%f\n", value->bits.float_v);
      break;

    case rv_lazy:
    case rv_print:
    case rv_function:
      printf("$function\n");
      break;
  }
}

// ------------------------------------------------------------------------------------------------
// MARK: Eval loop
// ------------------------------------------------------------------------------------------------

/// Evaluates a l-value.
RuntimeValue* eval_lvalue(EvalState* self, NodeID index, EvalErrorCallback report_diag) {
  Node* node = context_get_nodeptr(self->context, index);
  switch (node->kind) {
    case nk_declref_expr: {
      Ident ident;
      ident_init(&ident, self->context, &node->bits.declref_expr);
      RuntimeValue* value = ident_lookup(self, &ident, report_diag);
      free(ident.name);
      return value;
    }

    default:
      return NULL;
  }
}

/// Evaluates a node.
bool eval_node(NodeID index, NodeKind kind, bool pre, void* user) {
  EvalEnv* env = (EvalEnv*)(user);
  EvalState* self = env->state;
  Node* node = context_get_nodeptr(self->context, index);

  // Exit if evaluation failed.
  if (self->status == EVAL_STATUS_ERR) { return false; }
  assert(self->status == EVAL_STATUS_OK);

  // Some nodes must be handled in the "pre" phase.
  if (pre) {
    switch (kind) {
      case nk_fun_decl: {
        // Create the function object.
        RuntimeValue* fun_val = malloc(sizeof(RuntimeValue));
        fun_val->kind = rv_function;
        fun_val->bits.function_v.decl = index;
        fun_val->bits.function_v.env = NULL;

        // Store the function in the locals.
        Ident ident;
        ident_init(&ident, self->context, &node->bits.fun_decl.name);

        SymTable* table = &self->frame->locals;
        if (!insert_symbol(self, table, &ident, fun_val, env->report_diag)) {
          free(ident.name);
          free(fun_val);
          self->status = EVAL_STATUS_ERR;
          return false;
        }

        // Build the function's capture list.
        Token* symv[MAX_CAPTURE_COUNT];
        size_t symc = capture_list(self, index, symv);

        // Create the function's environment.
        if (symc > 0) {
          // Allocate a symbol table.
          SymTable* fun_env = malloc(sizeof(SymTable));
          symtable_init(fun_env);
          fun_val->bits.function_v.env = fun_env;

          // Copy each captured symbol.
          for (size_t i = 0; i < symc; ++i) {
            ident_init(&ident, self->context, symv[i]);
            RuntimeValue* value = ident_lookup(self, &ident, env->report_diag);

            // Make sure the captured parameter exists.
            if (value == NULL) {
              free(fun_val);
              self->status = EVAL_STATUS_ERR;
              return false;
            }

            // Capture the parameter if it's not already in the environment.
            if (symtable_get(fun_env, ident.name) != NULL) {
              free(ident.name);
              continue;
            } else {
              RuntimeValue* param = malloc(sizeof(RuntimeValue));
              value_copy(param, value);
              symtable_insert(fun_env, ident.name, param);
            }
          }
        }

        return false;
      }

      case nk_binary_expr: {
        // Non-assignment expressions are handled in the "post" phase only.
        if (node->bits.binary_expr.op.kind != tk_assign) { return true; }

        // Evaluate the l-value first.
        RuntimeValue* lvalue = eval_lvalue(self, node->bits.binary_expr.lhs, env->report_diag);
        if (lvalue == NULL) {
          self->status = EVAL_STATUS_ERR;
          return false;
        }

        // Evaluate the assignment.
        node_walk(node->bits.binary_expr.rhs, self->context, user, eval_node);
        value_copy(lvalue, &eval_stack_top(self));
        drop(&eval_stack_top(self));
        self->value_index--;
        return false;
      }

      case nk_brace_stmt: {
        // A brace statement pushes a new frame before its statements are evaluated. The frame is
        // popped in the "post" phase.
        eval_push_frame(self, ef_anonymous);
        return true;
      }

      case nk_if_stmt: {
        // The condition is evaluated first, determining the branch to execute next.
        node_walk(node->bits.if_stmt.cond, self->context, user, eval_node);

        // Make sure we've got a Boolean.
        if (eval_stack_top(self).kind != rv_bool) {
          Node* cond_node = context_get_nodeptr(self->context, node->bits.if_stmt.cond);
          EvalError error = {
            cond_node->start, cond_node->end,
            "'if' condition must evaluate to a Boolean value"
          };
          env->report_diag(error, self);
          self->status = EVAL_STATUS_ERR;
          return false;
        }

        // Choose the branch to execute next.
        bool enter_look = eval_stack_top(self).bits.bool_v;
        drop(&eval_stack_top(self));
        self->value_index--;

        if (enter_look) {
          node_walk(node->bits.if_stmt.then_, self->context, user, eval_node);
        } else if (node->bits.if_stmt.else_ != ~0) {
          node_walk(node->bits.if_stmt.else_, self->context, user, eval_node);
        }
        return false;
      }

      case nk_while_stmt: {
        while (true) {
          // Evaluate the condition at the loop's entry.
          node_walk(node->bits.while_stmt.cond, self->context, user, eval_node);

          // Make sure we've got a Boolean.
          EvalFrame* frame = self->frame;
          if (eval_stack_top(self).kind != rv_bool) {
            Node* cond_node = context_get_nodeptr(self->context, node->bits.if_stmt.cond);
            EvalError error = {
              cond_node->start, cond_node->end,
              "'while' condition must evaluate to a Boolean value"
            };
            env->report_diag(error, self);
            self->status = EVAL_STATUS_ERR;
            return false;
          }

          bool enter_then = eval_stack_top(self).bits.bool_v;
          drop(&eval_stack_top(self));
          self->value_index--;
          if (!enter_then) { return false; }

          // Execute the body of the loop.
          node_walk(node->bits.while_stmt.body, self->context, user, eval_node);
          while (self->frame != frame) {
            eval_pop_frame(self);
          }

          // Exit the loop if we executed a break statement.
          if (self->status == EVAL_STATUS_BRK) {
            self->status = EVAL_STATUS_OK;
            return false;
          }
        }
      }

      default:
        // Other nodes are handled entirely in the "post" phase.
        return true;
    }
  }

  // Handle the "post" phase.
  switch (kind) {
    case nk_top_decl:
      assert(self->value_index == 0);
      return true;

    case nk_var_decl: {
      Ident ident;
      ident_init(&ident, self->context, &node->bits.var_decl.name);

      RuntimeValue* value = malloc(sizeof(RuntimeValue));
      if (node->bits.var_decl.initializer != ~0) {
        value_copy(value, &eval_stack_top(self));
        drop(&eval_stack_top(self));
        self->value_index--;
      } else {
        value->kind = rv_junk;
      }

      SymTable* table = &self->frame->locals;
      if (!insert_symbol(self, table, &ident, value, env->report_diag)) {
        free(ident.name);
        self->status = EVAL_STATUS_ERR;
        return false;
      } else {
        return true;
      }
    }

    case nk_fun_decl:
      return true;

    case nk_declref_expr: {
      Ident ident;
      ident_init(&ident, self->context, &node->bits.declref_expr);

      // Check for reserved identifiers.
      if (strcmp(ident.name, "print") == 0) {
        eval_stack(self, +1).kind = rv_print;
        self->value_index++;
        assert(self->value_index < VALUE_STACK_SIZE);
        break;
      }

      // Lookup the identifier.
      RuntimeValue* value = ident_lookup(self, &ident, env->report_diag);
      free(ident.name);
      if (value != NULL) {
        // If the value is lazy, evaluate it now.
        if (value->kind == rv_lazy) {
          eval_push_frame(self, ef_function);
          node_walk(value->bits.lazy_v, self->context, user, eval_node);
          eval_pop_frame(self);
        } else {
          value_copy(&eval_stack(self, +1), value);
          self->value_index++;
          assert(self->value_index < VALUE_STACK_SIZE);
        }
        break;
      }

      // The identifier was undefined.
      self->status = EVAL_STATUS_ERR;
      return false;
    }

    case nk_bool_expr: {
      eval_stack(self, +1).kind = rv_bool;
      eval_stack(self, +1).bits.bool_v = node->bits.bool_expr;
      self->value_index++;
      assert(self->value_index < VALUE_STACK_SIZE);
      break;
    }

    case nk_integer_expr: {
      eval_stack(self, +1).kind = rv_integer;
      eval_stack(self, +1).bits.integer_v = node->bits.integer_expr;
      self->value_index++;
      assert(self->value_index < VALUE_STACK_SIZE);
      break;
    }

    case nk_float_expr: {
      eval_stack(self, +1).kind = rv_float;
      eval_stack(self, +1).bits.float_v = node->bits.float_expr;
      self->value_index++;
      assert(self->value_index < VALUE_STACK_SIZE);
      break;
    }

    case nk_unary_expr: {
      // Compute the expression.
      RuntimeValue* subexpr = &eval_stack_top(self);
      switch (subexpr->kind) {
        case rv_integer:
          switch (node->bits.unary_expr.op.kind) {
            case tk_plus:
              return true;
            case tk_minus:
              subexpr->bits.integer_v = -subexpr->bits.integer_v;
              return true;
            case tk_tilde:
              subexpr->bits.integer_v = ~subexpr->bits.integer_v;
              return true;
            default:
              break;
          }
          break;

        case rv_float:
          switch (node->bits.unary_expr.op.kind) {
            case tk_plus:
              return true;
            case tk_minus:
              subexpr->bits.float_v = -subexpr->bits.float_v;
              return true;
            default:
              break;
          }
          break;

        case rv_bool:
          if (node->bits.unary_expr.op.kind == tk_not) {
            subexpr->bits.bool_v = !subexpr->bits.bool_v;
            return true;
          }

        default: break;
      }

      // Complain if we couldn't find a valid operation to apply.
      Token* op = &node->bits.unary_expr.op;
      char msg[255] = { 0 };
      strcpy (msg, "unary operator '");
      strncat(msg, self->context->source + op->start, op->end - op->start);
      strcat (msg, "' is not defined for value of type '");
      strcat (msg, value_type_name(subexpr));
      strcat (msg, "'");
      EvalError error = { node->start, node->end, msg };
      env->report_diag(error, self);
      self->status = EVAL_STATUS_ERR;
      return false;
    }

    case nk_binary_expr: {
#define BIN_CASE(token_kind, bin_op, field) case token_kind:\
  lhs->bits.field = bin_op(lhs->bits.field, rhs->bits.field);\
  break;

#define CMP_CASE(token_kind, cmp_op, field) case token_kind:\
  lhs->kind = rv_bool; \
  lhs->bits.bool_v = cmp_op(lhs->bits.field, rhs->bits.field);\
  break;

      RuntimeValue* rhs = &eval_stack_top(self);
      RuntimeValue* lhs = &eval_stack(self, -1);

      // All binary operators are functions whose domain is a pair of the same type.
      bool was_defined = lhs->kind == rhs->kind;
      if (was_defined) {
        switch (lhs->kind) {
          case rv_integer:
            switch (node->bits.binary_expr.op.kind) {
              BIN_CASE(tk_l_shift , COCODOL_ILSH, integer_v)
              BIN_CASE(tk_r_shift , COCODOL_IRSH, integer_v)
              BIN_CASE(tk_star    , COCODOL_IMUL, integer_v)
              BIN_CASE(tk_slash   , COCODOL_IDIV, integer_v)
              BIN_CASE(tk_percent , COCODOL_IMOD, integer_v)
              BIN_CASE(tk_plus    , COCODOL_IADD, integer_v)
              BIN_CASE(tk_minus   , COCODOL_ISUB, integer_v)
              BIN_CASE(tk_pipe    , COCODOL_IOR , integer_v)
              BIN_CASE(tk_amp     , COCODOL_IAND, integer_v)
              BIN_CASE(tk_caret   , COCODOL_IXOR, integer_v)
              CMP_CASE(tk_lt      , COCODOL_LT  , integer_v)
              CMP_CASE(tk_le      , COCODOL_LE  , integer_v)
              CMP_CASE(tk_gt      , COCODOL_GT  , integer_v)
              CMP_CASE(tk_ge      , COCODOL_GE  , integer_v)
              CMP_CASE(tk_eq      , COCODOL_EQ  , integer_v)
              CMP_CASE(tk_ne      , COCODOL_NE  , integer_v)
              default:
                was_defined = false;
                break;
            }
            break;

          case rv_float:
            switch (node->bits.binary_expr.op.kind) {
              BIN_CASE(tk_star    , COCODOL_FMUL, float_v)
              BIN_CASE(tk_slash   , COCODOL_FDIV, float_v)
              BIN_CASE(tk_percent , COCODOL_FMOD, float_v)
              BIN_CASE(tk_plus    , COCODOL_FADD, float_v)
              BIN_CASE(tk_minus   , COCODOL_FSUB, float_v)
              CMP_CASE(tk_lt      , COCODOL_LT  , float_v)
              CMP_CASE(tk_le      , COCODOL_LE  , float_v)
              CMP_CASE(tk_gt      , COCODOL_GT  , float_v)
              CMP_CASE(tk_ge      , COCODOL_GE  , float_v)
              CMP_CASE(tk_eq      , COCODOL_EQ  , float_v)
              CMP_CASE(tk_ne      , COCODOL_NE  , float_v)
              default:
                was_defined = false;
                break;
            }
            break;

          case rv_bool:
            switch (node->bits.binary_expr.op.kind) {
              BIN_CASE(tk_and     , COCODOL_LAND, bool_v)
              BIN_CASE(tk_or      , COCODOL_LOR , bool_v)
              default:
                was_defined = false;
                break;
            }
            break;


          default:
            was_defined = false;
            break;
        }
      }

      if (!was_defined) {
        // Complain if we couldn't find a valid operation to apply.
        Token* op = &node->bits.binary_expr.op;
        char msg[255] = { 0 };
        strcpy (msg, "operator '");
        strncat(msg, self->context->source + op->start, op->end - op->start);
        strcat (msg, "' is not defined for values of type '");
        strcat (msg, value_type_name(lhs));
        strcat (msg, "' and '");
        strcat (msg, value_type_name(rhs));
        strcat (msg, "'");
        EvalError error = { node->start, node->end, msg };
        env->report_diag(error, self);
        self->status = EVAL_STATUS_ERR;
        return false;
      } else {
        drop(&eval_stack_top(self));
        self->value_index--;
      }

      break;
    }

    case nk_apply_expr: {
      size_t argc = node->bits.apply_expr.argc;
      RuntimeValue* callee = &eval_stack(self, -argc);
      switch (callee->kind) {
        case rv_print:
          eval_print(callee + 1);
          for (size_t i = 0; i <= argc; ++i) {
            drop(&eval_stack(self, -i));
          }
          self->value_index -= (argc + 1);
          break;

        case rv_function: {
          // Get the declaration of the function being called.
          Node* fun_decl = context_get_nodeptr(self->context, callee->bits.function_v.decl);

          // Copies the function parameters into its locals.
          EvalFrame* frame = eval_push_frame(self, ef_function);
          size_t paramc = fun_decl->bits.fun_decl.paramc;

          Ident ident;
          for (size_t i = 0; i < paramc; ++i) {
            RuntimeValue* arg = malloc(sizeof(RuntimeValue));
            value_copy(arg, &eval_stack(self, -i));
            drop(&eval_stack(self, -i));

            ident_init(&ident, self->context, &fun_decl->bits.fun_decl.paramv[paramc - i - 1]);
            if (!insert_symbol(self, &frame->locals, &ident, arg, env->report_diag)) {
              free(ident.name);
              self->status = EVAL_STATUS_ERR;
              return false;
            }
          }
          self->value_index -= argc;

          // Copies the function environment into its locals.
          if (callee->bits.function_v.env != NULL) {
            symtable_foreach(callee->bits.function_v.env, &frame->locals, copy_symbol_entry);
          }

          // Call the function.
          node_walk(fun_decl->bits.fun_decl.body, self->context, user, eval_node);
          while (self->frame != frame->prev) {
            eval_pop_frame(self);
          }

          // Drop the callee and move the function result down.
          drop(callee);
          eval_stack(self, -1) = eval_stack_top(self);
          eval_stack_top(self).kind = rv_junk;
          self->value_index--;
          break;
        }

        default: {
          EvalError error = { node->start, node->end, "bad callee" };
          env->report_diag(error, self);
          self->status = EVAL_STATUS_ERR;
          return false;
        }
      }

      break;
    }

    case nk_paren_expr:
      return true;

    case nk_expr_stmt: {
      // Clear the value stack if the parent node won't consume it.
      size_t offset = self->frame
        ? self->value_index - self->frame->value_index
        : self->value_index;
      for (size_t i = 0; i < offset; ++i) {
        drop(&eval_stack(self, -i));
      }
      self->value_index -= offset;
      break;
    }

    case nk_brace_stmt:
      eval_pop_frame(self);
      break;

    case nk_brk_stmt:
      self->status = EVAL_STATUS_BRK;
      return false;

    case nk_nxt_stmt:
    case nk_ret_stmt:
      return false;

    default:
      assert(false && "bad AST");
  }

  return true;
}

/// Evaluates the given program.
int eval_program(EvalState* self,
                 const NodeID* decls,
                 size_t decl_count,
                 EvalErrorCallback report_diag)
{
  // Create a buffer to cache top-level declarations.
  NodeID top_decls[decl_count];
  size_t top_decl_count = 0;

  // Populate the global symbol table.
  for (size_t i = 0; i < decl_count; ++i) {
    NodeID decl_index = decls[i];
    Node* decl = context_get_nodeptr(self->context, decl_index);

    // Skip top-level declarations.
    if (decl->kind == nk_top_decl) {
      top_decls[top_decl_count] = decl_index;
      top_decl_count++;
      continue;
    }

    // Register a global variable.
    if (decl->kind == nk_var_decl) {
      RuntimeValue* value = malloc(sizeof(RuntimeValue));
      if (decl->bits.var_decl.initializer != ~0) {
        value->kind = rv_lazy;
        value->bits.lazy_v = decl->bits.var_decl.initializer;
      } else {
        value->kind = rv_junk;
      }

      Ident ident;
      ident_init(&ident, self->context, &decl->bits.var_decl.name);
      if (!insert_symbol(self, &self->globals, &ident, value, report_diag)) {
        free(ident.name);
        free(value);
      }
      continue;
    }

    // Register a global function.
    if (decl->kind == nk_fun_decl) {
      RuntimeValue* value = malloc(sizeof(RuntimeValue));
      value->kind = rv_function;
      value->bits.function_v.decl = decl_index;
      value->bits.function_v.env = NULL;

      Ident ident;
      ident_init(&ident, self->context, &decl->bits.fun_decl.name);
      if (!insert_symbol(self, &self->globals, &ident, value, report_diag)) {
        free(ident.name);
        free(value);
      }
      continue;
    }

    // FIXME
    assert(false && "no implemented");
  }

  // Evaluates the top-level declarations.
  EvalEnv env = { self, report_diag };
  for (size_t i = 0; i < top_decl_count; ++i) {
    if (!node_walk(top_decls[i], self->context, &env, eval_node)) { break; }
  }

  return self->status;
}
