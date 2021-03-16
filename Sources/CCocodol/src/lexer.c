#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "token.h"

#define lexer_stream(self)  (self->source + self->index)
#define lexer_peek(self)    (self->source[self->index])

int is_ident_char(int ch) {
  return (isalnum(ch) || ch == '_');
}

void lexer_init(LexerState* self, const char* source) {
  self->source = source;
  self->index = 0;
}

void lexer_deinit(LexerState* self) {
  self->source = 0;
  self->index = 0;
}

/// Consumes the longest sequence of characters tht satisfy the given predicate from the lexer's
/// input buffer.
size_t lexer_take_while(LexerState* self, int (*predicate)(int)) {
  const char* stream = lexer_stream(self);
  if (!stream[0]) {
    return 0;
  }

  size_t i = 0;
  while (stream[i] && predicate(stream[i])) {
    i += 1;
  }

  self->index += i;
  return i;
}

bool lexer_next(LexerState* self, Token* token) {
  char ch;

  while (lexer_peek(self)) {
    // Skip all leading whitespace characters.
    lexer_take_while(self, &isspace);

    // Skip the remainder of the line if we recognized a comment.
    if (strncmp(lexer_stream(self), "//", 2) == 0) {
      while ((ch = lexer_peek(self))) {
        self->index++;
        if ((ch == '\n') || (ch == '\r')) {
          break;
        }
      }
    } else {
      // We're ready to consume a token.
      break;
    }
  }

  if (!(ch = lexer_peek(self))) {
    return false;
  }
  token->start = self->index;

  // Scan identifiers and keywords.
  if (isalpha(ch) || (ch == '_')) {
    const char* id = lexer_stream(self);
    lexer_take_while(self, &is_ident_char);
    token->end = self->index;

    if (strncmp(id, "if", 2) == 0) {
      token->kind = tk_if;
    } else if (strncmp(id, "or", 2) == 0) {
      token->kind = tk_or;
    } else if (strncmp(id, "var", 3) == 0) {
      token->kind = tk_var;
    } else if (strncmp(id, "fun", 3) == 0) {
      token->kind = tk_fun;
    } else if (strncmp(id, "ret", 3) == 0) {
      token->kind = tk_ret;
    } else if (strncmp(id, "obj", 3) == 0) {
      token->kind = tk_obj;
    } else if (strncmp(id, "brk", 3) == 0) {
      token->kind = tk_brk;
    } else if (strncmp(id, "nxt", 3) == 0) {
      token->kind = tk_nxt;
    } else if (strncmp(id, "and", 3) == 0) {
      token->kind = tk_and;
    } else if (strncmp(id, "true", 4) == 0) {
      token->kind = tk_true;
    } else if (strncmp(id, "else", 4) == 0) {
      token->kind = tk_else;
    } else if (strncmp(id, "false", 5) == 0) {
      token->kind = tk_false;
    } else if (strncmp(id, "while", 5) == 0) {
      token->kind = tk_while;
    } else {
      token->kind = tk_name;
    }

    return true;
  }

  // Scan for numbers.
  if (isdigit(ch)) {
    token->kind = tk_integer;
    lexer_take_while(self, &isdigit);

    // Look for a floating part.
    if (lexer_peek(self) == '.') {
      self->index++;
      if (lexer_take_while(self, &isdigit) == 0) {
        self->index--;
      }
      token->kind = tk_float;
    }

    token->end = self->index;
    return true;
  }

  // Scan operators and punctuation.
  char next = self->source[self->index + 1];
  switch (ch) {
    case '+': token->kind = tk_plus;      break;
    case '-': token->kind = tk_minus;     break;
    case '*': token->kind = tk_star;      break;
    case '/': token->kind = tk_slash;     break;
    case '%': token->kind = tk_percent;   break;
    case '|': token->kind = tk_pipe;      break;
    case '&': token->kind = tk_amp;       break;
    case '^': token->kind = tk_caret;     break;
    case '~': token->kind = tk_tilde;     break;
    case '.': token->kind = tk_dot;       break;
    case ':': token->kind = tk_colon;     break;
    case ';': token->kind = tk_semicolon; break;
    case ',': token->kind = tk_comma;     break;
    case '(': token->kind = tk_l_paren;   break;
    case ')': token->kind = tk_r_paren;   break;
    case '{': token->kind = tk_l_brace;   break;
    case '}': token->kind = tk_r_brace;   break;
    case EOF: token->kind = tk_eof;       break;

    case '!':
      if (next == '=') {
        self->index += 2;
        token->kind = tk_ne;
        token->end = self->index;
        return true;
      }
      token->kind = tk_not;
      break;

    case '<':
      if (next == '=') {
        self->index += 2;
        token->kind = tk_le;
        token->end = self->index;
        return true;
      } else if (next == '<') {
        self->index += 2;
        token->kind = tk_l_shift;
        token->end = self->index;
        return true;
      }
      token->kind = tk_lt;
      break;

    case '>':
      if (next == '=') {
        self->index += 2;
        token->kind = tk_ge;
        token->end = self->index;
        return true;
      } else if (next == '>') {
        self->index += 2;
        token->kind = tk_r_shift;
        token->end = self->index;
        return true;
      }
      token->kind = tk_gt;
      break;

    case '=':
      if (next == '=') {
        self->index += 2;
        token->kind = tk_eq;
        token->end = self->index;
        return true;
      }
      token->kind = tk_assign;
      break;

    default:
      token->kind = tk_error;
      break;
  }

  self->index++;
  token->end = self->index;
  return true;
}
