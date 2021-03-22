#ifndef COCODOL_TOKEN_H
#define COCODOL_TOKEN_H

#include "common.h"

#define TOK_DECL_BIT (1 << 16)
#define TOK_STMT_BIT (1 << 17)
#define TOK_OPER_BIT (1 << 18)
#define TOK_PRFX_BIT (1 << 19)

#define ASSIGNMENT_PRECEDENCE     (1 <<  8)
#define LOGICAL_OR_PRECEDENCE     (1 <<  9)
#define LOGICAL_AND_PRECEDENCE    (1 << 10)
#define COMPARISON_PRECEDENCE     (1 << 11)
#define ADDITION_PRECEDENCE       (1 << 12)
#define MULTIPLICATION_PRECEDENCE (1 << 13)
#define SHIFT_PRECEDENCE          (1 << 14)

/// The kind of a token.
typedef enum TokenKind: unsigned int {
  tk_error      =  0,

  tk_name       =  1,
  tk_true       =  2,
  tk_false      =  3,
  tk_integer    =  4,
  tk_float      =  5,
  tk_dot        =  6,
  tk_colon      =  7,
  tk_semicolon  =  8,
  tk_comma      =  9,
  tk_l_paren    = 10,
  tk_r_paren    = 11,
  tk_l_brace    = 12,
  tk_r_brace    = 13,
  tk_eof        = 14,

  tk_var        =  1 | TOK_DECL_BIT,
  tk_fun        =  2 | TOK_DECL_BIT,
  tk_obj        =  3 | TOK_DECL_BIT,

  tk_if         =  1 | TOK_STMT_BIT,
  tk_else       =  2 | TOK_STMT_BIT,
  tk_while      =  3 | TOK_STMT_BIT,
  tk_brk        =  4 | TOK_STMT_BIT,
  tk_nxt        =  5 | TOK_STMT_BIT,
  tk_ret        =  6 | TOK_STMT_BIT,

  tk_l_shift    =  1 | TOK_OPER_BIT | SHIFT_PRECEDENCE          ,
  tk_r_shift    =  2 | TOK_OPER_BIT | SHIFT_PRECEDENCE          ,
  tk_star       =  3 | TOK_OPER_BIT | MULTIPLICATION_PRECEDENCE ,
  tk_slash      =  4 | TOK_OPER_BIT | MULTIPLICATION_PRECEDENCE ,
  tk_percent    =  5 | TOK_OPER_BIT | MULTIPLICATION_PRECEDENCE ,
  tk_plus       =  6 | TOK_OPER_BIT | ADDITION_PRECEDENCE       | TOK_PRFX_BIT,
  tk_minus      =  7 | TOK_OPER_BIT | ADDITION_PRECEDENCE       | TOK_PRFX_BIT,
  tk_pipe       =  8 | TOK_OPER_BIT | ADDITION_PRECEDENCE       ,
  tk_amp        =  9 | TOK_OPER_BIT | ADDITION_PRECEDENCE       ,
  tk_caret      = 10 | TOK_OPER_BIT | ADDITION_PRECEDENCE       ,
  tk_lt         = 11 | TOK_OPER_BIT | COMPARISON_PRECEDENCE     ,
  tk_le         = 12 | TOK_OPER_BIT | COMPARISON_PRECEDENCE     ,
  tk_gt         = 13 | TOK_OPER_BIT | COMPARISON_PRECEDENCE     ,
  tk_ge         = 14 | TOK_OPER_BIT | COMPARISON_PRECEDENCE     ,
  tk_eq         = 15 | TOK_OPER_BIT | COMPARISON_PRECEDENCE     ,
  tk_ne         = 16 | TOK_OPER_BIT | COMPARISON_PRECEDENCE     ,
  tk_and        = 17 | TOK_OPER_BIT | LOGICAL_AND_PRECEDENCE    ,
  tk_or         = 18 | TOK_OPER_BIT | LOGICAL_OR_PRECEDENCE     ,
  tk_assign     = 19 | TOK_OPER_BIT | ASSIGNMENT_PRECEDENCE     ,
  tk_not        = 20 | TOK_OPER_BIT                             | TOK_PRFX_BIT,
  tk_tilde      = 21 | TOK_OPER_BIT                             | TOK_PRFX_BIT,
} TokenKind;

/// A token.
typedef struct Token {

  /// The token's kind.
  TokenKind kind;

  /// The index at which the token starts in the source input.
  size_t start;

  /// The index at which the token ends in the source input.
  size_t end;

} Token;

/// Returns the length of the given token's textual representation.
static inline size_t token_text_len(Token* token) {
  return token->end - token->start;
}

/// Returns whether the textual representations of two token are equal.
bool token_text_equal(struct Context* context, Token* lhs, Token* rhs);

#endif
