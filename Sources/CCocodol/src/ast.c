#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "ast.h"
#include "context.h"
#include "token.h"
#include "utils.h"

void node_deinit(Node* self) {
  switch (self->kind) {
    case nk_top_decl:
      free(self->bits.top_decl.stmtv);
      self->bits.top_decl.stmtv = NULL;
      self->bits.top_decl.stmtc = 0;
      break;

    case nk_fun_decl:
      free(self->bits.fun_decl.paramv);
      self->bits.fun_decl.paramv = NULL;
      self->bits.fun_decl.paramc = 0;
      break;

    case nk_apply_expr:
      free(self->bits.apply_expr.argv);
      self->bits.apply_expr.argv = NULL;
      self->bits.apply_expr.argc = 0;
      break;

    case nk_brace_stmt:
      free(self->bits.brace_stmt.stmtv);
      self->bits.brace_stmt.stmtv = NULL;
      self->bits.brace_stmt.stmtc = 0;

      DeclList* head = self->bits.brace_stmt.last_decl;
      while (head != NULL) {
        DeclList* next = head->prev;
        free(head);
        head = next;
      }
      break;

    default:
      break;
  }
}

bool node_walk(NodeID index,
               struct Context* context,
               void*  user,
               bool   (*visit)(NodeID, NodeKind, bool, void*))
{
  Node* node = context_get_nodeptr(context, index);
  NodeKind kind = node->kind;
  if (!visit(index, kind, true, user)) { return true; }

  switch (kind) {
    case nk_top_decl: {
      size_t  stmtc = node->bits.top_decl.stmtc;
      NodeID* stmtv = node->bits.top_decl.stmtv;
      for (size_t i = 0; i < stmtc; ++i) {
        if (!node_walk(stmtv[i], context, user, visit)) { return false; }
      }
      break;
    }

    case nk_var_decl: {
      NodeID init = node->bits.var_decl.initializer;
      if (init != ~0) {
        if (!node_walk(init, context, user, visit)) { return false; }
      }
      break;
    }

    case nk_fun_decl: {
      NodeID body = node->bits.fun_decl.body;
      if (!node_walk(body, context, user, visit)) { return false; }
      break;
    }

    case nk_obj_decl: {
      NodeID body = node->bits.obj_decl.body;
      if (!node_walk(body, context, user, visit)) { return false; }
      break;
    }

    case nk_unary_expr: {
      NodeID subexpr = node->bits.unary_expr.subexpr;
      if (!node_walk(subexpr, context, user, visit)) { return false; }
      break;
    }

    case nk_binary_expr: {
      NodeID lhs = node->bits.binary_expr.lhs;
      NodeID rhs = node->bits.binary_expr.rhs;
      if (!node_walk(lhs, context, user, visit)) { return false; }
      if (!node_walk(rhs, context, user, visit)) { return false; }
      break;
    }

    case nk_member_expr: {
      size_t base = node->bits.member_expr.base;
      if (!node_walk(base, context, user, visit)) { return false; }
      break;
    }

    case nk_apply_expr: {
      NodeID callee = node->bits.apply_expr.callee;
      size_t  argc = node->bits.apply_expr.argc;
      NodeID* argv = node->bits.apply_expr.argv;
      if (!node_walk(callee, context, user, visit)) { return false; }
      for (size_t i = 0; i < argc; ++i) {
        if (!node_walk(argv[i], context, user, visit)) { return false; }
      }
      break;
    }

    case nk_paren_expr: {
      NodeID subexpr = node->bits.paren_expr;
      if (!node_walk(subexpr, context, user, visit)) { return false; }
      break;
    }

    case nk_brace_stmt: {
      size_t  stmtc = node->bits.brace_stmt.stmtc;
      NodeID* stmtv = node->bits.brace_stmt.stmtv;
      for (size_t i = 0; i < stmtc; ++i) {
        if (!node_walk(stmtv[i], context, user, visit)) { return false; }
      }
      break;
    }

    case nk_expr_stmt: {
      NodeID expr = node->bits.expr_stmt;
      if (!node_walk(expr, context, user, visit)) { return false; }
      break;
    }

    case nk_if_stmt: {
      NodeID cond  = node->bits.if_stmt.cond;
      NodeID then  = node->bits.if_stmt.then_;
      NodeID else_ = node->bits.if_stmt.else_;
      if (!node_walk(cond, context, user, visit)) { return false; }
      if (!node_walk(then, context, user, visit)) { return false; }
      if (else_ != ~0) {
        if (!node_walk(else_, context, user, visit)) { return false; }
      }
      break;
    }

    case nk_while_stmt: {
      NodeID cond = node->bits.while_stmt.cond;
      NodeID body = node->bits.while_stmt.body;
      if (!node_walk(cond, context, user, visit)) { return false; }
      if (!node_walk(body, context, user, visit)) { return false; }
      break;
    }

    case nk_ret_stmt: {
      NodeID value = node->bits.ret_stmt;
      if (!node_walk(value, context, user, visit)) { return false; }
      break;
    }

    case nk_error:
    case nk_declref_expr:
    case nk_bool_expr:
    case nk_integer_expr:
    case nk_float_expr:
    case nk_brk_stmt:
    case nk_nxt_stmt:
      break;
  }

  return visit(index, kind, false, user);
}

