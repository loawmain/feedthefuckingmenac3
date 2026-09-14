#include "asm.h"
#include "../vm/isa.h"
#include "../vm/mix.h"
#include <stdlib.h>

enum{ T_REG,T_I8,T_I32,T_I64,T_REL,T_HASH };
typedef struct{ uint8_t type; uint64_t v; int label; } tok;
typedef struct{ uint8_t op; int ntok; tok toks[4]; uint32_t pos; } ins;
struct ftm_asm{ ins*v; int n,cap; int*lab; int nl,lcap; };

ftm_asm* asm_new(void){ return (ftm_asm*)calloc(1,sizeof(ftm_asm)); }
void asm_free(ftm_asm*a){ if(!a)return; free(a->v); free(a->lab); free(a); }

static ins* pushi(ftm_asm*a,uint8_t op){
    if(a->n==a->cap){ a->cap=a->cap?a->cap*2:64; a->v=(ins*)realloc(a->v,a->cap*sizeof(ins)); }
    ins*I=&a->v[a->n++]; I->op=op; I->ntok=0; return I;
}
static void tk(ins*I,uint8_t t,uint64_t v,int lab){ tok*x=&I->toks[I->ntok++]; x->type=t; x->v=v; x->label=lab; }

int asm_label(ftm_asm*a){
    if(a->nl==a->lcap){ a->lcap=a->lcap?a->lcap*2:32; a->lab=(int*)realloc(a->lab,a->lcap*sizeof(int)); }
    a->lab[a->nl]=-1; return a->nl++;
}
void asm_bind(ftm_asm*a,int l){ a->lab[l]=a->n; }

void a_ldi(ftm_asm*a,int r,uint64_t im){ ins*I=pushi(a,OP_LDI); tk(I,T_REG,(uint64_t)r,0); tk(I,T_I64,im,0); }
void a_mov(ftm_asm*a,int d,int s){ ins*I=pushi(a,OP_MOV); tk(I,T_REG,d,0); tk(I,T_REG,s,0); }
void a_ld(ftm_asm*a,int d,int s,int32_t o){ ins*I=pushi(a,OP_LD); tk(I,T_REG,d,0); tk(I,T_REG,s,0); tk(I,T_I32,(uint32_t)o,0); }
void a_st(ftm_asm*a,int d,int32_t o,int s){ ins*I=pushi(a,OP_ST); tk(I,T_REG,d,0); tk(I,T_I32,(uint32_t)o,0); tk(I,T_REG,s,0); }
void a_bin(ftm_asm*a,int op,int x,int y){ ins*I=pushi(a,(uint8_t)op); tk(I,T_REG,x,0); tk(I,T_REG,y,0); }
void a_imm(ftm_asm*a,int op,int d,uint64_t im){ ins*I=pushi(a,(uint8_t)op); tk(I,T_REG,d,0); tk(I,T_I64,im,0); }
void a_sh(ftm_asm*a,int op,int d,int n){ ins*I=pushi(a,(uint8_t)op); tk(I,T_REG,d,0); tk(I,T_I8,(uint8_t)n,0); }
void a_un(ftm_asm*a,int op,int d){ ins*I=pushi(a,(uint8_t)op); tk(I,T_REG,d,0); }
void a_cmp(ftm_asm*a,int x,int y){ ins*I=pushi(a,OP_CMP); tk(I,T_REG,x,0); tk(I,T_REG,y,0); }
void a_cmov(ftm_asm*a,int d,int s){ ins*I=pushi(a,OP_CMOV); tk(I,T_REG,d,0); tk(I,T_REG,s,0); }
void a_jmp(ftm_asm*a,int op,int l){ ins*I=pushi(a,(uint8_t)op); tk(I,T_REL,0,l); }
void a_vmcall(ftm_asm*a,int id){ ins*I=pushi(a,OP_VMCALL); tk(I,T_I8,(uint8_t)id,0); }
void a_chk(ftm_asm*a){ ins*I=pushi(a,OP_CHK); tk(I,T_HASH,0,0); }
void a_chkx(ftm_asm*a,int d){ ins*I=pushi(a,OP_CHKX); tk(I,T_REG,(uint64_t)d,0); tk(I,T_HASH,0,0); }
void a_nop(ftm_asm*a){ pushi(a,OP_NOP); }
void a_halt(ftm_asm*a){ pushi(a,OP_HALT); }

