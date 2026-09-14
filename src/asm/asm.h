#ifndef FTM_ASM_H
#define FTM_ASM_H
#include <stdint.h>

typedef struct ftm_asm ftm_asm;

ftm_asm* asm_new(void);
void asm_free(ftm_asm*);

int  asm_label(ftm_asm*);
void asm_bind(ftm_asm*, int label);

void a_ldi(ftm_asm*, int r, uint64_t imm);
void a_mov(ftm_asm*, int d, int s);
void a_ld(ftm_asm*, int d, int s, int32_t off);
void a_st(ftm_asm*, int d, int32_t off, int s);
void a_bin(ftm_asm*, int op, int a, int b);
void a_imm(ftm_asm*, int op, int d, uint64_t imm);
void a_sh(ftm_asm*, int op, int d, int n);
void a_un(ftm_asm*, int op, int d);
void a_cmp(ftm_asm*, int a, int b);
void a_cmov(ftm_asm*, int d, int s);
void a_jmp(ftm_asm*, int op, int label);
void a_vmcall(ftm_asm*, int id);
void a_chk(ftm_asm*);
void a_chkx(ftm_asm*, int d);
void a_nop(ftm_asm*);
void a_halt(ftm_asm*);

uint8_t* asm_build(ftm_asm*, uint64_t seed, uint32_t* len);

typedef struct {
    uint64_t seed;
    int junk;
    int opaque;
    int traps;
    int chk;
    int poison_reg;
    int trap_thunk;
    int scratch_lo;
} poly_cfg;

ftm_asm* asm_polymorph(const ftm_asm* src, const poly_cfg* cfg);

#endif
