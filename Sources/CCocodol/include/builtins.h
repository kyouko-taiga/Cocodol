#ifndef COCODOL_BUILTINS_H
#define COCODOL_BUILTINS_H

#include <math.h>

#define COCODOL_ILSH(a, b) ((a) << (b))
#define COCODOL_IRSH(a, b) ((a) >> (b))
#define COCODOL_IMUL(a, b) ((a) * (b))
#define COCODOL_IDIV(a, b) ((a) / (b))
#define COCODOL_IMOD(a, b) ((a) % (b))
#define COCODOL_IADD(a, b) ((a) + (b))
#define COCODOL_ISUB(a, b) ((a) - (b))
#define COCODOL_IAND(a, b) ((a) & (b))
#define COCODOL_IOR(a, b)  ((a) | (b))
#define COCODOL_IXOR(a, b) ((a) ^ (b))

#define COCODOL_FMUL(a, b) ((a) * (b))
#define COCODOL_FDIV(a, b) ((a) / (b))
#define COCODOL_FMOD(a, b) (fmod(a, b))
#define COCODOL_FADD(a, b) ((a) + (b))
#define COCODOL_FSUB(a, b) ((a) - (b))

#define COCODOL_LT(a, b)   ((a) < (b))
#define COCODOL_LE(a, b)   ((a) <= (b))
#define COCODOL_GT(a, b)   ((a) > (b))
#define COCODOL_GE(a, b)   ((a) >= (b))
#define COCODOL_EQ(a, b)   ((a) == (b))
#define COCODOL_NE(a, b)   ((a) != (b))

#define COCODOL_LAND(a, b) ((a) && (b))
#define COCODOL_LOR(a, b)  ((a) || (b))

#endif
