#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "parser.h"

#define INITIAL_VEC_CAPACITY 64
#define MAX_PARAM_COUNT      64

#define token_isprefix(kind) (((kind) & TOK_PRFX_BIT) == TOK_PRFX_BIT)

NodeID parse_brace_stmt(ParserState* self, ParseErrorCallback report_diag);

void parser_init(ParserState* self, Context* context, void* user_data) {
  lexer_init(&self->lexer, context->source);
  self->context = context;
  self->lookahead_start = 0;
  self->lookahead_end = 0;
  self->scope = ~0;
  self->user_data = user_data;
}

void parser_deinit(ParserState* self) {
  lexer_deinit(&self->lexer);
  self->context = NULL;
  self->lookahead_start = 0;
  self->lookahead_end = 0;
  self->user_data = NULL;
}

/// Returns a pointer to the next token in the stream, or `NULL` if the parser reached the end of
/// the stream.
Token* peek(ParserState* self) {
  Token* token_ptr = self->lookahead_buffer + self->lookahead_start;
  if (self->lookahead_start < self->lookahead_end) {
    return token_ptr;
  }

  if (lexer_next(&self->lexer, token_ptr)) {
    self->lookahead_end++;
    return token_ptr;
  } else {
    return NULL;
  }
}

/// Consumes a token from the stream and returns a pointer to it.
Token* consume(ParserState* self) {
  Token* token_ptr = peek(self);
  self->lookahead_start++;
  if (self->lookahead_start == TOKEN_BUFFER_LENGTH) {
    self->lookahead_start = 0;
    self->lookahead_end -= TOKEN_BUFFER_LENGTH;
  }
  return token_ptr;
}

/// Returns whether the given token can serve to delimit the end of a statement when the parser
/// attempts to recover.
bool is_stmt_delimiter(Token* token, Context* context, TokenKind terminator) {
  if (token->start == 0) {
    return false;
  } else if ((token->kind == tk_semicolon) || (token->kind == terminator)) {
    return true;
  } else {
    char ch = context->source[token->start - 1];
    return (ch == '\n') || (ch == '\r');
  }
}

NodeID create_error_node(Context* context, size_t start, size_t end) {
  NodeID node_index = context_new_node(context);
  Node* node  = context_get_nodeptr(context, node_index);
  node->kind  = nk_error;
  node->start = start;
  node->end   = end;
  return node_index;
}

// ------------------------------------------------------------------------------------------------
// MARK: Declarations
// ------------------------------------------------------------------------------------------------

/// Parses a list of function parameters, including the opening and closing parentheses.
size_t parse_param_list(ParserState* self, Token* paramv, ParseErrorCallback report_diag) {
  Token* next = peek(self);
  size_t count = 0;

  // Parse the opening parenthesis.
  if (!next || (next->kind != tk_l_paren)) {
    size_t end = next ? next->start : strlen(self->context->source);
    ParseError error = { end, "expected parameter list" };
    report_diag(error, self);
    return ~0;
  }
  consume(self);

  // Parse the list of parameters.
  while ((next = peek(self))) {
    // Stop if we found the list terminator.
    if (next->kind == tk_r_paren) { break; }

    // Complain if there's a leading separator.
    if (next->kind == tk_comma) {
      ParseError error = { next->start, "expected parameter name" };
      report_diag(error, self);
      while ((next = peek(self)) && (next->kind == tk_comma)) {
        consume(self);
      }

      // Bail out if we reach the end of the stream.
      if (!next) { break; }
    }

    // Parse one name.
    paramv[count] = *next;
    if (next->kind == tk_name) {
      consume(self);
    } else {
      paramv[count].kind = tk_error;
      ParseError error = { next->start, "expected parameter name" };
      report_diag(error, self);
    }

    // Increment the number of parameters.
    count++;
    assert(count <= MAX_PARAM_COUNT);

    // Parse a separator, unless we reached the terminator.
    next = peek(self);
    if (!next || (next->kind == tk_r_paren)) { break; }
    if (next->kind == tk_comma) {
      consume(self);
    } else {
      ParseError error = { next->start, "expected ',' separator" };
      report_diag(error, self);
    }
  }

  // Parse the closing parenthesis.
  next = peek(self);
  if (next && next->kind == tk_r_paren) {
    consume(self);
  } else {
    size_t end = next ? next->start : strlen(self->context->source);
    ParseError error = { end, "missing closing parenthesis" };
    report_diag(error, self);
  }

  return count;
}

