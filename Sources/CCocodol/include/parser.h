#ifndef COCODOL_PARSER_H
#define COCODOL_PARSER_H

#include "common.h"
#include "lexer.h"
#include "token.h"

#define TOKEN_BUFFER_LENGTH 8

/// The state of a parser.
typedef struct ParserState {

  /// The AST context in which the source is being parsed.
  struct Context* context;

  /// The lexer that tokenizes the parser's input.
  LexerState lexer;

  /// The lookahead buffer.
  Token lookahead_buffer[TOKEN_BUFFER_LENGTH];

  /// The current start index of the lookahead buffer.
  size_t lookahead_start;

  /// The current end index of the lookahead buffer.
  size_t lookahead_end;

  /// The index from which the input is being parser.
  size_t index;

  /// The current lexical scope.
  NodeID scope;

  /// A pointer to arbitrary user data.
  void* user_data;

} ParserState;

/// A parse error.
typedef struct ParseError {

  /// The location of the error in the program source.
  size_t location;

  /// The error message.
  const char* message;

} ParseError;

/// Initializes a parser's state.
void parser_init(ParserState*, struct Context* context, void* user_data);

/// Deinitializes a parser's state.
void parser_deinit(ParserState*);

/// Parses a sequence of top-level declarations from the input buffer.
///
/// The function allocates an array of declaration at `declv` and returns its lenght. The caller is
/// responsible for disposing of the allocated the memory.
size_t parse(ParserState*, NodeID** declv, ParseErrorCallback);

/// Parses a single declaration and returns its index in the context.
NodeID parse_decl(ParserState* self, ParseErrorCallback);

/// Parses a single expression and returns its index in the context.
NodeID parse_expr(ParserState* self, ParseErrorCallback);

/// Parses a single statement or declaration and returns its index in the context.
NodeID parse_stmt(ParserState* self, ParseErrorCallback);

#endif
