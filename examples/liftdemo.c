#include "../src/vm/isa.h"
#include "../src/vm/mix.h"
#include "../src/asm/asm.h"
#include "../src/asm/expr.h"
#include "../src/rt/guard.h"
#include "../src/rt/protect.h"
#include <stdio.h>
#include <stdlib.h>

static uint64_t g_input;
static uint64_t th_input(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)a;(void)b;return g_input;}
static uint64_t th_guard(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)a;(void)b;return ftm_dbg_check();}

static uint64_t transform(uint64_t v){
    v=rol64(v,7);
    v=v^0xDEADBEEFCAFEBABEull;
    v=v+0x0123456789ABCDEFull;
    return v;
}

static const char* EXPR = "(rol(x,7) ^ 0xDEADBEEFCAFEBABE) + 0x0123456789ABCDEF";

static ftm_asm* prog(uint64_t inp){
    ftm_asm*a=asm_new();
    int good=asm_label(a);
    a_vmcall(a,0);
    expr_var v[1]={{"x",0}};
    if(expr_compile(a,EXPR,1,v,1,2)){ printf("expr parse failed\n"); }
    a_ldi(a,2,transform(inp));
    a_cmp(a,1,2);
    a_jmp(a,OP_JZ,good);
    a_ldi(a,0,0); a_halt(a);
    asm_bind(a,good);
    a_ldi(a,0,1); a_chk(a); a_halt(a);
    return a;
}

static int run(uint8_t*code,uint32_t len,uint64_t seed,uint64_t inp){
    vm_thunk th[2]={th_input,th_guard};
    vm_ctx c; vm_seed(&c,seed);
    c.code=code; c.code_len=len; c.thunks=th; c.nthunks=2;
    g_input=inp;
    return vm_run(&c);
}

int main(void){
    uint64_t inp=0x1234ABCDF00Dull;

    ftm_asm*a=prog(inp);
    uint32_t l0; uint64_t s0=0xACE0FBEE12ull;
    uint8_t*C0=asm_build(a,s0,&l0);
    printf("lifted len=%u\n",l0);
    printf("lift good -> %d (want 1)\n", run(C0,l0,s0,inp));
    printf("lift bad  -> %d (want 0)\n", run(C0,l0,s0,inp+1));

    poly_cfg cfg={0};
    cfg.junk=3; cfg.opaque=4; cfg.traps=2; cfg.chk=2;
    cfg.poison_reg=1; cfg.trap_thunk=1; cfg.scratch_lo=8;
    cfg.seed=0x5150ull;
    ftm_asm*pp=asm_polymorph(a,&cfg);

    uint64_t pk=0x0FF1CE5511223344ull;
    ftm_blob blob=ftm_pack(pp,0xB0BAull,pk);
    vm_thunk th[2]={th_input,th_guard};
    printf("packed blob=%u\n",blob.len);
    g_input=inp;   printf("full good     -> %d (want 1)\n", ftm_exec(blob.data,blob.len,pk,th,2));
    g_input=inp+1; printf("full bad      -> %d (want 0)\n", ftm_exec(blob.data,blob.len,pk,th,2));
    g_input=inp;   printf("full wrongkey -> %d (want !=1)\n", ftm_exec(blob.data,blob.len,pk^1,th,2));

    ftm_blob_free(&blob);
    free(C0); asm_free(pp); asm_free(a);
    return 0;
}
