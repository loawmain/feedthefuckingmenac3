#include "../src/vm/isa.h"
#include "../src/vm/mix.h"
#include "../src/asm/asm.h"
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

static ftm_asm* prog(uint64_t inp){
    ftm_asm*a=asm_new();
    int good=asm_label(a);
    a_vmcall(a,0);
    a_mov(a,1,0);
    a_sh(a,OP_ROL,1,7);
    a_imm(a,OP_XORI,1,0xDEADBEEFCAFEBABEull);
    a_imm(a,OP_ADDI,1,0x0123456789ABCDEFull);
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
    uint64_t inp=0xF00DF00DF00DF00Dull;
    uint64_t seedA=0xC0FFEE1234ull, seedB=0xBADA55EE99ull;

    ftm_asm*a=prog(inp);
    uint32_t la,lb;
    uint8_t*A=asm_build(a,seedA,&la);
    uint8_t*B=asm_build(a,seedB,&lb);

    printf("plain len=%u\n",la);
    printf("A good -> %d (want 1)\n", run(A,la,seedA,inp));
    printf("A bad  -> %d (want 0)\n", run(A,la,seedA,inp^1));
    printf("B good -> %d (want 1)\n", run(B,lb,seedB,inp));
    A[12]^=0xFF;
    printf("A tampered -> %d\n", run(A,la,seedA,inp));

    poly_cfg cfg={0};
    cfg.junk=3; cfg.opaque=4; cfg.traps=2;
    cfg.poison_reg=1; cfg.trap_thunk=1; cfg.scratch_lo=8;

    cfg.seed=0x1122334455ull;
    ftm_asm*p1=asm_polymorph(a,&cfg);
    cfg.seed=0x99AABBCCDDull;
    ftm_asm*p2=asm_polymorph(a,&cfg);

    uint32_t l1,l2;
    uint64_t s1=0x7777AAAAull, s2=0x314159265ull;
    uint8_t*P1=asm_build(p1,s1,&l1);
    uint8_t*P2=asm_build(p2,s2,&l2);

    printf("poly1 len=%u  poly2 len=%u  (plain=%u)\n",l1,l2,la);
    printf("P1 good -> %d (want 1)\n", run(P1,l1,s1,inp));
    printf("P1 bad  -> %d (want 0)\n", run(P1,l1,s1,inp^1));
    printf("P2 good -> %d (want 1)\n", run(P2,l2,s2,inp));
    printf("P2 bad  -> %d (want 0)\n", run(P2,l2,s2,inp^1));

    uint64_t pk=0xBEEFCAFE12345678ull;
    ftm_blob blob=ftm_pack(p1,0x424242ull,pk);
    vm_thunk th[2]={th_input,th_guard};
    printf("blob len=%u\n",blob.len);
    g_input=inp;   printf("packed good     -> %d (want 1)\n", ftm_exec(blob.data,blob.len,pk,th,2));
    g_input=inp^1; printf("packed bad      -> %d (want 0)\n", ftm_exec(blob.data,blob.len,pk,th,2));
    g_input=inp;   printf("packed wrongkey -> %d (want !=1)\n", ftm_exec(blob.data,blob.len,pk^1,th,2));
    ftm_blob_free(&blob);

    free(A); free(B); free(P1); free(P2);
    asm_free(p1); asm_free(p2); asm_free(a);
    return 0;
}