NodeID parse_var_decl(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_var));

  // Create a new node, representing the variable declaration.
  NodeID decl_index = context_new_node(self->context);
#define the_decl context_get_nodeptr(self->context, decl_index)
  the_decl->kind = nk_var_decl;
  the_decl->start = next->start;

  // Parse the name of the variable.
  next = peek(self);
  if (next == NULL) {
    the_decl->kind = nk_error;
    ParseError error = { strlen(self->context->source), "expected variable name" };
    report_diag(error, self);
    return decl_index;
  }

  if (next->kind == tk_name) {
    the_decl->bits.var_decl.name = *next;
    consume(self);
  } else {
    the_decl->bits.var_decl.name.kind = tk_error;
    ParseError error = { next->start, "expected variable name" };
    report_diag(error, self);
  }

  // Register the declaration in the current scope.
  if (self->scope != ~0) {
    Node* scope = context_get_nodeptr(self->context, self->scope);
    assert(scope->kind == nk_brace_stmt);

    DeclList* link = malloc(sizeof(DeclList));
    link->decl = decl_index;
    link->prev = scope->bits.brace_stmt.last_decl;
    scope->bits.brace_stmt.last_decl = link;
  }

  // Parse the variable's initializer, if any.
  next = peek(self);
  if (next && (next->kind == tk_assign)) {
    consume(self);

    // Parse the initializer expression.
    NodeID expr_index = parse_expr(self, report_diag);
    the_decl->bits.var_decl.initializer = expr_index;
    the_decl->end = context_get_nodeptr(self->context, expr_index)->end;
  } else {
    the_decl->bits.var_decl.initializer = ~0;
    the_decl->end = next->end;
  }

  return decl_index;
#undef the_decl
}

NodeID parse_fun_decl(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_fun));

  // Create a new node, representing the function declaration.
  NodeID decl_index = context_new_node(self->context);
#define the_decl context_get_nodeptr(self->context, decl_index)
  the_decl->kind  = nk_fun_decl;
  the_decl->start = next->start;

  // Parse the name of the function.
  next = peek(self);
  if (next == NULL) {
    the_decl->kind = nk_error;
    ParseError error = { strlen(self->context->source), "expected function name" };
    report_diag(error, self);
    return decl_index;
  }

  if (next->kind == tk_name) {
    the_decl->bits.fun_decl.name = *next;
    consume(self);
  } else {
    the_decl->bits.fun_decl.name.kind = tk_error;
    ParseError error = { next->start, "expected function name" };
    report_diag(error, self);
  }

  // Register the declaration in the current scope.
  if (self->scope != ~0) {
    Node* scope = context_get_nodeptr(self->context, self->scope);
    assert(scope->kind == nk_brace_stmt);

    DeclList* link = malloc(sizeof(DeclList));
    link->decl = decl_index;
    link->prev = scope->bits.brace_stmt.last_decl;
    scope->bits.brace_stmt.last_decl = link;
  }

  // Parse the list of parameters.
  Token paramv[MAX_PARAM_COUNT];
  size_t paramc = parse_param_list(self, paramv, report_diag);
  the_decl->bits.fun_decl.paramc = paramc;
  if (paramc > 0) {
    the_decl->bits.fun_decl.paramv = malloc(paramc * sizeof(Token));
    memcpy(the_decl->bits.fun_decl.paramv, paramv, paramc * sizeof(Token));
  } else {
    the_decl->bits.fun_decl.paramv = NULL;
  }

  // Parse the body of the function.
  next = peek(self);
  if (next && (next->kind == tk_l_brace)) {
    NodeID body_index = parse_brace_stmt(self, report_diag);
    the_decl->bits.fun_decl.body = body_index;
    the_decl->end = context_get_nodeptr(self->context, body_index)->end;
  } else {
    size_t end = next ? next->start : strlen(self->context->source);
    the_decl->bits.fun_decl.body = create_error_node(self->context, end, end);
    the_decl->end = end;
    ParseError error = { end, "expected function body" };
    report_diag(error, self);
  }

  return decl_index;