static uint32_t tsz(uint8_t t){ return t==T_REG?1u:t==T_I8?1u:t==T_I32?4u:t==T_I64?8u:t==T_REL?4u:8u; }
static uint32_t ilen(const ins*I){ uint32_t l=1; for(int i=0;i<I->ntok;i++) l+=tsz(I->toks[i].type); return l; }

uint8_t* asm_build(ftm_asm*a,uint64_t seed,uint32_t*len){
    uint32_t pc=0;
    for(int i=0;i<a->n;i++){ a->v[i].pos=pc; pc+=ilen(&a->v[i]); }
    uint32_t total=pc;
    uint8_t*e=(uint8_t*)malloc(total?total:1);

    for(int i=0;i<a->n;i++){
        ins*I=&a->v[i]; uint32_t p=I->pos; uint32_t L=ilen(I);
        uint8_t ds=dselof(seed,p);
        uint8_t rnd=(uint8_t)(sm64(seed^((uint64_t)p*2654435761u)^0xA5)>>5);
        uint8_t pop=(uint8_t)((((uint32_t)I->op+ds)&0x3F)|((rnd&3)<<6));
        e[p]=(uint8_t)(pop^ksb(seed,p)); p++;
        for(int t=0;t<I->ntok;t++){
            tok*x=&I->toks[t];
            if(x->type==T_REG){
                uint8_t r2=(uint8_t)(sm64(seed^((uint64_t)p*40503u)^0x5C)>>6);
                uint8_t pb=(uint8_t)((x->v&0xF)|((r2&0xF)<<4));
                e[p]=(uint8_t)(pb^ksb(seed,p)); p++;
            } else if(x->type==T_I8){
                e[p]=(uint8_t)((uint8_t)x->v^ksb(seed,p)); p++;
            } else if(x->type==T_I32||x->type==T_REL){
                uint32_t val;
                if(x->type==T_REL){ int ti=a->lab[x->label]; uint32_t tp=(ti>=0&&ti<a->n)?a->v[ti].pos:total; val=(uint32_t)((int32_t)(tp-(I->pos+L))); }
                else val=(uint32_t)x->v;
                for(int k=0;k<4;k++){ e[p]=(uint8_t)(((uint8_t)(val>>(8*k)))^ksb(seed,p)); p++; }
            } else if(x->type==T_I64){
                uint64_t val=x->v;
                for(int k=0;k<8;k++){ e[p]=(uint8_t)(((uint8_t)(val>>(8*k)))^ksb(seed,p)); p++; }
            } else {
                for(int k=0;k<8;k++){ e[p]=(uint8_t)(0u^ksb(seed,p)); p++; }
            }
        }
    }
    for(int i=0;i<a->n;i++){
        ins*I=&a->v[i]; uint32_t off=I->pos+1;
        for(int t=0;t<I->ntok;t++){
            if(I->toks[t].type==T_HASH){
                uint64_t h=hashcode(e,off);
                for(int k=0;k<8;k++) e[off+k]=(uint8_t)(((uint8_t)(h>>(8*k)))^ksb(seed,off+k));
                off+=8;
            } else off+=tsz(I->toks[t].type);
        }
    }
    *len=total; return e;
}

static uint64_t prng(uint64_t*s){ *s=sm64(*s); return *s; }

static void copy_ins(ftm_asm*out,const ins*I,const int*omap){
    ins*O=pushi(out,I->op);
    for(int t=0;t<I->ntok;t++){ tok x=I->toks[t]; if(x.type==T_REL) x.label=omap[x.label]; O->toks[O->ntok++]=x; }
}

