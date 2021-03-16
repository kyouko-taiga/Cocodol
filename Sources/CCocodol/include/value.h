#ifndef COCODOL_VALUE_H
#define COCODOL_VALUE_H

#include "common.h"

/// A runtime value.
typedef struct RuntimeValue {

  /// The value's kind.
  enum {
    rv_junk     ,
    rv_print    ,
    rv_lazy     ,
    rv_function ,
    rv_bool     ,
    rv_integer  ,
    rv_float    ,
  } kind;

  /// The bits of the runtime value.
  union {

    NodeID lazy_v;

    struct {
      NodeID decl;
      struct SymTable* env;
    } function_v;

    bool bool_v;

    long int integer_v;

    float float_v;

  } bits;

} RuntimeValue;

#endif
