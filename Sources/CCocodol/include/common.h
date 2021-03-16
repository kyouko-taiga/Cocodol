#ifndef COCODOL_COMMON_H
#define COCODOL_COMMON_H

#include <stdbool.h>
#include <stddef.h>

struct  Context;
struct  DeclList;
struct  LexerState;
struct  EvalError;
struct  EvalState;
struct  ParseError;
struct  ParserState;
struct  Node;
enum    NodeKind;
struct  SymTable;
struct  Token;
enum    TokenKind;
struct  RuntimeValue;

/// The type of a callback for runtime errors.
typedef void(*EvalErrorCallback)(struct EvalError, const struct EvalState*);

/// The type of a callback for parse errors.
typedef void(*ParseErrorCallback)(struct ParseError, const struct ParserState*);

/// The index of an AST node.
typedef size_t NodeID;

#endif