static void emit_junk(ftm_asm*out,uint64_t*st,int lo){
    int span=16-lo; if(span<2){ lo=14; span=2; }
    int a=lo+(int)(prng(st)%span), b=lo+(int)(prng(st)%span);
    switch(prng(st)%7){
        case 0: a_ldi(out,a,prng(st)); break;
        case 1: a_bin(out,OP_ADD,a,b); break;
        case 2: a_bin(out,OP_XOR,a,b); break;
        case 3: a_imm(out,OP_XORI,a,prng(st)); break;
        case 4: a_sh(out,OP_ROL,a,(int)(prng(st)&63)); break;
        case 5: a_un(out,OP_NOT,a); break;
        default: a_nop(out); break;
    }
}

static void emit_opaque(ftm_asm*out,uint64_t*st,int lo){
    int span=16-lo; if(span<3){ lo=13; span=3; }
    int base=(int)(prng(st)%span);
    int x=lo+base, t=lo+((base+1)%span), z=lo+((base+2)%span);
    a_ldi(out,x,prng(st)|1);
    a_mov(out,t,x);
    if(prng(st)&1){
        a_bin(out,OP_ADD,t,x);
    } else {
        a_imm(out,OP_ADDI,t,1);
        a_bin(out,OP_MUL,t,x);
    }
    a_imm(out,OP_ANDI,t,1);
    a_ldi(out,z,0);
    int skip=asm_label(out);
    a_cmp(out,t,z);
    a_jmp(out,OP_JZ,skip);
    int j=1+(int)(prng(st)%3);
    for(int i=0;i<j;i++) emit_junk(out,st,lo);
    asm_bind(out,skip);
}

static void emit_trap(ftm_asm*out,uint64_t*st,int lo,int poison,int thunk){
    int span=16-lo; if(span<2){ lo=14; span=2; }
    int s=lo+(int)(prng(st)%span);
    a_un(out,OP_PUSH,0);
    a_vmcall(out,thunk);
    a_mov(out,s,0);
    a_un(out,OP_POP,0);
    a_bin(out,OP_XOR,poison,s);
}

ftm_asm* asm_polymorph(const ftm_asm* src,const poly_cfg* cfg){
    ftm_asm* out=asm_new();
    uint64_t st=cfg->seed?cfg->seed:0x9E3779B9u;
    int lo=cfg->scratch_lo; if(lo<1||lo>15) lo=12;
    int opb=cfg->opaque, trb=cfg->traps, ckb=cfg->chk;

    int* omap=(int*)malloc((src->nl?src->nl:1)*sizeof(int));
    for(int l=0;l<src->nl;l++) omap[l]=asm_label(out);

    int pending=0;
    for(int i=0;i<src->n;i++){
        if(!pending && opb>0 && (prng(&st)&3)==0){ emit_opaque(out,&st,lo); opb--; }
        if(trb>0 && cfg->poison_reg>=0 && (prng(&st)&7)==0){ emit_trap(out,&st,lo,cfg->poison_reg,cfg->trap_thunk); trb--; }
        if(ckb>0 && (prng(&st)%5)==0){ a_chk(out); ckb--; }
        int jc=(int)(prng(&st)%(uint64_t)(cfg->junk+1));
        for(int k=0;k<jc;k++) emit_junk(out,&st,lo);

        for(int l=0;l<src->nl;l++) if(src->lab[l]==i) asm_bind(out,omap[l]);
        const ins*I=&src->v[i];
        copy_ins(out,I,omap);
        if(I->op==OP_CMP) pending=1; else if(I->op==OP_JZ||I->op==OP_JNZ) pending=0;
    }
    for(int l=0;l<src->nl;l++) if(src->lab[l]==src->n) asm_bind(out,omap[l]);
    free(omap);
    return out;
}
