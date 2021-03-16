#ifndef COCODOL_AST_H
#define COCODOL_AST_H

#include "common.h"
#include "token.h"

#define NODE_DECL_BIT (1 << 16)
#define NODE_EXPR_BIT (1 << 17)
#define NODE_STMT_BIT (1 << 18)

/// The kind of an AST Node.
typedef enum NodeKind {
  nk_error        = 0,

  nk_top_decl     = 1 | NODE_DECL_BIT,
  nk_var_decl     = 2 | NODE_DECL_BIT,
  nk_fun_decl     = 3 | NODE_DECL_BIT,
  nk_obj_decl     = 4 | NODE_DECL_BIT,

  nk_declref_expr = 1 | NODE_EXPR_BIT,
  nk_bool_expr    = 2 | NODE_EXPR_BIT,
  nk_integer_expr = 3 | NODE_EXPR_BIT,
  nk_float_expr   = 4 | NODE_EXPR_BIT,
  nk_unary_expr   = 5 | NODE_EXPR_BIT,
  nk_binary_expr  = 6 | NODE_EXPR_BIT,
  nk_member_expr  = 7 | NODE_EXPR_BIT,
  nk_apply_expr   = 8 | NODE_EXPR_BIT,
  nk_paren_expr   = 9 | NODE_EXPR_BIT,

  nk_brace_stmt   = 1 | NODE_STMT_BIT,
  nk_expr_stmt    = 2 | NODE_STMT_BIT,
  nk_if_stmt      = 3 | NODE_STMT_BIT,
  nk_while_stmt   = 4 | NODE_STMT_BIT,
  nk_brk_stmt     = 5 | NODE_STMT_BIT,
  nk_nxt_stmt     = 6 | NODE_STMT_BIT,
  nk_ret_stmt     = 7 | NODE_STMT_BIT,
} NodeKind;

/// A linked list of declarations.
typedef struct DeclList {
  NodeID decl;
  struct DeclList* prev;
} DeclList;

/// An AST node.
typedef struct Node {

  /// The node's kind.
  NodeKind kind;

  /// The index at which the node starts in the source input.
  size_t start;

  /// The index at which the node ends in the source input.
  size_t end;

  /// The contents of the node.
  union NodeContents {

    /// An array containing the indices of each statement in the top-level declaration.
    struct {
      size_t  stmtc;
      NodeID* stmtv;
    } top_decl;

    /// The name of the declaration and its initializer, if any.
    ///
    /// If the declaration has no initializer, its index is set to the maximum representable value
    /// of `NodeID` (i.e., `~0`).
    struct {
      Token   name;
      NodeID  initializer;
    } var_decl;

    /// The name of the function, its parameters and its body.
    struct {
      Token   name;
      size_t  paramc;
      Token*  paramv;
      NodeID  body;
    } fun_decl;

    /// The name of the type and its body.
    struct {
      Token   name;
      NodeID  body;
    } obj_decl;

    /// The name of the symbol being referred.
    Token declref_expr;

    /// The Boolean value.
    bool bool_expr;

    /// The number's value.
    long int integer_expr;

    /// The number's value.
    float float_expr;

    /// The prefix operator and the operand's expression.
    struct {
      Token   op;
      NodeID  subexpr;
    } unary_expr;

    /// The infix operator and each operand's expression.
    struct {
      Token   op;
      NodeID  lhs;
      NodeID  rhs;
    } binary_expr;

    /// The base expression and the member's name.
    struct {
      NodeID  base;
      Token   member;
    } member_expr;

    /// The callee's expression and and the arguments of the application.
    struct {
      NodeID  callee;
      size_t  argc;
      NodeID* argv;
    } apply_expr;

    /// The sub-expression.
    NodeID paren_expr;

    /// An array containing the indices of each statement the index of the parent scope and a
    /// pointer to a linked list of named declarations.
    struct {
      size_t  stmtc;
      NodeID* stmtv;
      NodeID  parent;
      DeclList* last_decl;
    } brace_stmt;

    /// The expression being wrapped.
    NodeID expr_stmt;

    /// The statement's condition and its branches.
    ///
    /// It then "else" branch is not defined, then its index is set to the maximum representable
    /// value of `NodeID` (i.e., `~0`).
    struct {
      NodeID  cond;
      NodeID  then_;
      NodeID  else_;
    } if_stmt;

    /// The statement's condition and its body.
    struct {
      NodeID  cond;
      NodeID  body;
    } while_stmt;

    /// The expression of the return value.
    NodeID ret_stmt;

  } bits;

} Node;

/// Deinitializes a node.
///
/// This frees the memory optionally allocated for the node's contents.
void node_deinit(Node*);

/// Walks an AST, calling the given function every time the walker enters or exits a node.
///
/// The `visit` function` must accept 4 parameters:
/// (1) the index of the node being visited,
/// (2) its kind,
/// (3) a flag indicating whether the walker is entering (`true`) or exiting (`false`) the node,
/// (4) a pointer to arbitrary user data that can serve as an environment.
///
/// In pre-order mode, the return value of `visit` determines whether the walk should continue to
/// the node's children (`true`) or skip the sub-tree (`false`). In post-order mode, it determines
/// whether the walk should continue (`true`) or abort (`false`).
///
/// This function returns `false` if the walk was aborted in post-order mode, otherwise it always
/// returns `true`.
bool node_walk(NodeID index,
               struct Context* context,
               void*  user,
               bool   (*visit)(NodeID, NodeKind, bool, void*));

#endif
