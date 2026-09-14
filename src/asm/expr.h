#ifndef FTM_EXPR_H
#define FTM_EXPR_H
#include <stdint.h>
#include "asm.h"

typedef struct { const char* name; int reg; } expr_var;

int expr_compile(ftm_asm* a, const char* src, int dst,
                 const expr_var* vars, int nvars, int tmp_lo);

#endif