typedef struct {
  Context* context;
  NodeID fun_index;
  NodeID body_index;
  NodeID scope;
  size_t symc;
  Token** symv;
} CaptureVisitorEnv;

bool ident_is_local(CaptureVisitorEnv* env, Token* lhs) {
  Context* context = env->context;
  Token* rhs = NULL;

  // Search within the function's local scopes.
  NodeID scope_index = env->scope;
  while (scope_index != ~0) {
    // Get the declaration list of the current scope.
    Node* scope = context_get_nodeptr(context, scope_index);
    assert(scope->kind == nk_brace_stmt);

    // Search within the current scope.
    DeclList* list = scope->bits.brace_stmt.last_decl;
    while (list) {
      Node* decl = context_get_nodeptr(context, list->decl);
      list = list->prev;

      switch (decl->kind) {
        case nk_var_decl:
          rhs = &decl->bits.var_decl.name;
          break;

        case nk_fun_decl:
          rhs = &decl->bits.var_decl.name;
          break;

        default:
          continue;
      }

      if (token_text_equal(context, lhs, rhs)) {
        // It's a reference to a local declaration; no further action required.
        return true;
      }
    }

    // Move to the parent scope, unless we reached the function.
    if (scope_index != env->body_index) {
      scope_index = scope->bits.brace_stmt.parent;
    } else {
      scope_index = ~0;
    }
  }

  // Check the function's parameters.
  Node* fun_decl = context_get_nodeptr(context, env->fun_index);
  for (size_t i = 0; i < fun_decl->bits.fun_decl.paramc; ++i) {
    rhs = &fun_decl->bits.fun_decl.paramv[i];
    if (token_text_equal(context, lhs, rhs)) {
      // It's a reference to a parameter; no further action required.
      return true;
    }
  }

  // Check the function's name.
  rhs = &fun_decl->bits.fun_decl.name;
  if (token_text_equal(context, lhs, rhs)) {
    // It's a recursive reference; no further action required.
    return true;
  }

  // The symbol is defined externally.
  return false;
}

bool capture_insert_symbol(Context* context, Token** symv, Token* new_sym) {
  // Compute the index of the symbol to insert.
  size_t h = fnv1_hash_buffer(context->source + new_sym->start, new_sym->end - new_sym->start);
  size_t position = h % MAX_CAPTURE_COUNT;
  size_t insert_position = position;
  while (symv[insert_position] != NULL) {
    if (token_text_equal(context, new_sym, symv[insert_position])) {
      // The symbol is already in the capture list.
      return false;
    }

    // Check the next position.
    insert_position = (insert_position + 1) % MAX_CAPTURE_COUNT;
    assert(insert_position != position);
  }

  symv[insert_position] = new_sym;
  return true;
}

bool capture_visitor(NodeID index, NodeKind kind, bool pre, void* user) {
  CaptureVisitorEnv* env = (CaptureVisitorEnv*)user;
  Context* context = env->context;

  switch (kind) {
    case nk_fun_decl: {
      assert(pre);

      // Build and merge the nested function's capture list.
      Token* symv[MAX_CAPTURE_COUNT];
      capture_set(index, context, symv, false);

      for (size_t i = 0; i < MAX_CAPTURE_COUNT; ++i) {
        // Check whether the nested function's captured symbols also escape the current function.
        if ((symv[i] != NULL) &&
            !ident_is_local(env, symv[i]) &&
            capture_insert_symbol(context, env->symv, symv[i])) {
          env->symc++;
        }
      }

      return false;
    }

    case nk_obj_decl:
      assert(false && "not implemented");

    case nk_declref_expr: {
      assert(pre);

      // Lookup the symbol statically.
      Node* expr = context_get_nodeptr(context, index);
      Token* lhs = &expr->bits.declref_expr;
      if (!ident_is_local(env, lhs) &&
          capture_insert_symbol(context, env->symv, lhs)) {
        env->symc++;
      }

      return false;
    }

    case nk_brace_stmt:
      if (pre) {
        env->scope = index;
      } else {
        env->scope = context_get_nodeptr(context, index)->bits.brace_stmt.parent;
      }
      return true;

    default:
      // assert(pre);
      return true;
  }
}

size_t capture_set(NodeID fun_index, Context* context, Token** symv, bool defrag) {
  memset(symv, 0, MAX_CAPTURE_COUNT * sizeof(Token*));

  NodeID body_index = context_get_nodeptr(context, fun_index)->bits.fun_decl.body;
  CaptureVisitorEnv env = {
    context   , // AST context
    fun_index , // index of the declaration being visited
    body_index, // index of the function's body
    body_index, // initial search scope
    0         , // identifier count
    symv };

  // Visit the function declaration.
  node_walk(body_index, context, &env, capture_visitor);

  // Defragment the results, if requested to.
  if (defrag) {
    size_t free_position = 0;
    for (size_t i = 0; i < MAX_CAPTURE_COUNT; ++i) {
      if (symv[i] != NULL) {
        symv[free_position] = symv[i];
        free_position += 1;
      }
    }
  }

  return env.symc;
}
