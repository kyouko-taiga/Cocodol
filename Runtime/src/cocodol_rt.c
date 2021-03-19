#include "cocodol_rt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void _cocodol_drop(int64_t _0, int64_t _1) {
  if ((_1 != 0) && ((_0 & 3) == COCODOL_RT_FUNCTION)) {
    void*       base      = (void*)_1 - sizeof(int64_t);
    int64_t     env_size  = *((int64_t*)base);
    AnyObject*  env       = (AnyObject*)_1;

    for (int64_t i = 0; i < env_size; ++i) {
      _cocodol_drop(env[i]._0, env[i]._1);
    }

#ifdef DEBUG
    printf("drop function env %p\n", base);
#endif
    free(base);
  }
}

AnyObject _cocodol_copy(int64_t _0, int64_t _1) {
  AnyObject dst = { _0, _1 };

  if ((_1 != 0) && ((_0 & 3) == COCODOL_RT_FUNCTION)) {
    void*       base      = (void*)_1 - sizeof(int64_t);
    int64_t     env_size  = *((int64_t*)base);
    AnyObject*  env       = (AnyObject*)_1;
    void*       new_base  = malloc(sizeof(int64_t) + env_size * sizeof(AnyObject));
#ifdef DEBUG
    printf("copy function env %p to %p\n", base, new_base);
#endif
    AnyObject*  new_env   = (AnyObject*)(new_base + sizeof(int64_t));
    for (int64_t i = 0; i < env_size; ++i) {
      new_env[i] = _cocodol_copy(env[i]._0, env[i]._1);
    }
    dst._1 = (int64_t)new_env;
  }

  return dst;
}

void _cocodol_print(int64_t _0, int64_t _1) {
//  printf("%lli, %lli\n", _0, _1); return;
  switch (_0) {
    case COCODOL_RT_JUNK:
      fputs("$junk\n", stdout);
      break;

    case COCODOL_RT_PRINT:
      fputs("$function\n", stdout);

    case COCODOL_RT_BOOL:
      fputs(_1 ? "true\n" : "false\n", stdout);
      break;

    case COCODOL_RT_INTEGER:
      printf("%lli\n", _1);
      break;

    case COCODOL_RT_FLOAT:
      printf("%f\n", *((double*)(&_1)));
      break;

    default:
      if ((_0 & 3) == COCODOL_RT_FUNCTION) {
        fputs("$function\n", stdout);
      } else {
        fputs("$object\n", stdout);
      }
      break;
  }
}

AnyObject _cocodol_binop(int64_t _a0, int64_t _a1, int64_t _b0, int64_t _b1, uint32_t op) {
  AnyObject res;

  switch (_a0) {
    case COCODOL_RT_BOOL:
      if (_b0 != COCODOL_RT_BOOL) { abort(); }
      switch (op) {
        case TOK_AND:
          res._0 = COCODOL_RT_BOOL;
          res._1 = _a1 & _b1;
          return res;

        case TOK_OR:
          res._0 = COCODOL_RT_BOOL;
          res._1 = _a1 | _b1;
          return res;

        default:
          abort();
      }

    case COCODOL_RT_INTEGER:
      if (_b0 != COCODOL_RT_INTEGER) { abort(); }
      switch (op) {
        case TOK_L_SHIFT:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 << _b1;
          return res;

        case TOK_R_SHIFT:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 >> _b1;
          return res;

        case TOK_STAR:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 * _b1;
          return res;

        case TOK_SLASH:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 / _b1;
          return res;

        case TOK_PERCENT:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 % _b1;
          return res;

        case TOK_PLUS:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 + _b1;
          return res;

        case TOK_MINUS:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 - _b1;
          return res;

        case TOK_PIPE:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 | _b1;
          return res;

        case TOK_AMP:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 & _b1;
          return res;

        case TOK_CARET:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1 ^ _b1;
          return res;

        case TOK_LT:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (_a1 < _b1) ? 1 : 0;
          return res;

        case TOK_LE:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (_a1 <= _b1) ? 1 : 0;
          return res;

        case TOK_GT:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (_a1 > _b1) ? 1 : 0;
          return res;

        case TOK_GE:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (_a1 >= _b1) ? 1 : 0;
          return res;

        case TOK_EQ:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (_a1 == _b1) ? 1 : 0;
          return res;

        case TOK_NE:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (_a1 != _b1) ? 1 : 0;
          return res;

        default:
          abort();
      }

    case COCODOL_RT_FLOAT:
      if (_b0 != COCODOL_RT_FLOAT) { abort(); }
      switch (op) {
        case TOK_STAR:
          res._0 = COCODOL_RT_FLOAT;
          *(double*)(&res._1) = *(double*)(&_a1) * *(double*)(&_b1);
          return res;

        case TOK_SLASH:
          res._0 = COCODOL_RT_FLOAT;
          *(double*)(&res._1) = *(double*)(&_a1) / *(double*)(&_b1);
          return res;

        case TOK_PERCENT:
          res._0 = COCODOL_RT_FLOAT;
          *(double*)(&res._1) = fmod(*(double*)(&_a1), *(double*)(&_b1));
          return res;

        case TOK_PLUS:
          res._0 = COCODOL_RT_FLOAT;
          *(double*)(&res._1) = *(double*)(&_a1) + *(double*)(&_b1);
          return res;

        case TOK_MINUS:
          res._0 = COCODOL_RT_FLOAT;
          *(double*)(&res._1) = *(double*)(&_a1) - *(double*)(&_b1);
          return res;

        case TOK_LT:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (*(double*)(&_a1) < *(double*)(&_b1)) ? 1 : 0;
          return res;

        case TOK_LE:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (*(double*)(&_a1) <= *(double*)(&_b1)) ? 1 : 0;
          return res;

        case TOK_GT:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (*(double*)(&_a1) > *(double*)(&_b1)) ? 1 : 0;
          return res;

        case TOK_GE:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (*(double*)(&_a1) >= *(double*)(&_b1)) ? 1 : 0;
          return res;

        case TOK_EQ:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (*(double*)(&_a1) == *(double*)(&_b1)) ? 1 : 0;
          return res;

        case TOK_NE:
          res._0 = COCODOL_RT_BOOL;
          res._1 = (*(double*)(&_a1) != *(double*)(&_b1)) ? 1 : 0;
          return res;

        default:
          abort();
      }

    default:
      abort();
  }
}

AnyObject _cocodol_unop(int64_t _a0, int64_t _a1, uint32_t op) {
  AnyObject res;

  switch (_a0) {
    case COCODOL_RT_BOOL:
      switch (op) {
        case TOK_NOT:
          res._0 = COCODOL_RT_BOOL;
          res._1 = _a1 ? 0 : 1;
          return res;

        default:
          abort();
      }

    case COCODOL_RT_INTEGER:
      switch (op) {
        case TOK_PLUS:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = _a1;
          return res;

        case TOK_MINUS:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = -_a1;
          return res;

        case TOK_TILDE:
          res._0 = COCODOL_RT_INTEGER;
          res._1 = ~_a1;
          return res;

        default:
          abort();
      }

    case COCODOL_RT_FLOAT:
      switch (op) {
        case TOK_PLUS:
          res._0 = COCODOL_RT_FLOAT;
          res._1 = _a1;
          return res;

        case TOK_MINUS:
          res._0 = COCODOL_RT_FLOAT;
          *(double*)(&res._1) = -(*(double*)(&_a1));
          return res;

        default:
          abort();
      }

    default:
      abort();
  }
}