#undef the_decl
}

NodeID parse_obj_decl(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_obj));

  // Create a new node, representing the variable declaration.
  NodeID decl_index = context_new_node(self->context);
#define the_decl context_get_nodeptr(self->context, decl_index)
  the_decl->kind = nk_obj_decl;
  the_decl->start = next->start;

  // Parse the name of the type.
  next = peek(self);
  if (next == NULL) {
    the_decl->kind = nk_error;
    ParseError error = { strlen(self->context->source), "expected type name" };
    report_diag(error, self);
    return decl_index;
  }

  if (next->kind == tk_name) {
    the_decl->bits.obj_decl.name = *next;
    consume(self);
  } else {
    the_decl->bits.obj_decl.name.kind = tk_error;
    ParseError error = { next->start, "expected type name" };
    report_diag(error, self);
  }

  // Register the declaration in the current scope.
  if (self->scope != ~0) {
    Node* scope = context_get_nodeptr(self->context, self->scope);
    assert(scope->kind == nk_brace_stmt);

    DeclList* link = malloc(sizeof(DeclList));
    link->decl = decl_index;
    link->prev = scope->bits.brace_stmt.last_decl;
    scope->bits.brace_stmt.last_decl = link;
  }

  // Parse the body of the type.
  next = peek(self);
  if (next && (next->kind == tk_l_brace)) {
    NodeID body_index = parse_brace_stmt(self, report_diag);
    the_decl->bits.obj_decl.body = body_index;
    the_decl->end = context_get_nodeptr(self->context, body_index)->end;
  } else {
    size_t end = next ? next->start : strlen(self->context->source);
    the_decl->bits.obj_decl.body = create_error_node(self->context, end, end);
    the_decl->end = end;
    ParseError error = { next->start, "expected type body" };
    report_diag(error, self);
  }

  return decl_index;
#undef the_decl
}

NodeID parse_decl(ParserState* self, ParseErrorCallback report_diag) {
  Token* head = peek(self);
  if (head == NULL) {
    size_t loc = strlen(self->context->source);
    ParseError error = { loc, "expected declaration" };
    report_diag(error, self);
    return create_error_node(self->context, loc, loc);
  }

  switch (head->kind) {
    case tk_var: return parse_var_decl(self, report_diag);
    case tk_fun: return parse_fun_decl(self, report_diag);
    case tk_obj: return parse_obj_decl(self, report_diag);
    default: break;
  }

  ParseError error = { head->start, "expected declaration" };
  report_diag(error, self);
  return create_error_node(self->context, head->start, head->end);
}

// ------------------------------------------------------------------------------------------------
// MARK: Expressions
// ------------------------------------------------------------------------------------------------

