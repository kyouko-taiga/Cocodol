#ifndef COCODOL_LEXER_H
#define COCODOL_LEXER_H

#include "common.h"

/// The state of a lexer.
typedef struct LexerState {

  /// The input string representing the program source.
  const char* source;

  /// The index from which the input is being tokenized.
  size_t index;

} LexerState;

/// Initializes a lexer's state.
void lexer_init(LexerState*, const char* source);

/// Deinitializes a lexer's state.
void lexer_deinit(LexerState*);

/// Returns the next token in the stream.
///
/// If a token is successfully tokenized, this function returns `true` and stores the value of the
/// token at `token`. Otherwise, it returns `false` and the value at `token` is undefined.
bool lexer_next(LexerState*, struct Token* token);

#endif
