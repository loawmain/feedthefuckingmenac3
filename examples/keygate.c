#include "../src/vm/isa.h"
#include "../src/vm/mix.h"
#include "../src/asm/asm.h"
#include "../src/rt/guard.h"
#include "../src/rt/protect.h"
#include <stdio.h>
#include <stdlib.h>

#define C1   0xDEADBEEFCAFEBABEull
#define C2   0x0123456789ABCDEFull

static uint64_t g_in, g_out;
static uint64_t th_in(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)a;(void)b;return g_in;}
static uint64_t th_out(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)b;g_out=a;return a;}
static uint64_t th_guard(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)a;(void)b;return ftm_dbg_check();}

static uint64_t xform(uint64_t v){ v=rol64(v,7); v^=C1; v+=C2; return v; }

static ftm_asm* gate(uint64_t cipher){
    ftm_asm*a=asm_new();
    a_vmcall(a,0);
    a_mov(a,1,0);
    a_sh(a,OP_ROL,1,7);
    a_imm(a,OP_XORI,1,C1);
    a_imm(a,OP_ADDI,1,C2);
    a_chkx(a,1);
    a_mov(a,3,1);
    a_imm(a,OP_ADDI,3,0x9E3779B97F4A7C15ull);
    a_mov(a,5,3); a_sh(a,OP_SHR,5,30); a_bin(a,OP_XOR,3,5);
    a_ldi(a,6,0xBF58476D1CE4E5B9ull); a_bin(a,OP_MUL,3,6);
    a_mov(a,5,3); a_sh(a,OP_SHR,5,27); a_bin(a,OP_XOR,3,5);
    a_ldi(a,6,0x94D049BB133111EBull); a_bin(a,OP_MUL,3,6);
    a_mov(a,5,3); a_sh(a,OP_SHR,5,31); a_bin(a,OP_XOR,3,5);
    a_ldi(a,4,cipher);
    a_bin(a,OP_XOR,4,3);
    a_mov(a,0,4);
    a_vmcall(a,1);
    a_halt(a);
    return a;
}

static uint64_t runraw(uint8_t*code,uint32_t len,uint64_t seed,uint64_t in,int*st){
    vm_thunk th[3]={th_in,th_out,th_guard};
    vm_ctx c; vm_seed(&c,seed);
    c.code=code; c.code_len=len; c.thunks=th; c.nthunks=3;
    g_in=in; g_out=0;
    int s=vm_run(&c); if(st)*st=s;
    return g_out;
}

int main(void){
    uint64_t correct=0xC0FFEE1234ull, wrong=correct^1ull;
    uint64_t payload=0x1337C0DE5EEDF00Dull;
    uint64_t expected=xform(correct);
    uint64_t cipher=payload^sm64(expected);

    printf("payload = 0x%016llX\n",(unsigned long long)payload);

    ftm_asm*a=gate(cipher);
    uint32_t len; uint64_t seed=0x9911AA22BBull;
    uint8_t*code=asm_build(a,seed,&len);

    int st;
    printf("raw correct -> 0x%016llX\n",(unsigned long long)runraw(code,len,seed,correct,&st));
    printf("raw wrong   -> 0x%016llX\n",(unsigned long long)runraw(code,len,seed,wrong,&st));
    code[12]^=0x40;
    printf("raw tamper  -> 0x%016llX  (status %d, no 0xBADC0DE halt)\n",(unsigned long long)runraw(code,len,seed,correct,&st),st);
    code[12]^=0x40;

    poly_cfg cfg={0};
    cfg.junk=3; cfg.opaque=4; cfg.traps=3;
    cfg.poison_reg=1; cfg.trap_thunk=2; cfg.scratch_lo=8;
    cfg.seed=0x71AA33ull;
    ftm_asm*pp=asm_polymorph(a,&cfg);

    uint64_t pk=0xB16B00B5DEADCE11ull;
    ftm_blob blob=ftm_pack(pp,seed,pk);
    vm_thunk th[3]={th_in,th_out,th_guard};

    g_in=correct; g_out=0; ftm_exec(blob.data,blob.len,pk,th,3);
    printf("packed correct  -> 0x%016llX (want payload)\n",(unsigned long long)g_out);
    g_in=wrong;   g_out=0; ftm_exec(blob.data,blob.len,pk,th,3);
    printf("packed wrong    -> 0x%016llX\n",(unsigned long long)g_out);
    g_in=correct; g_out=0; ftm_exec(blob.data,blob.len,pk^1,th,3);
    printf("packed wrongkey -> 0x%016llX\n",(unsigned long long)g_out);
    blob.data[blob.len/2]^=0xFF;
    g_in=correct; g_out=0; ftm_exec(blob.data,blob.len,pk,th,3);
    printf("packed tampered -> 0x%016llX\n",(unsigned long long)g_out);

    ftm_blob_free(&blob);
    free(code); asm_free(pp); asm_free(a);
    return 0;
}