/// Parses a comma-separated list of expressions, terminated by a right parenthesis.
///
/// This functions expects that the next consumable token be after the opening parenthesis. The
/// closing parenthesis is not consumed.
size_t parse_expr_list(ParserState* self, NodeID* items, ParseErrorCallback report_diag) {
  Token* next = NULL;
  size_t count = 0;

  while ((next = peek(self))) {
    // Stop if we found the list terminator.
    if (next->kind == tk_r_paren) { break; }

    // Complain if there's a leading separator.
    if (next->kind == tk_comma) {
      ParseError error = { next->start, "expected expression" };
      report_diag(error, self);
      while ((next = peek(self)) && (next->kind == tk_comma)) {
        consume(self);
      }
    }

    // Parse an item.
    items[count] = parse_expr(self, report_diag);
    count++;
    assert(count <= MAX_PARAM_COUNT);

    // Parse a separator, unless we reached the terminator.
    next = peek(self);
    if (!next || (next->kind == tk_r_paren)) { return count; }
    if (next->kind == tk_comma) {
      consume(self);
    } else {
      ParseError error = { next->start, "expected ',' separator" };
      report_diag(error, self);
    }
  }

  return count;
}

NodeID parse_primary_expr(ParserState* self, ParseErrorCallback report_diag) {
  Token* head = consume(self);
  if (head == NULL) {
    size_t loc = strlen(self->context->source);
    ParseError error = { loc, "expected expression" };
    report_diag(error, self);
    return create_error_node(self->context, loc, loc);
  }

  // Parse a Boolean literal.
  if ((head->kind == tk_true) || (head->kind == tk_false)) {
    // Create a new Boolean expression.
    NodeID expr_index = context_new_node(self->context);
    Node* expr = context_get_nodeptr(self->context, expr_index);
    expr->kind = nk_bool_expr;
    expr->start = head->start;
    expr->end   = head->end;
    expr->bits.bool_expr = self->context->source[head->start] == 't';

    return expr_index;
  }

  // Parse an integer or float literal.
  if ((head->kind == tk_integer) || (head->kind == tk_float)) {
    // Extract the float value of the token.
    size_t len = head->end - head->start;
    char str[len + 1];
    memcpy(str, self->context->source + head->start, len);
    str[len] = 0;

    // Create a new number expression.
    NodeID expr_index = context_new_node(self->context);
    Node* expr = context_get_nodeptr(self->context, expr_index);
    expr->start = head->start;
    expr->end   = head->end;

    if (head->kind == tk_integer) {
      expr->kind = nk_integer_expr;
      expr->bits.integer_expr = atoi(str);
    } else {
      expr->kind = nk_float_expr;
      expr->bits.float_expr = atof(str);
    }

    return expr_index;
  }

  // Parse a declaration reference.
  if (head->kind == tk_name) {
    // Create a new declaration reference.
    NodeID expr_index = context_new_node(self->context);
    Node* expr = context_get_nodeptr(self->context, expr_index);
    expr->kind  = nk_declref_expr;
    expr->start = head->start;
    expr->end   = head->end;
    expr->bits.declref_expr = *head;

    return expr_index;
  }

  /// Parse a parenthesized expression.
  if (head->kind == tk_l_paren) {
    size_t start = head->start;
    size_t end = 0;

    // Parse the sub-expression.
    NodeID subexpr_index = parse_expr(self, report_diag);

    // Parse the closing parenthesis.
    Token* tail = peek(self);
    if (tail && tail->kind == tk_r_paren) {
      end = tail->end;
      consume(self);
    } else {
      end = context_get_nodeptr(self->context, subexpr_index)->end;
      ParseError error = { end, "missing closing parenthesis" };
      report_diag(error, self);
    }

    // Create a new node.
    NodeID expr_index = context_new_node(self->context);
    Node* expr = context_get_nodeptr(self->context, expr_index);
    expr->kind  = nk_paren_expr;
    expr->start = start;
    expr->end   = end;
    expr->bits.paren_expr = subexpr_index;

    return expr_index;
  }

  ParseError error = { head->start, "expected expression" };
  report_diag(error, self);
  return create_error_node(self->context, head->start, head->end);
}

