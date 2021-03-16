#include <stdlib.h>

#include "ast.h"
#include "context.h"

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
