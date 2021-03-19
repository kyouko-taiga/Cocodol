#ifndef COCODOL_RT_H
#define COCODOL_RT_H

#include <stdint.h>

// ------------------------------------------------------------------------------------------------
// MARK: Data type identifiers
// ------------------------------------------------------------------------------------------------

#define COCODOL_RT_JUNK           0b00000
#define COCODOL_RT_FUNCTION       0b00001
// #define COCODOL_RT_OBJECT         0b00010
#define COCODOL_RT_PRINT          0b00111
#define COCODOL_RT_BOOL           0b01011
#define COCODOL_RT_INTEGER        0b01111
#define COCODOL_RT_FLOAT          0b10011

// ------------------------------------------------------------------------------------------------
// MARK: Operator identifiers
// ------------------------------------------------------------------------------------------------

// Note that the value of these definitions must match those of the parser.

#define ASSIGNMENT_PRECEDENCE     (1 <<  8)
#define LOGICAL_OR_PRECEDENCE     (1 <<  9)
#define LOGICAL_AND_PRECEDENCE    (1 << 10)
#define COMPARISON_PRECEDENCE     (1 << 11)
#define ADDITION_PRECEDENCE       (1 << 12)
#define MULTIPLICATION_PRECEDENCE (1 << 13)
#define SHIFT_PRECEDENCE          (1 << 14)

#define TOK_OPER_BIT              (1 << 18)
#define TOK_PRFX_BIT              (1 << 19)

#define TOK_L_SHIFT               ( 1 | TOK_OPER_BIT | SHIFT_PRECEDENCE                        )
#define TOK_R_SHIFT               ( 2 | TOK_OPER_BIT | SHIFT_PRECEDENCE                        )
#define TOK_STAR                  ( 3 | TOK_OPER_BIT | MULTIPLICATION_PRECEDENCE               )
#define TOK_SLASH                 ( 4 | TOK_OPER_BIT | MULTIPLICATION_PRECEDENCE               )
#define TOK_PERCENT               ( 5 | TOK_OPER_BIT | MULTIPLICATION_PRECEDENCE               )
#define TOK_PLUS                  ( 6 | TOK_OPER_BIT | ADDITION_PRECEDENCE       | TOK_PRFX_BIT)
#define TOK_MINUS                 ( 7 | TOK_OPER_BIT | ADDITION_PRECEDENCE       | TOK_PRFX_BIT)
#define TOK_PIPE                  ( 8 | TOK_OPER_BIT | ADDITION_PRECEDENCE                     )
#define TOK_AMP                   ( 9 | TOK_OPER_BIT | ADDITION_PRECEDENCE                     )
#define TOK_CARET                 (10 | TOK_OPER_BIT | ADDITION_PRECEDENCE                     )
#define TOK_LT                    (11 | TOK_OPER_BIT | COMPARISON_PRECEDENCE                   )
#define TOK_LE                    (12 | TOK_OPER_BIT | COMPARISON_PRECEDENCE                   )
#define TOK_GT                    (13 | TOK_OPER_BIT | COMPARISON_PRECEDENCE                   )
#define TOK_GE                    (14 | TOK_OPER_BIT | COMPARISON_PRECEDENCE                   )
#define TOK_EQ                    (15 | TOK_OPER_BIT | COMPARISON_PRECEDENCE                   )
#define TOK_NE                    (16 | TOK_OPER_BIT | COMPARISON_PRECEDENCE                   )
#define TOK_AND                   (17 | TOK_OPER_BIT | LOGICAL_AND_PRECEDENCE                  )
#define TOK_OR                    (18 | TOK_OPER_BIT | LOGICAL_OR_PRECEDENCE                   )
#define TOK_ASSIGN                (19 | TOK_OPER_BIT | ASSIGNMENT_PRECEDENCE                   )
#define TOK_NOT                   (20 | TOK_OPER_BIT                             | TOK_PRFX_BIT)
#define TOK_TILDE                 (21 | TOK_OPER_BIT                             | TOK_PRFX_BIT)

// ------------------------------------------------------------------------------------------------
// MARK: Runtime library
// ------------------------------------------------------------------------------------------------

typedef struct AnyObject {
  int64_t _0, _1;
} AnyObject;

/// Deinitializes and deallocates the given value.
void _cocodol_drop      (int64_t _0, int64_t _1);

/// Copies the given value.
AnyObject _cocodol_copy (int64_t _0, int64_t _1);

/// Prints the given value.
void _cocodol_print     (int64_t _0, int64_t _1);

/// Applies the specified binary operator on the given operands.
AnyObject _cocodol_binop(int64_t _a0, int64_t _a1, int64_t _b0, int64_t _b1, uint32_t op);

/// Applies the specified binary operator on the given operand.
AnyObject _cocodol_unop (int64_t _a0, int64_t _a1, uint32_t op);

#endif