NodeID parse_post_expr(ParserState* self, ParseErrorCallback report_diag) {
  NodeID subexpr_index = parse_primary_expr(self, report_diag);
  if (context_get_nodeptr(self->context, subexpr_index)->kind == nk_error) {
    return subexpr_index;
  }

  // Parse the trailing of a postfix expression.
  Token* next = NULL;
  while ((next = peek(self))) {
    Node* subexpr = context_get_nodeptr(self->context, subexpr_index);
    size_t start  = subexpr->start;
    size_t end    = subexpr->end;

    // Parse a member expression.
    if (next->kind == tk_dot) {
      consume(self);
      next = peek(self);
      if (next && next->kind == tk_name) {
        end = next->end;
        consume(self);
      } else {
        subexpr->kind = nk_error;
        ParseError error = { end, "expected member name" };
        report_diag(error, self);
        return subexpr_index;
      }

      // Create a new node.
      NodeID expr_index = context_new_node(self->context);
      Node* expr = context_get_nodeptr(self->context, expr_index);
      expr->kind   = nk_member_expr;
      expr->start  = start;
      expr->end    = end;
      expr->bits.member_expr.base = subexpr_index;
      expr->bits.member_expr.member = *next;

      subexpr_index = expr_index;
      continue;
    }

    // Parse a call expression.
    if (next->kind == tk_l_paren) {
      consume(self);

      // Parse a list of arguments.
      NodeID argv[MAX_PARAM_COUNT] = { 0 };
      size_t argc = parse_expr_list(self, argv, report_diag);

      // Parse the closing parenthesis.
      next = peek(self);
      if (next && next->kind == tk_r_paren) {
        end = next->end;
        consume(self);
      } else {
        if (argc > 0) {
          end = context_get_nodeptr(self->context, argv[argc - 1])->end;
        } else if (next) {
          end = next->start;
        }
        ParseError error = { end, "missing closing parenthesis" };
        report_diag(error, self);
      }

      // Create a new node.
      NodeID expr_index = context_new_node(self->context);
      Node* expr = context_get_nodeptr(self->context, expr_index);
      expr->kind  = nk_apply_expr;
      expr->start = start;
      expr->end   = end;
      expr->bits.apply_expr.callee = subexpr_index;
      expr->bits.apply_expr.argc = argc;
      if (argc > 0) {
        expr->bits.apply_expr.argv = malloc(argc * sizeof(NodeID));
        memcpy(expr->bits.apply_expr.argv, argv, argc * sizeof(NodeID));
      } else {
        expr->bits.apply_expr.argv = NULL;
      }

      subexpr_index = expr_index;
      continue;
    }

    // No more trailing.
    break;
  }

  return subexpr_index;
}

NodeID parse_pre_expr(ParserState* self, ParseErrorCallback report_diag) {
  // Attempt to parse a prefix operator.
  Token* next = peek(self);
  if (next && token_isprefix(next->kind)) {
    Token op = *consume(self);

    // Parse the operand.
    NodeID subexpr_index = parse_expr(self, report_diag);

    // Create a new node.
    NodeID expr_index = context_new_node(self->context);
    Node* expr = context_get_nodeptr(self->context, expr_index);
    expr->kind  = nk_unary_expr;
    expr->start = op.start;
    expr->end   = context_get_nodeptr(self->context, subexpr_index)->end;
    expr->bits.unary_expr.op = op;
    expr->bits.unary_expr.subexpr = subexpr_index;

    return expr_index;
  }

  return parse_post_expr(self, report_diag);
}

NodeID parse_infix_expr(ParserState* self, unsigned int prec, ParseErrorCallback report_diag) {
  // Parse the left hand side.
  NodeID lhs_index = parse_pre_expr(self, report_diag);
  if (context_get_nodeptr(self->context, lhs_index)->kind == nk_error) {
    return lhs_index;
  }

  // Parse the trailing of a binary expression.
  Token* next = NULL;
  unsigned int current_prec = prec;

  while (current_prec <= SHIFT_PRECEDENCE) {
    // Bail out if the next token is not an operator.
    next = peek(self);
    if (!next || ((next->kind & TOK_OPER_BIT) != TOK_OPER_BIT)) { break; }

    // Check if the next token is an operator in which we're interested.
    if ((next->kind & current_prec) == current_prec) {
      Token op = *consume(self);

      // Parse the right hand side.
      NodeID rhs_index = (current_prec == SHIFT_PRECEDENCE)
        ? parse_pre_expr(self, report_diag)
        : parse_infix_expr(self, current_prec << 1, report_diag);

      // Create a new node.
      NodeID expr_index = context_new_node(self->context);
      Node* expr = context_get_nodeptr(self->context, expr_index);
      expr->kind  = nk_binary_expr;
      expr->start = op.start;
      expr->end   = context_get_nodeptr(self->context, rhs_index)->end;
      expr->bits.binary_expr.op  = op;
      expr->bits.binary_expr.lhs = lhs_index;
      expr->bits.binary_expr.rhs = rhs_index;

      lhs_index = expr_index;
      current_prec = prec;
      continue;
    }

    current_prec = current_prec << 1;
  }

  return lhs_index;
}

NodeID parse_expr(ParserState* self, ParseErrorCallback report_diag) {
  return parse_infix_expr(self, ASSIGNMENT_PRECEDENCE, report_diag);
}

// ------------------------------------------------------------------------------------------------
// MARK: Statements
// ------------------------------------------------------------------------------------------------

/// Parses a sequence of statements.
size_t parse_stmt_list(ParserState* self,
                       NodeID** stmtv,
                       TokenKind terminator,
                       ParseErrorCallback report_diag)
{
  NodeID* buffer = malloc(INITIAL_VEC_CAPACITY * sizeof(NodeID));
  size_t count = 0;
  size_t capacity = INITIAL_VEC_CAPACITY;

  // Parse the statements.
  Token* next;
  while ((next = peek(self))) {
    // Skip any number of leading semicolons.
    if (next->kind == tk_semicolon) {
      consume(self);
      continue;
    }

    // Stop if we found the terminator.
    if (next->kind == terminator) { break; }

    // Resize the statement buffer if necessary.
    if (count == capacity) {
      NodeID* new_buffer = malloc(capacity * 2 * sizeof(NodeID));
      memcpy(new_buffer, buffer, count * sizeof(NodeID));
      free(buffer);
      buffer = new_buffer;
    }

    // Parse a statement.
    buffer[count] = parse_stmt(self, report_diag);
    bool has_error = context_get_nodeptr(self->context, buffer[count])->kind == nk_error;
    count++;

    // Upon failure, recover at the next statement delimiter.
    if (has_error) {
      while((next = peek(self)) && !is_stmt_delimiter(next, self->context, terminator)) {
        consume(self);
      }
    }
  }

  *stmtv = buffer;
  return count;
}

NodeID parse_brace_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_l_brace));

  // Create a new node, representing the brace statement.
  NodeID stmt_index = context_new_node(self->context);
#define the_stmt context_get_nodeptr(self->context, stmt_index)
  the_stmt->kind  = nk_brace_stmt;
  the_stmt->start = next->start;
  the_stmt->bits.brace_stmt.parent = self->scope;
  the_stmt->bits.brace_stmt.last_decl = NULL;
  self->scope = stmt_index;

  // Parse the statements.
  NodeID* buffer;
  size_t count = parse_stmt_list(self, &buffer, tk_r_brace, report_diag);

  // Parse the closing brace.
  next = peek(self);
  if (next && (next->kind == tk_r_brace)) {
    the_stmt->end = next->end;
    consume(self);
  } else {
    if (count > 0) {
      the_stmt->end = context_get_nodeptr(self->context, buffer[count - 1])->end;
    } else if (next) {
      the_stmt->end = next->start;
    } else {
      the_stmt->end = strlen(self->context->source);
    }
    ParseError error = { the_stmt->end, "missing closing brace" };
    report_diag(error, self);
  }

  // Store the list of statements.
  the_stmt->bits.brace_stmt.stmtc = count;
  the_stmt->bits.brace_stmt.stmtv = buffer;

  self->scope = the_stmt->bits.brace_stmt.parent;
  return stmt_index;
#undef the_stmt
}

NodeID parse_if_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_if));

  // Create a new node.
  NodeID stmt_index = context_new_node(self->context);
#define the_stmt context_get_nodeptr(self->context, stmt_index)
  the_stmt->kind  = nk_if_stmt;
  the_stmt->start = next->start;

  // Parse the condition.
  the_stmt->bits.if_stmt.cond = parse_expr(self, report_diag);

  // Parse the "then" branch.
  next = peek(self);
  if (next && (next->kind == tk_l_brace)) {
    NodeID branch_index = parse_brace_stmt(self, report_diag);
    the_stmt->bits.if_stmt.then_ = branch_index;
    the_stmt->end = context_get_nodeptr(self->context, branch_index)->end;
  } else {
    size_t end = next ? next->start : strlen(self->context->source);
    the_stmt->bits.if_stmt.then_ = create_error_node(self->context, end, end);
    the_stmt->end = end;
    ParseError error = { end, "expected '{' after 'if' condition" };
    report_diag(error, self);
  }

  // Parse the "else" branch, if any.
  next = peek(self);
  if (next && (next->kind == tk_else)) {
    consume(self);
    NodeID branch_index = parse_stmt(self, report_diag);
    the_stmt->bits.if_stmt.else_ = branch_index;
    the_stmt->end = context_get_nodeptr(self->context, branch_index)->end;
  } else {
    the_stmt->bits.if_stmt.else_ = ~0;
  }

  return stmt_index;
#undef the_stmt
}

NodeID parse_while_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_while));

  // Create a new node.
  NodeID stmt_index = context_new_node(self->context);
#define the_stmt context_get_nodeptr(self->context, stmt_index)
  the_stmt->kind  = nk_while_stmt;
  the_stmt->start = next->start;

  // Parse the condition.
  the_stmt->bits.while_stmt.cond = parse_expr(self, report_diag);

  // Parse the body of the statement.
  next = peek(self);
  if (next && (next->kind == tk_l_brace)) {
    NodeID branch_index = parse_brace_stmt(self, report_diag);
    the_stmt->bits.while_stmt.body = branch_index;
    the_stmt->end = context_get_nodeptr(self->context, branch_index)->end;
  } else {
    size_t end = next ? next->start : strlen(self->context->source);
    the_stmt->bits.while_stmt.body = create_error_node(self->context, end, end);
    the_stmt->end = end;
    ParseError error = { end, "expected '{' after 'while' condition" };
    report_diag(error, self);
  }

  return stmt_index;
#undef the_stmt
}

NodeID parse_brk_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_brk));

  // Create a new node.
  NodeID stmt_index = context_new_node(self->context);
  Node* stmt = context_get_nodeptr(self->context, stmt_index);
  stmt->kind  = nk_brk_stmt;
  stmt->start = next->start;
  stmt->end   = next->end;

  return stmt_index;
}

NodeID parse_nxt_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_nxt));

  // Create a new node.
  NodeID stmt_index = context_new_node(self->context);
  Node* stmt = context_get_nodeptr(self->context, stmt_index);
  stmt->kind  = nk_nxt_stmt;
  stmt->start = next->start;
  stmt->end   = next->end;

  return stmt_index;
}

NodeID parse_ret_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = consume(self);
  assert(next && (next->kind == tk_ret));

  // Create a new node.
  NodeID stmt_index = context_new_node(self->context);
#define the_stmt context_get_nodeptr(self->context, stmt_index)
  the_stmt->kind  = nk_ret_stmt;
  the_stmt->start = next->start;

  // Parse the return value.
  NodeID expr_index = parse_expr(self, report_diag);
  the_stmt->bits.ret_stmt = expr_index;
  the_stmt->end = context_get_nodeptr(self->context, expr_index)->end;

  return stmt_index;
#undef the_stmt
}

NodeID parse_stmt(ParserState* self, ParseErrorCallback report_diag) {
  Token* next = peek(self);
  if (next == NULL) {
    size_t loc = strlen(self->context->source);
    ParseError error = { loc, "expected statement" };
    report_diag(error, self);
    return create_error_node(self->context, loc, loc);
  }

  // Attempt to parse a declaration.
  if ((next->kind & TOK_DECL_BIT) == TOK_DECL_BIT) {
    return parse_decl(self, report_diag);
  }

  // Attempt to parse a statement.
  switch (next->kind) {
    case tk_l_brace : return parse_brace_stmt(self, report_diag);
    case tk_if      : return parse_if_stmt(self, report_diag);
    case tk_while   : return parse_while_stmt(self, report_diag);
    case tk_brk     : return parse_brk_stmt(self, report_diag);
    case tk_nxt     : return parse_nxt_stmt(self, report_diag);
    case tk_ret     : return parse_ret_stmt(self, report_diag);
    default: break;
  }

  // Parse an expression and wrap it inside a statement.
  NodeID expr_index = parse_expr(self, report_diag);
  NodeID stmt_index = context_new_node(self->context);
  Node* stmt = context_get_nodeptr(self->context, stmt_index);
  stmt->kind  = nk_expr_stmt;
  stmt->start = context_get_nodeptr(self->context, expr_index)->start;
  stmt->end   = context_get_nodeptr(self->context, expr_index)->end;
  stmt->bits.expr_stmt = expr_index;

  return stmt_index;
}

// ------------------------------------------------------------------------------------------------
// MARK: Top-level
// ------------------------------------------------------------------------------------------------

NodeID create_top_decl(Context* context, NodeID* stmtv, size_t start, size_t end) {
  size_t byte_count = (end - start) * sizeof(NodeID);
  NodeID* buffer = malloc(byte_count);
  memcpy(buffer, stmtv + start, byte_count);

  NodeID decl_index = context_new_node(context);
  Node* decl = context_get_nodeptr(context, decl_index);
  decl->kind  = nk_top_decl;
  decl->start = context_get_nodeptr(context, stmtv[start])->start;
  decl->end   = context_get_nodeptr(context, stmtv[end - 1])->end;
  decl->bits.top_decl.stmtc = end - start;
  decl->bits.top_decl.stmtv = buffer;

  return decl_index;
}

NodeID parse(ParserState* self, NodeID** declv, ParseErrorCallback report_diag) {
  // Parse a sequence of "top-level" nodes.
  NodeID* stmtv;
  size_t stmtc = parse_stmt_list(self, &stmtv, tk_eof, report_diag);
  if (stmtc == 0) {
    *declv = NULL;
    return 0;
  }

  // Allocate the return buffer.
  *declv = malloc(stmtc * sizeof(NodeID));
  size_t count = 0;

  // Gather consecutive expressions and statements nodes into top-level declarations.
  size_t start = 0;
  for (size_t i = 0; i < stmtc; ++i) {
    if ((context_get_nodeptr(self->context, stmtv[i])->kind & NODE_DECL_BIT) == NODE_DECL_BIT) {
      // Wrap previous exprs and stmts into a top-level decl.
      if (start < i) {
        (*declv)[count] = create_top_decl(self->context, stmtv, start, i);
        count++;
      }

      (*declv)[count] = stmtv[i];
      count++;
      start = i + 1;
    }
  }

  // Wrap the remaining non-declaration nodes, if necessary.
  if (start < stmtc) {
    (*declv)[count] = create_top_decl(self->context, stmtv, start, stmtc);
    count++;
  }

  return count;
}
